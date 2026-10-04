#include <stdint.h>
#include <string.h>

#define KEYLEN   16
#define BLOCKLEN 16
#define SEEDLEN  32

typedef struct {
    uint8_t Key[KEYLEN];
    uint8_t V[BLOCKLEN];
} ctr_drbg_state;

// initialize the ctr_drbg for its initialization function
// gets state (key 16bytes seed16bytes) entropy input(32 bytes seed)
void ctr_drbg_instantiate(ctr_drbg_state *state, const uint8_t entropy_input[SEEDLEN])
{
    // set all bytes of key and value to 0
    memset(state->Key, 0, KEYLEN);
    memset(state->V, 0, BLOCKLEN);

}