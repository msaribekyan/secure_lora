# key generation logic for aes128
# idea is that we get entropy from the laptop's hardware using rdseed
# feed the entropy to a prng (ctr-drbg)
# ctr-drbg uses aes128 internally to generate the key
# output is a 16 byte aes128 key
# key is saved to aes_key.bin which isn't commited to git
# content of aes_key.bin should initially be used as the pre-shared key

# so the overall flow is the following:
# rdseed -> 32 bytes entropy -> ctr-drbg -> 16 byte aes128 key

# to compile:
# gcc -Wall -Wextra -std=c11 -mrdseed main.c trng.c ctr_drbg.c aes.c -o keygen

# note that -mrdseed should be set in the gcc command

# to run:
# ./keygen

# this creates aes_key.bin

# to view the key in hex:
# xxd -p aes_key.bin