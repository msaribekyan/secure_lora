#include "confirm.h"
#include "aes_gcm.h"

#define CONFIRM_COUNTER 0
#define CONFIRM_AAD_LEN 65

static void confirm_build(uint8_t role, const uint8_t pub_initiator[32], const uint8_t pub_responder[32], uint8_t iv[12], uint8_t aad[CONFIRM_AAD_LEN])
{
	aes_gcm_build_iv(role, CONFIRM_COUNTER, iv);
	aad[0] = role;
	for (uint8_t i = 0;i < 32;i++)
	{
		aad[1 + i] = pub_initiator[i];
		aad[33 + i] = pub_responder[i];
	}
}

int confirm_make(const uint8_t key[16], uint8_t role, const uint8_t pub_initiator[32], const uint8_t pub_responder[32], uint8_t tag[16])
{
	uint8_t iv[12];
	uint8_t aad[CONFIRM_AAD_LEN];

	confirm_build(role, pub_initiator, pub_responder, iv, aad);
	if (aes_gcm_encrypt(key, iv, aad, CONFIRM_AAD_LEN, 0, 0, 0, tag) != AES_GCM_OK)
		return -1;
	return 0;
}

int confirm_check(const uint8_t key[16], uint8_t role, const uint8_t pub_initiator[32], const uint8_t pub_responder[32], const uint8_t tag[16])
{
	uint8_t iv[12];
	uint8_t aad[CONFIRM_AAD_LEN];

	confirm_build(role, pub_initiator, pub_responder, iv, aad);
	if (aes_gcm_decrypt(key, iv, aad, CONFIRM_AAD_LEN, 0, 0, tag, 0) != AES_GCM_OK)
		return -1;
	return 0;
}
