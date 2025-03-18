#include <string.h>
#include <openssl/evp.h>

void sha256_hash(const char *input, unsigned char out[32]) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, input, strlen(input));
    EVP_DigestFinal(ctx, out, NULL);
    EVP_MD_CTX_free(ctx);
}
