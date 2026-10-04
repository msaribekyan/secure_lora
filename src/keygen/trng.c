// trng generator as seed for later prng round
// uses cpu's rdseed, which exposes seed material from the hardware's entropy-conditioning path.
// for more info, refer to section 4 of https://www.intel.com/content/www/us/en/developer/articles/guide/intel-digital-random-number-generator-drng-software-implementation-guide.html

#include <stdio.h>
#include <stdint.h>
#include <cpuid.h>
#include <immintrin.h>

// utility for determining support for rdseed on 64-bit Linux

#define DRNG_NO_SUPPORT	0x0	
#define DRNG_HAS_RDSEED	0x1

int get_drng_support(void)
{
    unsigned int eax, ebx, ecx, edx;
    int drng_features = DRNG_NO_SUPPORT;

    if (__get_cpuid_max(0, NULL) < 1)
        return drng_features;

    __cpuid(1, eax, ebx, ecx, edx);

    // check if cpuid has leaf 7 since rdseed lives there
    if (__get_cpuid_max(0, NULL) >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);

        // check ebx bit 18 for rdseed
        if (ebx & (1u << 18))
            drng_features |= DRNG_HAS_RDSEED;
    }

    // return 0: none available 1: only rdseed
    return drng_features;
}

// unsigned long long is what gcc expects for rdseed64_step
int get_rdseed(unsigned long long *value)
{
    // intel recommends 10 calls
    for (int i = 0; i < 10; i++) {
        if (_rdseed64_step(value))
            return 1;
    }
    return 0;
}

// calls get_rdseed 4 times and stores in 32 byte buffer (later for ctr entropy source) 

int get_entropy_input(uint8_t entropy_input[32])
{
    unsigned long long value;

    for (int i = 0; i < 4; i++) {
        if (!get_rdseed(&value))
            return 0;

        // gets the 64 bit value, splits into 8 bytes and stores
        for (int j = 0; j < 8; j++) {
            // shift values to get last 8 bits each time, and stores in appropriate place
            entropy_input[i * 8 + j] = (uint8_t)(value >> (j * 8));
        }
    }

    return 1;
}