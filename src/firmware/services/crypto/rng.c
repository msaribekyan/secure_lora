#include "rng.h"
#include "jitter_rng.h"
#include "../../Core/Inc/aes.h"   /* aes128_encrypt_block(key, in, out) */

/*
 * rng.c: AES-128 CTR_DRBG (NIST SP 800-90A Rev.1, no derivation function)
 *
 * Entropy input: jitter_rng_fill(seed, 32). That is already conditioned
 * (AES-CBC-MAC), so it is used directly as the 256-bit seed (seedlen = 256
 * for AES-128 without df).
 *
 * State: Key (16), V (16), reseed counter. About 40 bytes of RAM.
 */

#define RNG_SEEDLEN          32u
#define RNG_RESEED_INTERVAL  256u   /* generate calls between automatic reseeds */

static uint8_t  s_key[16];
static uint8_t  s_v[16];
static uint16_t s_reseed_ctr;
static uint8_t  s_inst;      /* DRBG instantiated */
static uint8_t  s_fault;     /* latched */

static void wipe(volatile uint8_t *p, uint8_t n)
{
    while (n--)
        *p++ = 0u;
}

static void v_inc(void)                          /* V = (V + 1) mod 2^128, big endian */
{
    for (int8_t i = 15; i >= 0; i--)
        if (++s_v[i] != 0u)
            break;
}

/* CTR_DRBG_Update (90A 10.2.1.2). data == NULL means all-zero provided_data. */
static void drbg_update(const uint8_t *data)
{
    uint8_t temp[RNG_SEEDLEN];

    for (uint8_t b = 0u; b < 2u; b++)
    {
        v_inc();
        aes128_encrypt_block(s_key, s_v, &temp[16u * b]);
    }
    if (data != 0)
        for (uint8_t i = 0u; i < RNG_SEEDLEN; i++)
            temp[i] ^= data[i];

    for (uint8_t i = 0u; i < 16u; i++)
    {
        s_key[i] = temp[i];
        s_v[i]   = temp[16u + i];
    }
    wipe(temp, RNG_SEEDLEN);
}

static void latch_fault(void)
{
    s_fault = 1u;
    s_inst  = 0u;
    wipe(s_key, 16u);
    wipe(s_v, 16u);
}

void rng_zeroize(void)
{
    wipe(s_key, 16u);
    wipe(s_v, 16u);
    s_inst  = 0u;
    s_fault = 1u;        /* stay dead until rng_init() is called from INIT */
}

int rng_init(void)
{
    uint8_t seed[RNG_SEEDLEN];

    s_inst  = 0u;
    s_fault = 0u;
    wipe(s_key, 16u);                            /* Key = 0, V = 0 per 90A instantiate */
    wipe(s_v, 16u);

    if (jitter_rng_init() != JITTER_OK)
    {
        latch_fault();
        return RNG_ERR_SOURCE;
    }
    if (jitter_rng_fill(seed, RNG_SEEDLEN) != JITTER_OK)
    {
        wipe(seed, RNG_SEEDLEN);
        latch_fault();
        return RNG_ERR_SOURCE;
    }

    drbg_update(seed);
    wipe(seed, RNG_SEEDLEN);
    s_reseed_ctr = 1u;
    s_inst = 1u;
    return RNG_OK;
}

int rng_reseed(void)
{
    uint8_t seed[RNG_SEEDLEN];

    if (s_fault != 0u)
        return RNG_ERR_FAULT;
    if (s_inst == 0u)
        return rng_init();

    if (jitter_rng_fill(seed, RNG_SEEDLEN) != JITTER_OK)
    {
        wipe(seed, RNG_SEEDLEN);
        latch_fault();
        return RNG_ERR_SOURCE;
    }

    drbg_update(seed);                           /* seed_material = entropy_input */
    wipe(seed, RNG_SEEDLEN);
    s_reseed_ctr = 1u;
    return RNG_OK;
}

int rng_bytes(uint8_t *out, uint16_t len)
{
    uint8_t  blk[16];
    uint16_t off = 0u;
    int      r;

    if (len == 0u)
        return RNG_OK;
    if (out == 0)
        return RNG_ERR_PARAM;

    if (s_fault != 0u)
    {
        r = RNG_ERR_FAULT;
        goto fail;
    }
    if (s_inst == 0u)                            /* lazy first use */
    {
        r = rng_init();
        if (r != RNG_OK)
            goto fail;
    }
    if (s_reseed_ctr > RNG_RESEED_INTERVAL)
    {
        r = rng_reseed();
        if (r != RNG_OK)
            goto fail;
    }

    /* Generate (90A 10.2.1.5.1). len <= 65535 B < 2^19 bits, the per-request limit. */
    while (off < len)
    {
        uint16_t n = (uint16_t)(len - off);
        if (n > 16u)
            n = 16u;
        v_inc();
        aes128_encrypt_block(s_key, s_v, blk);
        for (uint8_t j = 0u; j < n; j++)
            out[off + j] = blk[j];
        off = (uint16_t)(off + n);
    }
    wipe(blk, 16u);

    drbg_update(0);                              /* backtracking resistance */
    s_reseed_ctr++;
    return RNG_OK;

fail:
    for (uint16_t i = 0u; i < len; i++)
        out[i] = 0u;
    return r;
}