#ifndef RNG_H
#define RNG_H

#include <stdint.h>

/*
 * rng.h: NIST SP 800-90A Rev.1 CTR_DRBG (AES-128, no df)
 *
 * Seeded by the physical two-oscillator jitter TRNG (jitter_rng).
 */

#define RNG_OK          0
#define RNG_ERR_PARAM   1
#define RNG_ERR_SOURCE  2
#define RNG_ERR_FAULT   3

/*
 * rng_init – instantiate the DRBG with 32 bytes from jitter_rng.
 * Returns RNG_OK on success, RNG_ERR_SOURCE on jitter failure.
 */
int  rng_init(void);

/*
 * rng_reseed – reseed the DRBG with 32 fresh bytes from jitter_rng.
 * Automatically called every 256 generate requests; can also be called manually.
 */
int  rng_reseed(void);

/*
 * rng_bytes – produce len cryptographically secure pseudo-random bytes.
 * Lazy-initializes on first call if not already initialized.
 * Provides backtracking resistance (forward secrecy) via drbg_update(0).
 * Returns RNG_OK on success; wipes output and returns error on failure.
 */
int  rng_bytes(uint8_t *out, uint16_t len);

/*
 * rng_zeroize – wipe Key, V, and state from RAM.
 */
void rng_zeroize(void);

#endif /* RNG_H */
