#include "aes.h"
#include "aes_gcm.h"
#include "confirm.h"
#include "crypto.h"
#include "jitter_rng.h"
#include "kdf.h"
#include "rng.h"
#include "x25519.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(int passed, const char *test, int line)
{
    if (!passed) {
        fprintf(stderr, "FAIL at line %d: %s\n", line, test);
        exit(EXIT_FAILURE);
    }
}

#define CHECK(condition) check((condition), #condition, __LINE__)

static uint8_t decode_hex(const char *hex, uint8_t *bytes, size_t size)
{
    const char *digits = "0123456789abcdef";
    size_t length = strlen(hex);
    CHECK(length % 2 == 0 && length / 2 <= size && length / 2 <= UINT8_MAX);
    for (size_t i = 0; i < length / 2; ++i) {
        const char *high = strchr(digits, hex[2 * i]);
        const char *low = strchr(digits, hex[2 * i + 1]);
        CHECK(high && low);
        bytes[i] = (uint8_t)(((high - digits) << 4) | (low - digits));
    }
    return (uint8_t)(length / 2);
}

static void flip_bit(uint8_t *bytes, size_t bit)
{
    bytes[bit / 8] ^= (uint8_t)(1u << (bit % 8));
}

/* FIPS 197 (2001), Appendix C.1. Encrypt and decrypt independently. */
static void test_aes(void)
{
    uint8_t key[16], plain[16], cipher[16], output[16];
    CHECK(decode_hex("000102030405060708090a0b0c0d0e0f", key, sizeof key) == sizeof key);
    CHECK(decode_hex("00112233445566778899aabbccddeeff", plain, sizeof plain) == sizeof plain);
    CHECK(decode_hex("69c4e0d86a7b0430d8cdb78070b4c55a", cipher, sizeof cipher) == sizeof cipher);
    aes128_encrypt_block(key, plain, output);
    CHECK(memcmp(output, cipher, sizeof cipher) == 0);
    aes128_decrypt_block(key, cipher, output);
    CHECK(memcmp(output, plain, sizeof plain) == 0);
    puts("PASS AES-128 FIPS 197 encryption and decryption");
}

struct gcm_vector {
    const char *key, *iv, *aad, *plain, *cipher, *tag;
};

#define LONG_PLAIN "d9313225f88406e5a55909c5aff5269a8" \
                   "6a7a9531534f7da2e4c303d8a318a721" \
                   "c3c0c95956809532fcf0e2449a6b525b" \
                   "16aedf5aa0de657ba637b39"
#define LONG_CIPHER "42831ec2217774244b7221b784d0d49c" \
                    "e3aa212f2c02a4e035c17e2329aca12e" \
                    "21d514b25466931c7d8f6a5aac84aa05" \
                    "1ba30b396a0aac973d58e091"

/* McGrew/Viega GCM submission, Appendix B cases 1–4. */
static const struct gcm_vector gcm_vectors[] = {
    {"00000000000000000000000000000000", "000000000000000000000000", "",
     "", "", "58e2fccefa7e3061367f1d57a4e7455a"},
    {"00000000000000000000000000000000", "000000000000000000000000", "",
     "00000000000000000000000000000000", "0388dace60b6a392f328c2b971b2fe78",
     "ab6e47d42cec13bdf53a67b21257bddf"},
    {"feffe9928665731c6d6a8f9467308308", "cafebabefacedbaddecaf888", "",
     LONG_PLAIN "1aafd255", LONG_CIPHER "473f5985", "4d5c2af327cd64a62cf35abd2ba6fab4"},
    {"feffe9928665731c6d6a8f9467308308", "cafebabefacedbaddecaf888",
     "feedfacedeadbeeffeedfacedeadbeefabaddad2", LONG_PLAIN, LONG_CIPHER,
     "5bc94fbc3221a5db94fae95ae7121a47"},
};

static void test_gcm(const struct gcm_vector *vector, size_t number)
{
    uint8_t key[16], iv[12], aad[20], plain[64], cipher[64], tag[16];
    uint8_t output[64], output_tag[16];
    CHECK(decode_hex(vector->key, key, sizeof key) == sizeof key);
    CHECK(decode_hex(vector->iv, iv, sizeof iv) == sizeof iv);
    uint8_t aad_len = decode_hex(vector->aad, aad, sizeof aad);
    uint8_t len = decode_hex(vector->plain, plain, sizeof plain);
    CHECK(decode_hex(vector->cipher, cipher, sizeof cipher) == len);
    CHECK(decode_hex(vector->tag, tag, sizeof tag) == sizeof tag);

    CHECK(aes_gcm_encrypt(key, iv, aad, aad_len, plain, len, output, output_tag) == AES_GCM_OK);
    CHECK(memcmp(output, cipher, len) == 0);
    CHECK(memcmp(output_tag, tag, sizeof tag) == 0);
    CHECK(aes_gcm_decrypt(key, iv, aad, aad_len, cipher, len, tag, output) == AES_GCM_OK);
    CHECK(memcmp(output, plain, len) == 0);

    struct {
        uint8_t *bytes;
        size_t len;
        const char *name;
    } fields[] = {{cipher, len, "ciphertext"}, {tag, sizeof tag, "tag"}, {aad, aad_len, "AAD"}};
    size_t mutations = 0;
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; ++i) {
        for (size_t bit = 0; bit < fields[i].len * 8; ++bit) {
            flip_bit(fields[i].bytes, bit);
            memset(output, 0xa5, sizeof output);
            uint8_t status = aes_gcm_decrypt(key, iv, aad, aad_len, cipher, len, tag, output);
            if (status != AES_GCM_ERR_AUTH) {
                fprintf(stderr, "GCM case %zu accepted or misclassified %s bit %zu\n",
                        number, fields[i].name, bit);
                exit(EXIT_FAILURE);
            }
            for (size_t j = 0; j < sizeof output; ++j)
                CHECK(output[j] == 0xa5); /* No plaintext written on authentication failure. */
            flip_bit(fields[i].bytes, bit);
            ++mutations;
        }
    }
    printf("PASS AES-GCM case %zu (%zu single-bit changes rejected)\n", number, mutations);
}

static void test_packet_header(void)
{
    const uint8_t key[16] = {0x58};
    const uint8_t message[] = "LoRa test";
    const uint8_t type = PACKET_TYPE_MESSAGE;
    const uint8_t sender_id = 0x51;
    const uint8_t receiver_id = 0x82;
    uint8_t cipher[CRYPTO_MAX_DATA_LEN] = {0};
    uint8_t tag[CRYPTO_MESSAGE_TAG_SIZE], output[CRYPTO_MAX_DATA_LEN], output_len;
    uint32_t counter;

    crypto_init(sender_id, CRYPTO_ROLE_INITIATOR);
    CHECK(crypto_set_key(key, CRYPTO_ROLE_INITIATOR) == CRYPTO_OK);
    CHECK(crypto_seal(type, sender_id, message, sizeof message, &counter, cipher, tag) == CRYPTO_OK);
    CHECK(counter == 1);

    crypto_init(receiver_id, CRYPTO_ROLE_RESPONDER);
    CHECK(crypto_set_key(key, CRYPTO_ROLE_RESPONDER) == CRYPTO_OK);
    uint8_t header[CRYPTO_AAD_DATA_LEN] = {type, sender_id, 0, 0, 0, 1, sizeof message};
    for (size_t bit = 0; bit < sizeof header * 8; ++bit) {
        flip_bit(header, bit);
        uint32_t changed_counter = ((uint32_t)header[2] << 24)
                                 | ((uint32_t)header[3] << 16)
                                 | ((uint32_t)header[4] << 8)
                                 | header[5];
        CHECK(crypto_open(header[0], header[1], changed_counter, cipher, header[6],
                          tag, output, &output_len) != CRYPTO_OK);
        flip_bit(header, bit);
    }
    CHECK(crypto_open(type, sender_id, counter, cipher, sizeof message,
                      tag, output, &output_len) == CRYPTO_OK);
    CHECK(output_len == sizeof message && memcmp(output, message, sizeof message) == 0);
    puts("PASS LoRa packet header (56 single-bit changes rejected)");
}

static void test_key_exchange(void)
{
    /* RFC 7748 section 6.1, both public keys and both directions of the exchange. */
    uint8_t alice[32], bob[32], alice_public[32], bob_public[32];
    uint8_t expected[32], alice_shared[32], bob_shared[32];
    CHECK(decode_hex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a",
                     alice, sizeof alice) == sizeof alice);
    CHECK(decode_hex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb",
                     bob, sizeof bob) == sizeof bob);
    CHECK(x25519_base_point_mult(alice_public, alice) == 0);
    CHECK(decode_hex("8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a",
                     expected, sizeof expected) == sizeof expected);
    CHECK(memcmp(alice_public, expected, sizeof expected) == 0);
    CHECK(x25519_base_point_mult(bob_public, bob) == 0);
    CHECK(decode_hex("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f",
                     expected, sizeof expected) == sizeof expected);
    CHECK(memcmp(bob_public, expected, sizeof expected) == 0);
    CHECK(x25519_multscalar(alice_shared, alice, bob_public) == 0);
    CHECK(x25519_multscalar(bob_shared, bob, alice_public) == 0);
    CHECK(decode_hex("4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742",
                     expected, sizeof expected) == sizeof expected);
    CHECK(memcmp(alice_shared, expected, sizeof expected) == 0);
    CHECK(memcmp(bob_shared, expected, sizeof expected) == 0);

    const uint8_t invalid_points[][32] = {{0}, {1}};
    for (size_t i = 0; i < sizeof invalid_points / sizeof invalid_points[0]; ++i) {
        uint8_t rejected[32];
        CHECK(x25519_multscalar(rejected, alice, invalid_points[i]) != 0);
    }
    puts("PASS X25519 RFC 7748 exchange and low-order public-key rejection");

    /* Same shared secret and PSK on the two simulated peers. */
    uint8_t psk[16] = {0x34};
    uint8_t alice_key[16], bob_key[16], changed_key[16];
    kdf_session_key(psk, alice_shared, alice_key);
    kdf_session_key(psk, bob_shared, bob_key);
    CHECK(memcmp(alice_key, bob_key, sizeof alice_key) == 0);
    flip_bit(psk, 0);
    kdf_session_key(psk, bob_shared, changed_key);
    CHECK(memcmp(alice_key, changed_key, sizeof alice_key) != 0);
    flip_bit(psk, 0);
    flip_bit(bob_shared, 0);
    kdf_session_key(psk, bob_shared, changed_key);
    CHECK(memcmp(alice_key, changed_key, sizeof alice_key) != 0);
    puts("PASS KDF agreement and changes to either input");

    for (uint8_t role = 1; role <= 2; ++role) {
        uint8_t tag[16];
        CHECK(confirm_make(alice_key, role, alice_public, bob_public, tag) == 0);
        CHECK(confirm_check(bob_key, role, alice_public, bob_public, tag) == 0);
        for (size_t bit = 0; bit < sizeof tag * 8; ++bit) {
            flip_bit(tag, bit);
            CHECK(confirm_check(bob_key, role, alice_public, bob_public, tag) != 0);
            flip_bit(tag, bit);
        }
    }
    puts("PASS key confirmation (256 single-bit tag changes rejected)");
}

/* The only replacement in this executable is the hardware entropy provider. */
static uint8_t entropy_init_status;
static uint8_t entropy_fill_status;
static unsigned entropy_reads;

uint8_t jitter_rng_init(void)
{
    return entropy_init_status;
}

uint8_t jitter_rng_fill(uint8_t *out, uint8_t len)
{
    ++entropy_reads;
    /* Simulate a source that already wrote bytes before reporting failure. */
    for (uint8_t i = 0; i < len; ++i)
        out[i] = (uint8_t)(i + entropy_reads);
    return entropy_fill_status;
}

static void expect_rng_error(int status)
{
    uint8_t bytes[32];
    const uint8_t zeros[sizeof bytes] = {0};
    memset(bytes, 0xa5, sizeof bytes);
    CHECK(rng_bytes(bytes, sizeof bytes) == status);
    CHECK(memcmp(bytes, zeros, sizeof bytes) == 0);
}

static void test_random_generator(void)
{
    uint8_t samples[256][32];

    entropy_init_status = JITTER_ERR_INIT;
    expect_rng_error(RNG_ERR_SOURCE);
    expect_rng_error(RNG_ERR_FAULT);

    entropy_init_status = JITTER_OK;
    entropy_fill_status = JITTER_ERR_HEALTH;
    CHECK(rng_init() == RNG_ERR_SOURCE);
    expect_rng_error(RNG_ERR_FAULT);

    entropy_fill_status = JITTER_OK;
    CHECK(rng_init() == RNG_OK);
    unsigned reads_after_init = entropy_reads;
    for (size_t i = 0; i < sizeof samples / sizeof samples[0]; ++i) {
        CHECK(rng_bytes(samples[i], sizeof samples[i]) == RNG_OK);
        for (size_t j = 0; j < i; ++j)
            CHECK(memcmp(samples[i], samples[j], sizeof samples[i]) != 0);
    }
    CHECK(entropy_reads == reads_after_init);

    entropy_fill_status = JITTER_ERR_TIMEOUT;
    expect_rng_error(RNG_ERR_SOURCE);
    CHECK(entropy_reads == reads_after_init + 1);
    expect_rng_error(RNG_ERR_FAULT);
    CHECK(entropy_reads == reads_after_init + 1);
    puts("PASS RNG (256 distinct 32-byte outputs, startup and reseed failures)");
}

int main(void)
{
    test_aes();
    for (size_t i = 0; i < sizeof gcm_vectors / sizeof gcm_vectors[0]; ++i)
        test_gcm(&gcm_vectors[i], i + 1);
    test_packet_header();
    test_key_exchange();
    test_random_generator();
    return EXIT_SUCCESS;
}
