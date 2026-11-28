# Secure LoRa host tests

From the repository root, run `sh tests/run-host-tests.sh` with a C compiler available as `cc` (or set `CC`). On macOS the Xcode Command Line Tools provide the compiler. There are no dependencies to install with pip and no board to flash for this suite.

| Requirement | Pass condition |
| --- | --- |
| AES-128 | Encrypts the FIPS 197 Appendix C.1 plaintext to the expected ciphertext and independently decrypts the published ciphertext. |
| AES-GCM | Matches ciphertext and tag and decrypts correctly for McGrew/Viega Appendix B cases 1–4. Each individual bit of the available ciphertext, tag and additional authenticated data (AAD) is flipped and restored. Every modified input returns an authentication error and leaves the plaintext buffer untouched. |
| LoRa packet header | The firmware packet receiver rejects all 56 single-bit changes across the type, sender ID, counter and length. It still accepts the original packet afterward. |
| X25519 | Both RFC 7748 section 6.1 public keys and both directions of the shared secret match the expected bytes. Public inputs 0 and 1 are rejected. |
| Session key derivation | Two simulated peers with the same shared secret and pre-shared key get the same session key. Changing one input at a time changes the output for these test values. |
| Key confirmation | Valid tags are accepted for both roles, and each of the 128 bit flips in each role's tag is rejected. |
| Random generator | All 256 sampled 32-byte outputs are distinct. A simulated entropy error on initialization or automatic reseed results in an error, clears the output buffer and latches the fault. |
| Entropy health | Balanced symbols pass, repeated symbols trip the Repetition Count Test, and a biased sequence with interrupted runs trips the Adaptive Proportion Test. A simulated timer capture timeout returns an error and clears the buffer. |

The suite compiles the firmware C sources directly. It simulates the two peers in one process. The RNG test replaces the hardware entropy provider with deterministic test data and injected errors. The entropy-health executable feeds deterministic symbols into the real firmware monitor and simulates a timer with no capture events. The output uniqueness check is a smoke test. Measuring the physical entropy source and demonstrating an exchange over LoRa require boards.

## Vector sources

- [FIPS 197, Appendix C.1 (2001 edition)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197.pdf)
- [McGrew/Viega GCM Appendix B cases 1–4, reproduced in NSS's vectors](https://nest-open-source.googlesource.com/nest-hello/507800056/nss/+/refs/heads/master/nss-3.26/nss/cmd/bltest/tests/aes_gcm/test_source.txt)
- [RFC 7748, Section 6.1](https://www.rfc-editor.org/rfc/rfc7748#section-6.1)
- [NIST SP 800-90B, Section 4.4](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/nist.sp.800-90b.pdf)
