#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <sys/epoll.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/socket.h>

#include "conn_table.h"
#include "args.h"
#include "mac.h"
#include "misc.h"
#include "outside.h"

#define BUFF_SIZE 0xffff
#define LEN_KEEPALIVE_MAC (sizeof(struct mac_t))

// global variables
static struct conn_table *conn_tbl;
static const struct args *prog_args;
static pthread_mutex_t lock;

int run_outside(const struct args* args) {

   // initialize global variables
    pthread_mutex_init(&lock, NULL);
    conn_tbl = conn_table_init(args->max_connections);
    prog_args = args;

    // create main entry socket
    int outside_sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in entry_addr;
    entry_addr.sin_family = AF_INET;
    entry_addr.sin_addr.s_addr = INADDR_ANY;
    entry_addr.sin_port = htons(args->outside_listen);

    if (bind(outside_sock, (struct sockaddr *)&entry_addr, sizeof(entry_addr)) < 0){
        LOG(ERROR, "error connecting to port %d", args->outside_listen);
        exit(EXIT_FAILURE);
    }

    LOG(INFO_1, "Connection established on port %d. Listening for incoming connections", args->outside_listen);

    // run cleanup thread
    pthread_t cleanup;
    pthread_create(&cleanup, 0, clean_table, 0);

    // listen to incoming connections
    // incoming traffic can be of two types
    // 1.) incoming client traffic, that has to be sent to the inside application via a dedicated tunnel
    // 2.) return traffic from the inside application that has to be sent back to the client
    // 3.) incoming keepalive signals from the inside client

    char buffer[BUFF_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    const struct sockaddr_in *conn;
    ssize_t bytes_recv;

    while(1) {

        // receiving from listening address
        bytes_recv = recvfrom(outside_sock, buffer, BUFF_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        //printf("received %d bytes\n", bytes_recv);
        pthread_mutex_lock(&lock);

        // check if signal is keepalive init
        if (bytes_recv == LEN_KEEPALIVE_MAC && strncmp(((struct mac_t *)buffer)->check, "KAS", 3) == 0) {
            if (!verify_mac((const struct mac_t*)buffer, args->secret)) {
                LOG(DEBUG, "received unverifiable keepalive signal from %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                // check if this is a new free tunnel

            } else if (!conn_table_is_tunnel(conn_tbl, &client_addr)) {

                // add this connection as a spare tunnel
                // only 1 free tunnel can exist at a time
                LOG(DEBUG, "registering new free tunnel %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                //conn_tbale_add_free_tunnel(conn_tbl, &client_addr);
                conn_tbl->free_tunnel = client_addr;
                conn_tbl->has_free = 1;
            } else {
                // keepalive is from a tunnel that is currently active (ie. associated with a client)
                // update last ping time
                //printf("received keepalive from from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                conn_table_update_last_ping(conn_tbl, &client_addr);
                // return 1 byte acknowledgement (check in in to not forward)
                //sendto(outside_sock, buffer, 1, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
            }
            goto unlock;
        }

        // traffic is from an established tunnel from the inside
        // send traffic back to the associated client
        if ((conn = conn_table_get_client_for_tunnel(conn_tbl, &client_addr))) {
            //printf("sending back to client\n");
            sendto(outside_sock, buffer, bytes_recv, 0, (struct sockaddr *)conn, sizeof(*conn));
            goto unlock;
        }

        // outside traffic from client and we have an established tunnel
        // forward the traffic into the tunnel
        if ((conn = conn_table_get_tunnel_for_client(conn_tbl, &client_addr))) {
            //printf("sending into tunnel %s:%d\n", inet_ntoa(conn->sin_addr), ntohs(conn->sin_port));
            sendto(outside_sock, buffer, bytes_recv, 0, (struct sockaddr *)conn, sizeof(*conn));
        }
        unlock:
        pthread_mutex_unlock(&lock);
    }
}

// periodically clean up the client<->tunnel mappings
// a mapping is removed from the table if we have missed >= two subsequent keepalive signals
// from the tunnel
// INFO: This means that a client<->tunnel mapping can continue to exist for up to (2*t_keepalive) seconds after
// the tunnel connection has been torn down by the inside client.
// If a client attempts to connect again from the same IP:Port within that period, no traffic will be processed.
void * clean_table() {

    while (1) {
    pthread_mutex_lock(&lock);
    conn_table_clean(conn_tbl, 2*prog_args->keepalive_timeout + 3);
    pthread_mutex_unlock(&lock);
    sleep(2 * prog_args->keepalive_timeout + 3);
    }
  return NULL;
}

