# key generation logic for aes128
# idea is that we get a trng from the laptop's hardware, feed it to a prng (ctr-drbg)
# which runs aes128 internally to generate the key 
# outputs a file not commited to git
# content of which should be used initially as the pre-shared key
# pls run with -mrdseed flag set to gcc