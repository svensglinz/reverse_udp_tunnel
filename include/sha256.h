#ifndef UDP_REVERSE_SHA256_H
#define UDP_REVERSE_SHA256_H

#include <openssl/evp.h>

void sha256_hash(const char *input, unsigned char out[32]);

#endif

