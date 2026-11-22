#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stdbool.h>
#include "radio.h"

/*
 * Sizes defined by specification:
 * - Public key: 32 bytes (X25519)
 * - Confirmation tag: 16 bytes (AES-GCM)
 * - Message tag: 16 bytes (AES-GCM)
 * - Symmetric key: 16 bytes (AES-128)
 */
#define CRYPTO_KEY_SIZE             16u
#define CRYPTO_PUBLIC_KEY_SIZE      32u
#define CRYPTO_CONFIRM_TAG_SIZE     16u
#define CRYPTO_MESSAGE_TAG_SIZE     16u
#define CRYPTO_AAD_DATA_LEN         7u
#define CRYPTO_MAX_DATA_LEN         MAX_MESSAGE_CIPHERTEXT_SIZE
#define CRYPTO_REPLAY_WINDOW_SIZE   64u

/* Protocol roles */
#define CRYPTO_ROLE_INITIATOR       0x01u
#define CRYPTO_ROLE_RESPONDER       0x02u

typedef enum crypto_status_e
{
	CRYPTO_OK = 0x00u,
	CRYPTO_ERR_PARAM,
	CRYPTO_ERR_NOT_INITIALIZED,
	CRYPTO_ERR_FAULT,
	CRYPTO_ERR_COUNTER_MAX,
	CRYPTO_ERR_REPLAY,
	CRYPTO_ERR_REFLECTION,
	CRYPTO_ERR_AUTH,
} crypto_status_t;

/*
 * Lifecycle & configuration
 */
void crypto_init(uint8_t my_id, uint8_t my_role);
crypto_status_t crypto_set_key(const uint8_t key[CRYPTO_KEY_SIZE], uint8_t my_role);
void crypto_fault(void);
void crypto_zeroize(void);
bool crypto_is_ready(void);

uint32_t crypto_get_tx_counter(void);

/*
 * Data packet seal (encrypt + authenticate)
 *
 * Builds nonce via aes_gcm_build_iv(my_role, current_counter, iv).
 * Builds AAD: type || id || counter(4 bytes BE) || len.
 * Refuses if current counter reaches 0xFFFFFFFF.
 * Increments counter on success.
 */
crypto_status_t crypto_seal(
	uint8_t type,
	uint8_t id,
	const uint8_t *plaintext,
	uint8_t plaintext_len,
	uint32_t *counter_out,
	uint8_t *ciphertext_out,
	uint8_t tag_out[CRYPTO_MESSAGE_TAG_SIZE]
);

/*
 * Data packet open (verify + decrypt)
 *
 * Performs reflection check (id != my_id).
 * Rejects counter 0 (data TX starts at 1).
 * Checks replay window against counter.
 * Builds nonce via aes_gcm_build_iv(peer_role, counter, iv).
 * Builds AAD: type || id || counter(4 bytes BE) || len.
 * Verifies tag BEFORE decrypting.
 * Replay window is updated ONLY after the tag verifies.
 * If authentication fails, plaintext is zeroed and replay window is unchanged.
 */
crypto_status_t crypto_open(
	uint8_t type,
	uint8_t id,
	uint32_t counter,
	const uint8_t *ciphertext,
	uint8_t ciphertext_len,
	const uint8_t expected_tag[CRYPTO_MESSAGE_TAG_SIZE],
	uint8_t *plaintext_out,
	uint8_t *plaintext_len_out
);

/*
 * Direct packet helpers for packet_message_t
 */
crypto_status_t crypto_seal_message(
	uint8_t id,
	const uint8_t *plaintext,
	uint8_t plaintext_len,
	packet_message_t *msg_out
);

crypto_status_t crypto_open_message(
	const packet_message_t *msg_in,
	uint8_t *plaintext_out,
	uint8_t *plaintext_len_out
);

#endif /* CRYPTO_H */
