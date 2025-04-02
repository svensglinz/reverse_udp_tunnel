#ifndef UDP_REVERSE_CONN_TABLE_H
#define UDP_REVERSE_CONN_TABLE_H

#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "hashmap.h"

typedef struct conn_table {
    hashmap_t* tunnel_to_client;
    hashmap_t* client_to_tunnel;
    hashmap_t* pending;
    struct sockaddr_in free_tunnel;
    int n_elem;
    int has_free;
    int max_elem;
} conntable_t;

typedef struct conn_table_inside {
    hashmap_t* fd_to_time;
    int free_tunnel;
    int n_elem;
} conntable_inside_t;

struct map_fd_addr {
    int key;
    struct sockaddr_in value;
};

struct map_addr_fd {
    struct sockaddr_in key;
    int value;
};

struct map_fd_time {
    int key;
    time_t val;
};

/**
 * remove all tunnel_addr / client_addr mappings from the table
 * where no keepalive signal has beeen received from the tunnel_addr in
 * more than max_keepalive seconds
 *
 * @param[in] tbl table to clean
 * @param[in] max_keepalive max inactivity time before mapping is removed
 */
void conn_table_clean(conntable_t *tbl, time_t max_keepalive);

/**
 *
 */
void conn_table_inside_clean(conntable_inside_t *tbl, int pollfd, time_t max_keepalive);

/**
 *
 */
void conn_table_inside_update_last_ping(struct conn_table_inside *con_tbl, int fd);

int conn_table_insert(const conntable_t *tbl, const struct sockaddr_in * addr, int fd);

/**
 *
 */
struct sockaddr_in* conn_table_get_client_for_tunnel(conntable_t *tbl, struct sockaddr_in *tunnel);

/**
 *
 */
struct sockaddr_in* conn_table_get_tunnel_for_client(conntable_t *tbl, struct sockaddr_in *client);

/**
 *
 */
int conn_table_is_tunnel(conntable_t *tbl, struct sockaddr_in *addr);

/**
 *
 */
struct sockaddr_in* conn_table_register_client_with_tunnel(conntable_t *tbl, struct sockaddr_in *addr);

/*
 *
 */
struct sockaddr_in* conn_table_get_addr_by_fd(conntable_inside_t *tbl, int fd);

/**
 *
 */
int conn_table_get_fd_by_addr(conntable_inside_t *tbl, struct sockaddr_in * addr);

/**
 *
 */
void conn_table_inside_add_fd(conntable_inside_t *tbl, int fd); // update name of function

/**
 *
 */
conntable_t* conn_table_init(int max_elem);

/**
 *
 */
conntable_inside_t* conn_table_inside_init();

/**
 * update time of last recepit of keepalive signal from an inside tunnel
 *
 * @param[in] tbl table to update
 * @param[in] addr address for which last ping should be updated
 */
void conn_table_update_last_ping(conntable_t *tbl, struct sockaddr_in *addr);

#endif
