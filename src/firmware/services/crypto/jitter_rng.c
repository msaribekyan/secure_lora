#include "jitter_rng.h"
#include "stm32f0xx.h"
#include "../../Core/Inc/aes.h"     /* aes128_encrypt_block (conditioner) */

/*
 * jitter_rng.c: two-oscillator timing-jitter entropy source, STM32F042
 *
 * Two independent clocks
 * ----------------------
 *   Clock A: system clock, HSE crystal -> PLL -> 48 MHz (set up by clock_48mhz_init
 *            unless the PLL is already the system clock). Drives TIM14's counter.
 *   Clock B: LSI, the internal ~40 kHz RC oscillator. Routed to TIM14 channel 1
 *            through the RTC clock mux (RTCSEL = LSI) and TIM14_OR.TI1_RMP.
 *
 * Every rising LSI edge latches the TIM14 count into CCR1. The difference
 * between two captures is the number of 48 MHz ticks in one LSI period
 * (about 1200). The two oscillators have no phase relationship, so the low bits of
 * that difference move with LSI period jitter and wander.
 *
 * Caveat: if the LSI/HSE frequency ratio were perfectly stable, the low bits
 * would still follow a deterministic pattern (like a Bresenham line). Only the
 * real jitter perturbs it, which is why the entropy MUST be measured
 * (jitter_rng_raw + ea_non_iid) and why the assumed entropy below is low.
 *
 * Pipeline: capture -> delta -> low 2 bits -> health tests (RCT + APT)
 *           -> pack -> AES-CBC-MAC compression -> 16 output bytes.
 *
 * Startup: plausibility self-test, then SP 800-90B startup health tests
 *          (1024 samples through RCT + APT, no output).
 */

/* ---- configuration: replace the placeholders after measuring ---------- */
#define JITTER_ASSUMED_H          0.125   /* PLACEHOLDER: min-entropy, bits per 2-bit symbol   */
#define JITTER_SAMPLES_PER_BLOCK  4096u   /* 4096 x 0.125 = 512 bits in -> 128 bits out (4x)    */
#define JITTER_RCT_CUTOFF         161u    /* 1 + ceil(20 / H), alpha = 2^-20                    */
#define JITTER_APT_WINDOW         512u    /* SP 800-90B window for non-binary symbols           */
#define JITTER_APT_CUTOFF         497u    /* 1 + CRITBINOM(512, 2^-H, 1 - 2^-20)                */
#define JITTER_SYMBOL_MASK        0x03u   /* keep 2 bits per sample (packing below assumes 2)   */
#define JITTER_STARTUP_SAMPLES    1024u   /* SP 800-90B 4.3: startup test length                */

#define JITTER_CLK_SPINS          200000u /* timeout for HSE / PLL / switch waits               */
#define JITTER_PLL_MUL            RCC_CFGR_PLLMUL6   /* 8 MHz HSE x 6 = 48 MHz (Red Pill crystal) */

#define JITTER_TIMEOUT_SPINS      20000u  /* >> one LSI period (25 us ~ 200 loop passes)        */
#define JITTER_LSI_START_SPINS    100000u
#define JITTER_DELTA_MIN          400u    /* self-test window for the 48 MHz count per LSI      */
#define JITTER_DELTA_MAX          3200u   /* period (LSI 30..60 kHz is 800..1600)               */

typedef char jitter_cfg_check[((JITTER_SAMPLES_PER_BLOCK % 64u) == 0u) ? 1 : -1];

static const uint8_t k_cond_key[16] = {0};   /* fixed key for CBC-MAC compression */

/* ---- state ------------------------------------------------------------- */
static uint16_t s_prev;
static uint8_t  s_have_prev;
static uint8_t  s_ready;
static uint8_t  s_failed;            /* latched JITTER_ERR_* or 0 */
static uint8_t  s_rct_last;
static uint8_t  s_rct_cnt;
static uint8_t  s_apt_ref;
static uint16_t s_apt_cnt;
static uint16_t s_apt_pos;

static void health_reset(void)
{
    s_rct_last = 0xFFu;      /* not a valid 2-bit symbol, so the first sample starts a run */
    s_rct_cnt  = 0u;
    s_apt_pos  = 0u;
    s_apt_cnt  = 0u;
    s_apt_ref  = 0u;
}

static void wipe(volatile uint8_t *p, uint8_t n)
{
    while (n--)
        *p++ = 0u;
}

/* ---- capture: one LSI period measured in 48 MHz ticks ------------------ */
static uint8_t next_delta(uint16_t *delta)
{
    for (;;)
    {
        uint32_t spins = JITTER_TIMEOUT_SPINS;

        while ((TIM14->SR & TIM_SR_CC1IF) == 0u)
        {
            if (--spins == 0u)
            {
                s_have_prev = 0u;
                return JITTER_ERR_TIMEOUT;
            }
        }

        uint16_t now = (uint16_t)TIM14->CCR1;       /* reading CCR1 clears CC1IF */

        if ((TIM14->SR & TIM_SR_CC1OF) != 0u)       /* we missed an edge: delta would span 2 periods */
        {
            TIM14->SR = ~(uint32_t)TIM_SR_CC1OF;    /* rc_w0: writing 0 clears, 1 has no effect */
            s_have_prev = 0u;
        }

        if (s_have_prev == 0u)                      /* first capture only sets the reference */
        {
            s_prev = now;
            s_have_prev = 1u;
            continue;
        }

        *delta = (uint16_t)(now - s_prev);          /* wraps correctly at 16 bits */
        s_prev = now;
        return JITTER_OK;
    }
}

/* ---- SP 800-90B continuous health tests (4.4.1 and 4.4.2) -------------- */
static uint8_t health_feed(uint8_t sym)            /* returns 1 on failure */
{
    /* Repetition Count Test */
    if (sym == s_rct_last)
    {
        if (++s_rct_cnt >= JITTER_RCT_CUTOFF)
            return 1u;
    }
    else
    {
        s_rct_last = sym;
        s_rct_cnt  = 1u;
    }

    /* Adaptive Proportion Test */
    if (s_apt_pos == 0u)
    {
        s_apt_ref = sym;
        s_apt_cnt = 1u;
        s_apt_pos = 1u;
    }
    else
    {
        if (sym == s_apt_ref)
            s_apt_cnt++;
        if (s_apt_cnt >= JITTER_APT_CUTOFF)
            return 1u;
        if (++s_apt_pos >= JITTER_APT_WINDOW)
            s_apt_pos = 0u;
    }
    return 0u;
}

/* ---- system clock: HSE -> PLL -> 48 MHz (Clock A) ------------------------ */
/* Idempotent: if the system clock already runs from the PLL (set up by the
 * startup code or another module) nothing is touched. Returns 0 on success. */
static uint8_t clock_48mhz_init(void)
{
    uint32_t t;

    if ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL)
        return 0u;

    RCC->CR |= RCC_CR_HSEON;
    t = JITTER_CLK_SPINS;
    while ((RCC->CR & RCC_CR_HSERDY) == 0u)
        if (--t == 0u)
            return 1u;

    FLASH->ACR |= FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;   /* 1 wait state above 24 MHz */

    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL))
              | RCC_CFGR_PLLSRC_HSE_PREDIV | JITTER_PLL_MUL;

    RCC->CR |= RCC_CR_PLLON;
    t = JITTER_CLK_SPINS;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u)
        if (--t == 0u)
            return 1u;

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    t = JITTER_CLK_SPINS;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
        if (--t == 0u)
            return 1u;

    SystemCoreClockUpdate();                         /* keep CMSIS SystemCoreClock correct */
    return 0u;
}

/* ---- init --------------------------------------------------------------- */
uint8_t jitter_rng_init(void)
{
    uint32_t t;
    uint16_t d;

    s_ready = 0u;
    s_failed = 0u;
    s_have_prev = 0u;
    health_reset();

    /* clock A: 48 MHz from HSE + PLL (skipped if already running from the PLL) */
    if (clock_48mhz_init() != 0u)
        return JITTER_ERR_INIT;

    /* backup-domain write access (needed for RTCSEL) and TIM14 clock */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_TIM14EN;
    PWR->CR |= PWR_CR_DBP;

    /* clock B: LSI on */
    RCC->CSR |= RCC_CSR_LSION;
    t = JITTER_LSI_START_SPINS;
    while ((RCC->CSR & RCC_CSR_LSIRDY) == 0u)
    {
        if (--t == 0u)
        {
            PWR->CR &= ~PWR_CR_DBP;
            return JITTER_ERR_INIT;
        }
    }

    /* RTC clock = LSI. RTCSEL can only be written once until a backup-domain
     * reset, so if something else already chose a source we stop. */
    {
        uint32_t sel = RCC->BDCR & RCC_BDCR_RTCSEL;
        if (sel == 0u)
            RCC->BDCR |= RCC_BDCR_RTCSEL_1;          /* 10 = LSI */
        else if (sel != RCC_BDCR_RTCSEL_1)
        {
            PWR->CR &= ~PWR_CR_DBP;
            return JITTER_ERR_INIT;
        }
        RCC->BDCR |= RCC_BDCR_RTCEN;
    }

    /* backup-domain writes are done: close write access again.
     * (Remove this line if another module needs DBP to stay set.) */
    PWR->CR &= ~PWR_CR_DBP;

    /* TIM14: counts the 48 MHz clock, captures on every rising LSI edge */
    TIM14->CR1   = 0u;
    TIM14->OR    = TIM14_OR_TI1_RMP_0;               /* 01: TI1 <- RTC_CLK */
    TIM14->PSC   = 0u;
    TIM14->ARR   = 0xFFFFu;
    TIM14->CCMR1 = TIM_CCMR1_CC1S_0;                 /* CC1 = input on TI1, no filter, no prescaler */
    TIM14->CCER  = TIM_CCER_CC1E;                    /* rising edge, capture enabled */
    TIM14->EGR   = TIM_EGR_UG;
    TIM14->SR    = 0u;
    TIM14->CR1   = TIM_CR1_CEN;

    /* self-test 1: edges must arrive and the period must be plausible */
    for (uint8_t i = 0u; i < 32u; i++)
    {
        if (next_delta(&d) != JITTER_OK)
            return JITTER_ERR_INIT;
        if (d < JITTER_DELTA_MIN || d > JITTER_DELTA_MAX)
            return JITTER_ERR_INIT;
    }

    /* self-test 2: SP 800-90B startup health tests, 1024 samples, no output.
     * The test state carries over into normal operation. */
    health_reset();
    for (uint16_t n = 0u; n < JITTER_STARTUP_SAMPLES; n++)
    {
        if (next_delta(&d) != JITTER_OK)
            return JITTER_ERR_INIT;
        if (health_feed((uint8_t)(d & JITTER_SYMBOL_MASK)) != 0u)
            return JITTER_ERR_HEALTH;
    }

    s_ready = 1u;
    return JITTER_OK;
}

/* ---- one conditioned 16-byte block -------------------------------------- */
static uint8_t make_block(uint8_t out[16])
{
    uint8_t state[16] = {0};
    uint8_t blk[16];
    uint8_t acc = 0u, nsym = 0u, bi = 0u;

    for (uint16_t n = 0u; n < JITTER_SAMPLES_PER_BLOCK; n++)
    {
        uint16_t d;
        uint8_t r = next_delta(&d);
        if (r != JITTER_OK)
        {
            wipe(state, 16); wipe(blk, 16);
            return r;
        }

        uint8_t sym = (uint8_t)(d & JITTER_SYMBOL_MASK);
        if (health_feed(sym) != 0u)
        {
            wipe(state, 16); wipe(blk, 16);
            return JITTER_ERR_HEALTH;
        }

        acc = (uint8_t)((acc << 2) | sym);
        if (++nsym == 4u)                            /* 4 symbols -> 1 byte */
        {
            nsym = 0u;
            blk[bi++] = acc;
            acc = 0u;
            if (bi == 16u)                           /* 64 symbols -> 1 AES block */
            {
                bi = 0u;
                for (uint8_t i = 0u; i < 16u; i++)
                    state[i] ^= blk[i];
                aes128_encrypt_block(k_cond_key, state, state);   /* CBC-MAC step */
            }
        }
    }

    for (uint8_t i = 0u; i < 16u; i++)
        out[i] = state[i];

    wipe(state, 16);
    wipe(blk, 16);
    return JITTER_OK;
}

/* ---- public: conditioned random bytes ----------------------------------- */
uint8_t jitter_rng_fill(uint8_t *buf, uint8_t len)
{
    uint8_t blk[16];

    if (buf == 0 || s_ready == 0u)
        return JITTER_ERR_INIT;
    if (s_failed != 0u)
        return s_failed;

    for (uint16_t off = 0u; off < len; off += 16u)   /* uint16_t: off += 16 would wrap at 240 */
    {
        uint8_t r = make_block(blk);
        if (r != JITTER_OK)
        {
            s_failed = r;                            /* latch: FR-07, no key from a failing source */
            wipe(blk, 16);                           /* may still hold the previous block */
            for (uint16_t i = 0u; i < len; i++)
                buf[i] = 0u;
            return r;
        }
        for (uint8_t j = 0u; j < 16u && (off + j) < len; j++)
            buf[off + j] = blk[j];
    }

    wipe(blk, 16);
    return JITTER_OK;
}

/* ---- public: raw samples for offline entropy assessment ------------------ */
uint8_t jitter_rng_raw(uint8_t *out, uint16_t n)
{
    uint16_t d;

    if (out == 0 || s_ready == 0u)
        return JITTER_ERR_INIT;

    s_have_prev = 0u;                                /* start from a clean reference */
    for (uint16_t i = 0u; i < n; i++)
    {
        uint8_t r = next_delta(&d);
        if (r != JITTER_OK)
            return r;
        out[i] = (uint8_t)(d & 0xFFu);
    }
    return JITTER_OK;
}
