#include "crypto.h"
#include "aes_gcm.h"

/*
 * crypto.c: seal / open for data packets (AES-128-GCM)
 *
 * AAD for data packets (must match the packet encoder exactly):
 *     type || id || counter (4 bytes, big-endian) || len      = 7 bytes
 * Nonce: aes_gcm_build_iv(role, counter). The two nodes use different roles
 * (initiator / responder), so their nonces never collide under one key.
 */

/* ---- compile-time checks: a header change must not silently break the logic ---- */
typedef char chk_window[(CRYPTO_REPLAY_WINDOW_SIZE == 64u) ? 1 : -1];   /* mask is 64 bits, 1ULL << diff */
typedef char chk_aad   [(CRYPTO_AAD_DATA_LEN == 7u)        ? 1 : -1];
typedef char chk_tag   [(CRYPTO_MESSAGE_TAG_SIZE == 16u)   ? 1 : -1];

/*
 * State held in RAM
 */
static uint8_t  s_key[CRYPTO_KEY_SIZE];
static uint8_t  s_key_valid = 0u;
static uint8_t  s_fault     = 0u;       /* latched: only crypto_init() clears it */

static uint8_t  s_my_id     = 0u;
static uint8_t  s_my_role   = CRYPTO_ROLE_INITIATOR;
static uint8_t  s_peer_role = CRYPTO_ROLE_RESPONDER;

static uint32_t s_tx_counter = 1u;      /* first packet of a session uses counter 1 */

/*
 * 64-packet sliding replay window for received packets
 */
static uint32_t s_rx_max_counter = 0u;
static uint64_t s_rx_window_mask = 0u;
static uint8_t  s_rx_started     = 0u;

/*
 * Compiler-retained memory wipe
 */
static void wipe(volatile uint8_t *p, uint8_t n)
{
	while (n--)
		*p++ = 0u;
}

/*
 * AAD builder for data packets:
 * format: type || id || counter(4 bytes big-endian) || len
 * Total length = 7 bytes.
 */
static void build_data_aad(uint8_t type, uint8_t id, uint32_t counter, uint8_t len, uint8_t aad[CRYPTO_AAD_DATA_LEN])
{
	aad[0] = type;
	aad[1] = id;
	aad[2] = (uint8_t)(counter >> 24);
	aad[3] = (uint8_t)(counter >> 16);
	aad[4] = (uint8_t)(counter >> 8);
	aad[5] = (uint8_t)(counter);
	aad[6] = len;
}

/* ------------------------------------------------------------------ */
/* Lifecycle & Configuration                                           */
/* ------------------------------------------------------------------ */

/* Call from INIT only: this is the one place that clears the FAULT latch. */
void crypto_init(uint8_t my_id, uint8_t my_role)
{
	s_fault     = 0u;
	s_my_id     = my_id;
	s_my_role   = my_role;
	s_peer_role = (my_role == CRYPTO_ROLE_INITIATOR) ? CRYPTO_ROLE_RESPONDER
	                                                 : CRYPTO_ROLE_INITIATOR;
	crypto_zeroize();
}

/*
 * Install a new session key together with this node's role.
 * The role comes from the key exchange ("lower public key is initiator"),
 * and the peer role is derived from it, so both nodes can never end up with
 * the same role (which would repeat nonces under one key).
 * The counters and the replay window restart with every new key.
 */
crypto_status_t crypto_set_key(const uint8_t key[CRYPTO_KEY_SIZE], uint8_t my_role)
{
	if (s_fault != 0u)
		return CRYPTO_ERR_FAULT;

	if (key == 0)
		return CRYPTO_ERR_PARAM;

	if (my_role != CRYPTO_ROLE_INITIATOR && my_role != CRYPTO_ROLE_RESPONDER)
		return CRYPTO_ERR_PARAM;

	s_my_role   = my_role;
	s_peer_role = (my_role == CRYPTO_ROLE_INITIATOR) ? CRYPTO_ROLE_RESPONDER
	                                                 : CRYPTO_ROLE_INITIATOR;

	for (uint8_t i = 0u; i < CRYPTO_KEY_SIZE; i++)
		s_key[i] = key[i];

	s_key_valid      = 1u;
	s_tx_counter     = 1u;
	s_rx_max_counter = 0u;
	s_rx_window_mask = 0u;
	s_rx_started     = 0u;

	return CRYPTO_OK;
}

/* Enter FAULT: erase the key and refuse everything until crypto_init(). */
void crypto_fault(void)
{
	wipe(s_key, CRYPTO_KEY_SIZE);
	s_key_valid      = 0u;
	s_fault          = 1u;
	s_tx_counter     = 1u;
	s_rx_max_counter = 0u;
	s_rx_window_mask = 0u;
	s_rx_started     = 0u;
}

/* Erase the key and counters. Does NOT clear the FAULT latch. */
void crypto_zeroize(void)
{
	wipe(s_key, CRYPTO_KEY_SIZE);
	s_key_valid      = 0u;
	s_tx_counter     = 1u;
	s_rx_max_counter = 0u;
	s_rx_window_mask = 0u;
	s_rx_started     = 0u;
}

bool crypto_is_ready(void)
{
	return (s_key_valid != 0u && s_fault == 0u);
}

uint32_t crypto_get_tx_counter(void)
{
	return s_tx_counter;
}

/* No setter for the TX counter on purpose: anything that can lower it can
 * repeat a nonce. It restarts at 1 only inside crypto_set_key(). */

/* ------------------------------------------------------------------ */
/* Data Packet Seal (Encrypt + Tag)                                   */
/* ------------------------------------------------------------------ */

crypto_status_t crypto_seal(
	uint8_t type,
	uint8_t id,
	const uint8_t *plaintext,
	uint8_t plaintext_len,
	uint32_t *counter_out,
	uint8_t *ciphertext_out,
	uint8_t tag_out[CRYPTO_MESSAGE_TAG_SIZE]
)
{
	uint8_t iv[12];
	uint8_t aad[CRYPTO_AAD_DATA_LEN];
	uint32_t current_counter;
	uint8_t err;

	if (s_fault != 0u)
		return CRYPTO_ERR_FAULT;

	if (s_key_valid == 0u)
		return CRYPTO_ERR_NOT_INITIALIZED;

	if (counter_out == 0 || tag_out == 0)
		return CRYPTO_ERR_PARAM;

	if (plaintext_len > 0u && (plaintext == 0 || ciphertext_out == 0))
		return CRYPTO_ERR_PARAM;

	if (plaintext_len > CRYPTO_MAX_DATA_LEN)
		return CRYPTO_ERR_PARAM;

	/* Refuse at max counter: repeating a counter in GCM destroys confidentiality and authenticity */
	if (s_tx_counter == 0xFFFFFFFFu)
		return CRYPTO_ERR_COUNTER_MAX;

	current_counter = s_tx_counter;

	/* Nonce from aes_gcm_build_iv */
	aes_gcm_build_iv(s_my_role, current_counter, iv);

	/* AAD built from header fields: type || id || counter || len */
	build_data_aad(type, id, current_counter, plaintext_len, aad);

	/* Encrypt and compute tag */
	err = aes_gcm_encrypt(s_key, iv, aad, CRYPTO_AAD_DATA_LEN, plaintext, plaintext_len, ciphertext_out, tag_out);
	if (err != AES_GCM_OK)
	{
		crypto_fault();
		return CRYPTO_ERR_FAULT;
	}

	*counter_out = current_counter;
	s_tx_counter++;                 /* advance only after a successful seal */

	return CRYPTO_OK;
}

/* ------------------------------------------------------------------ */
/* Data Packet Open (Verify + Decrypt)                                */
/* ------------------------------------------------------------------ */

crypto_status_t crypto_open(
	uint8_t type,
	uint8_t id,
	uint32_t counter,
	const uint8_t *ciphertext,
	uint8_t ciphertext_len,
	const uint8_t expected_tag[CRYPTO_MESSAGE_TAG_SIZE],
	uint8_t *plaintext_out,
	uint8_t *plaintext_len_out
)
{
	uint8_t iv[12];
	uint8_t aad[CRYPTO_AAD_DATA_LEN];
	uint8_t err;

	if (s_fault != 0u)
		return CRYPTO_ERR_FAULT;

	if (s_key_valid == 0u)
		return CRYPTO_ERR_NOT_INITIALIZED;

	if (expected_tag == 0 || plaintext_out == 0 || plaintext_len_out == 0)
		return CRYPTO_ERR_PARAM;

	if (ciphertext_len > 0u && ciphertext == 0)
		return CRYPTO_ERR_PARAM;

	if (ciphertext_len > CRYPTO_MAX_DATA_LEN)
		return CRYPTO_ERR_PARAM;

	*plaintext_len_out = 0u;        /* defined value on every error path */

	/* Reflection check: packet must not originate from ourselves */
	if (id == s_my_id)
		return CRYPTO_ERR_REFLECTION;

	/* Counter 0 is never sent by a correct node (TX starts at 1) */
	if (counter == 0u)
		return CRYPTO_ERR_REPLAY;

	/* Replay check: check window before attempting decryption (do NOT update window yet) */
	if (s_rx_started != 0u)
	{
		if (counter <= s_rx_max_counter)
		{
			uint32_t diff = s_rx_max_counter - counter;
			if (diff >= CRYPTO_REPLAY_WINDOW_SIZE)
				return CRYPTO_ERR_REPLAY; /* Too old: outside sliding window */

			if ((s_rx_window_mask & (1ULL << diff)) != 0u)
				return CRYPTO_ERR_REPLAY; /* Duplicate: already authenticated */
		}
	}

	/* Nonce from aes_gcm_build_iv using the peer's role */
	aes_gcm_build_iv(s_peer_role, counter, iv);

	/* AAD built from header fields: type || id || counter || len */
	build_data_aad(type, id, counter, ciphertext_len, aad);

	/* Authenticate tag BEFORE releasing any plaintext */
	err = aes_gcm_decrypt(s_key, iv, aad, CRYPTO_AAD_DATA_LEN, ciphertext, ciphertext_len, expected_tag, plaintext_out);
	if (err != AES_GCM_OK)
	{
		/* Authentication failed: zero output buffer, do NOT touch replay window */
		wipe(plaintext_out, ciphertext_len);
		return CRYPTO_ERR_AUTH;
	}

	/* Replay window updated ONLY after the tag verifies */
	if (s_rx_started == 0u)
	{
		s_rx_max_counter = counter;
		s_rx_window_mask = 1ULL;
		s_rx_started     = 1u;
	}
	else if (counter > s_rx_max_counter)
	{
		uint32_t shift = counter - s_rx_max_counter;
		if (shift >= CRYPTO_REPLAY_WINDOW_SIZE)
			s_rx_window_mask = 1ULL;
		else
			s_rx_window_mask = (s_rx_window_mask << shift) | 1ULL;

		s_rx_max_counter = counter;
	}
	else
	{
		uint32_t diff = s_rx_max_counter - counter;
		s_rx_window_mask |= (1ULL << diff);
	}

	*plaintext_len_out = ciphertext_len;
	return CRYPTO_OK;
}

/* ------------------------------------------------------------------ */
/* Direct Helpers for packet_message_t                                */
/* ------------------------------------------------------------------ */

crypto_status_t crypto_seal_message(
	uint8_t id,
	const uint8_t *plaintext,
	uint8_t plaintext_len,
	packet_message_t *msg_out
)
{
	crypto_status_t st;

	if (msg_out == 0)
		return CRYPTO_ERR_PARAM;

	st = crypto_seal(
		(uint8_t)PACKET_TYPE_MESSAGE,
		id,
		plaintext,
		plaintext_len,
		&msg_out->counter,
		msg_out->ciphertext,
		msg_out->tag
	);

	/* fill the header only on success, so a failed seal leaves no half-built packet */
	if (st == CRYPTO_OK)
	{
		msg_out->id = id;
		msg_out->ciphertext_length = plaintext_len;
	}
	return st;
}

crypto_status_t crypto_open_message(
	const packet_message_t *msg_in,
	uint8_t *plaintext_out,
	uint8_t *plaintext_len_out
)
{
	if (msg_in == 0)
		return CRYPTO_ERR_PARAM;

	return crypto_open(
		(uint8_t)PACKET_TYPE_MESSAGE,
		msg_in->id,
		msg_in->counter,
		msg_in->ciphertext,
		msg_in->ciphertext_length,
		msg_in->tag,
		plaintext_out,
		plaintext_len_out
	);
}
