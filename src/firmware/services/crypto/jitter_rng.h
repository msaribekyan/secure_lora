#ifndef JITTER_RNG_H
#define JITTER_RNG_H

#include <stdint.h>

/*
 * jitter_rng.h: two-oscillator timing-jitter entropy source for STM32F042
 *
 * Entropy source : TIM14 counter (48 MHz HSE/PLL) cross-sampled by LSI (~40 kHz).
 * Conditioning   : AES-CBC-MAC compression (4096 2-bit samples -> 128 bits).
 * Health tests   : NIST SP 800-90B continuous RCT and APT tests on raw symbols.
 */

#define JITTER_OK           0u
#define JITTER_ERR_INIT     1u
#define JITTER_ERR_HEALTH   2u
#define JITTER_ERR_TIMEOUT  3u

/*
 * Initialize LSI, RTC clock mux, and TIM14 capture channel.
 * Runs 32-sample delta sanity check.
 * Returns JITTER_OK on success, JITTER_ERR_INIT or JITTER_ERR_TIMEOUT on failure.
 */
uint8_t jitter_rng_init(void);

/*
 * Fill buf[0..len-1] with cryptographically conditioned random bytes.
 * Returns JITTER_OK on success; latches failure and zeros buffer on error.
 */
uint8_t jitter_rng_fill(uint8_t *buf, uint8_t len);

/*
 * Read n raw 8-bit unconditioned delta samples for offline entropy evaluation.
 * Returns JITTER_OK on success, error code otherwise.
 */
uint8_t jitter_rng_raw(uint8_t *out, uint16_t n);

#endif /* JITTER_RNG_H */