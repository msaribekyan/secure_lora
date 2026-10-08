#ifndef AES_H
#define AES_H

#define AES_BLOCK_SIZE 16
#define AES_KEY_SIZE   16

#include <stdint.h>


void aes128_encrypt_block(const uint8_t key[AES_KEY_SIZE], const uint8_t input[AES_BLOCK_SIZE], uint8_t output[AES_BLOCK_SIZE]);
void aes128_decrypt_block(const uint8_t key[AES_KEY_SIZE], const uint8_t input[AES_BLOCK_SIZE], uint8_t output[AES_BLOCK_SIZE]);

#endif // AES_H
