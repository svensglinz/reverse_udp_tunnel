#ifndef UDP_REVERSE_MAC_H
#define UDP_REVERSE_MAC_H

#include <stdint.h>
#include <time.h>

struct mac_t {
    char check[4];
    char hash[32];
    uint64_t nonce;
};

int verify_mac(const struct mac_t* msg, const char* secret) ;
void gen_mac(struct mac_t *mac, const char *secret, int seq);

#endif
