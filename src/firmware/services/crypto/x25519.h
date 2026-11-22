#ifndef X25519_H
#define X25519_H

#include <stdint.h>

int x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]);

int x25519_base_point_mult(uint8_t out[32], const uint8_t scalar[32]);
int x25519_multscalar(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]);

#endif
