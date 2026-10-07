#include <stdio.h>
#include <stdint.h>

#define KEYLEN  16
#define SEEDLEN 32

int get_drng_support(void);
int get_entropy_input(uint8_t entropy_input[SEEDLEN]);

typedef struct {
    uint8_t Key[KEYLEN];
    uint8_t V[KEYLEN];
} ctr_drbg_state;

void ctr_drbg_instantiate(
    ctr_drbg_state *state,
    const uint8_t entropy_input[SEEDLEN]
);

void ctr_drbg_generate(
    ctr_drbg_state *state,
    uint8_t output[KEYLEN]
);

int main(void)
{
    uint8_t entropy_input[SEEDLEN];
    uint8_t encryption_key[KEYLEN];
    ctr_drbg_state state;

    // make sure the CPU supports RDSEED before using it
    if (!get_drng_support()) {
        printf("RDSEED is not supported on this CPU.\n");
        return 1;
    }

    // get entropy from RDSEED
    if (!get_entropy_input(entropy_input)) {
        printf("Failed to get entropy.\n");
        return 1;
    }

    // initialize CTR_DRBG
    ctr_drbg_instantiate(&state, entropy_input);

    // generate AES-128 encryption key
    ctr_drbg_generate(&state, encryption_key);

    // save the key
    FILE *file = fopen("aes_key.bin", "wb");

    if (file == NULL) {
        printf("Failed to create key file.\n");
        return 1;
    }

    if (fwrite(encryption_key, 1, KEYLEN, file) != KEYLEN) {
        printf("Failed to write key.\n");
        fclose(file);
        return 1;
    }

    if (fclose(file) != 0) {
        printf("Failed to close key file.\n");
        return 1;
    }

    printf("AES-128 key generated and saved.\n");

    return 0;
}