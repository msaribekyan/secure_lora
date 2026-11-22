#include "aes_gcm.h"
#include "../../Core/Inc/aes.h"

static void gf128_shift(uint8_t a[16]) {
  uint8_t carry = 0;
  uint8_t next_carry = 0;

  for (uint8_t i = 0; i < 16; i++) {
    next_carry = a[i] & 0x01;
    a[i] >>= 1;
    a[i] |= (carry << 7);

    carry = next_carry;
  }
}

static void gf128_add(const uint8_t a[16], const uint8_t b[16], uint8_t c[16]) {
  for (uint8_t i = 0; i < 16; i++) {
    c[i] = a[i] ^ b[i];
  }
}

static void gf128_mult(const uint8_t a[16], const uint8_t b[16],
                       uint8_t c[16]) {
  uint8_t temp_a[16];
  uint8_t z[16] = {0};

  for (uint8_t i = 0; i < 16; i++) {
    temp_a[i] = a[i];
  }
  for (uint8_t i = 0; i < 128; i++) {
    uint8_t bit = (b[i / 8] >> (7 - (i % 8))) & 0x01;
    uint8_t mask = (uint8_t)(0 - bit);
    for (uint8_t j = 0; j < 16; j++) {
      z[j] ^= temp_a[j] & mask;
    }

    uint8_t lsb = temp_a[15] & 0x01;
    gf128_shift(temp_a);
    temp_a[0] ^= (uint8_t)(0xE1 & (uint8_t)(0 - lsb));
  }
  for (uint8_t i = 0; i < 16; i++) {
    c[i] = z[i];
  }
}

static void increment_counter(uint8_t counter[4]) {
  for (int8_t i = 3; i >= 0; i--) {
    counter[i]++;

    if (counter[i] != 0)
      break;
  }
}

static void calculate_counter(const uint8_t gcm_iv[12],
                              const uint8_t counter[4], uint8_t out[16]) {
  for (uint8_t i = 0; i < 12; i++)
    out[i] = gcm_iv[i];
  for (uint8_t i = 0; i < 4; i++)
    out[12 + i] = counter[i];
}

static void secure_zero(volatile uint8_t *p, uint8_t n) {
  while (n--)
    *p++ = 0;
}

void aes_gcm_build_iv(uint8_t role, uint32_t packet_counter, uint8_t iv[12]) {
  iv[0] = role;
  for (uint8_t i = 1; i < 8; i++)
    iv[i] = 0;
  iv[8] = (uint8_t)(packet_counter >> 24);
  iv[9] = (uint8_t)(packet_counter >> 16);
  iv[10] = (uint8_t)(packet_counter >> 8);
  iv[11] = (uint8_t)(packet_counter);
}

static void ghash_update(uint8_t G[16], const uint8_t H[16],
                         const uint8_t *data, uint8_t len) {
  uint8_t blk[16];

  for (uint16_t off = 0; off < len; off += 16) {
    uint8_t n = (len - off > 16) ? 16 : (uint8_t)(len - off);
    for (uint8_t j = 0; j < 16; j++)
      blk[j] = (j < n) ? data[off + j] : 0;

    gf128_add(G, blk, G);
    gf128_mult(G, H, G);
  }
}

static void gcm_ctr_crypt(const uint8_t key[16], const uint8_t iv[12],
                          uint8_t len, const uint8_t *in, uint8_t *out) {
  uint8_t ctr[4] = {0, 0, 0, 2};
  uint8_t C[16];
  uint8_t B[16];

  for (uint16_t off = 0; off < len; off += 16) {
    calculate_counter(iv, ctr, C);
    aes128_encrypt_block(key, C, B);
    for (uint8_t j = 0; j < 16 && (off + j) < len; j++)
      out[off + j] = B[j] ^ in[off + j];
    increment_counter(ctr);
  }
  secure_zero(B, 16);
}

static void gcm_compute_tag(const uint8_t key[16], const uint8_t iv[12],
                            const uint8_t *aad, uint8_t aad_len,
                            const uint8_t *ct, uint8_t ct_len,
                            uint8_t tag[16]) {
  uint8_t ctr0[4] = {0, 0, 0, 1};
  uint8_t H[16] = {0};
  uint8_t Jin[16];
  uint8_t J[16];
  uint8_t G[16] = {0};
  uint8_t L[16] = {0};
  uint16_t aad_bits = (uint16_t)aad_len * 8;
  uint16_t ct_bits = (uint16_t)ct_len * 8;

  aes128_encrypt_block(key, H, H);
  calculate_counter(iv, ctr0, Jin);
  aes128_encrypt_block(key, Jin, J);

  ghash_update(G, H, aad, aad_len);
  ghash_update(G, H, ct, ct_len);

  L[6] = (uint8_t)(aad_bits >> 8);
  L[7] = (uint8_t)(aad_bits & 0xFF);
  L[14] = (uint8_t)(ct_bits >> 8);
  L[15] = (uint8_t)(ct_bits & 0xFF);
  gf128_add(G, L, G);
  gf128_mult(G, H, G);

  gf128_add(J, G, tag);

  secure_zero(H, 16);
  secure_zero(J, 16);
  secure_zero(G, 16);
}

uint8_t aes_gcm_encrypt(const uint8_t aes_key[16], const uint8_t gcm_iv[12],
                        const uint8_t *aad, uint8_t aad_length,
                        const uint8_t *plaintext, uint8_t plaintext_length,
                        uint8_t *ciphertext, uint8_t tag[16]) {
  if (aes_key == 0 || gcm_iv == 0 || tag == 0)
    return AES_GCM_ERR_PARAM;
  if (aad_length != 0 && aad == 0)
    return AES_GCM_ERR_PARAM;
  if (plaintext_length != 0 && (plaintext == 0 || ciphertext == 0))
    return AES_GCM_ERR_PARAM;

  gcm_ctr_crypt(aes_key, gcm_iv, plaintext_length, plaintext, ciphertext);
  gcm_compute_tag(aes_key, gcm_iv, aad, aad_length, ciphertext,
                  plaintext_length, tag);

  return AES_GCM_OK;
}

uint8_t aes_gcm_decrypt(const uint8_t aes_key[16], const uint8_t gcm_iv[12],
                        const uint8_t *aad, uint8_t aad_length,
                        const uint8_t *ciphertext, uint8_t ciphertext_length,
                        const uint8_t expected_tag[16], uint8_t *plaintext) {
  uint8_t computed_tag[16];
  uint8_t diff = 0;

  if (aes_key == 0 || gcm_iv == 0 || expected_tag == 0)
    return AES_GCM_ERR_PARAM;
  if (aad_length != 0 && aad == 0)
    return AES_GCM_ERR_PARAM;
  if (ciphertext_length != 0 && (ciphertext == 0 || plaintext == 0))
    return AES_GCM_ERR_PARAM;

  gcm_compute_tag(aes_key, gcm_iv, aad, aad_length, ciphertext,
                  ciphertext_length, computed_tag);

  for (uint8_t i = 0; i < 16; i++)
    diff |= computed_tag[i] ^ expected_tag[i];
  secure_zero(computed_tag, 16);

  if (diff != 0)
    return AES_GCM_ERR_AUTH;

  gcm_ctr_crypt(aes_key, gcm_iv, ciphertext_length, ciphertext, plaintext);

  return AES_GCM_OK;
}
