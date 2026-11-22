#ifndef KDF_H
#define KDF_H

#include <stdint.h>

void kdf_session_key(const uint8_t psk[16], const uint8_t z[32], uint8_t key_out[16]);

#endif
