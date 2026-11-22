#ifndef CONFIRM_H
#define CONFIRM_H

#include <stdint.h>

int confirm_make(const uint8_t key[16], uint8_t role, const uint8_t pub_initiator[32], const uint8_t pub_responder[32], uint8_t tag[16]);

int confirm_check(const uint8_t key[16], uint8_t role, const uint8_t pub_initiator[32], const uint8_t pub_responder[32], const uint8_t tag[16]);

#endif
