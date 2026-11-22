#include "kdf.h"
#include "aes.h"

void kdf_session_key(const uint8_t psk[AES_KEY_SIZE], const uint8_t z[2 * AES_BLOCK_SIZE], uint8_t key_out[AES_BLOCK_SIZE])
{
	uint8_t block[AES_BLOCK_SIZE];
	volatile uint8_t *wipe = block;

	for (uint8_t i = 0;i < AES_BLOCK_SIZE;i++)
		block[i] = z[i] ^ z[AES_BLOCK_SIZE + i];
	aes128_encrypt_block(psk, block, key_out);

	for (uint8_t i = 0;i < AES_BLOCK_SIZE;i++)
		wipe[i] = 0;
}
