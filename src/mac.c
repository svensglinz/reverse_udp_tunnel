
#include <stdint.h>
#include "sha256.h"
#include <string.h>
#include "mac.h"
#include <stdio.h>
#include <stdatomic.h>

int verify_mac(const struct mac_t* msg, const char* secret) {

    static uint64_t nonce = 0;

    // verify sequence number
    printf("msg-nonce is %d, nonce is %d\n", msg->nonce, nonce);
    if (msg->nonce < nonce) return 0;

    char hash[32];
    size_t len_secret = strlen(secret);
    char msg_verify[len_secret + sizeof(msg->nonce)];
    memcpy(msg_verify, secret, len_secret);
    memcpy(msg_verify + len_secret, &msg->nonce, sizeof(msg->nonce));
    sha256_hash(msg_verify, (unsigned char*) hash);

    /*
     * we require that the nonce of each incoming 'ping' is strictly
     * higher than the last once.This means that packages arriving
     * out of order could potentially be rejected. This is unlikely,
     * as messages are sent with a delay of >= XX ms between each other
     */

    nonce = msg->nonce;
    return memcmp(hash, msg->hash, 32) == 0 ? 1 : 0;
}

void gen_mac(struct mac_t *mac, const char *secret, int nonce) {

    size_t secret_len = strlen(secret);
    char msg[secret_len + sizeof(nonce)];

    memcpy(msg, secret, secret_len);
    memcpy(msg + secret_len, &nonce, sizeof(nonce));
    mac->nonce = nonce;
    strcpy(mac->check, "KAS");
    sha256_hash(msg, (unsigned char *)mac->hash);


}
