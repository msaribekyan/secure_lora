#ifndef AES_GCM_H
#define AES_GCM_H

#include <stdint.h>

#define AES_GCM_OK 0
#define AES_GCM_ERR_PARAM 1
#define AES_GCM_ERR_AUTH 2

void aes_gcm_build_iv(uint8_t role, uint32_t packet_counter, uint8_t iv[12]);

uint8_t aes_gcm_encrypt(const uint8_t aes_key[16], const uint8_t gcm_iv[12],
                        const uint8_t *aad, uint8_t aad_length,
                        const uint8_t *plaintext, uint8_t plaintext_length,
                        uint8_t *ciphertext, uint8_t tag[16]);

uint8_t aes_gcm_decrypt(const uint8_t aes_key[16], const uint8_t gcm_iv[12],
                        const uint8_t *aad, uint8_t aad_length,
                        const uint8_t *ciphertext, uint8_t ciphertext_length,
                        const uint8_t expected_tag[16], uint8_t *plaintext);

#endif // AES_GCM_H
