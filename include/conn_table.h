#ifndef UDP_REVERSE_CONN_TABLE_H
#define UDP_REVERSE_CONN_TABLE_H

#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct conn_table {
    struct hashmap* tunnel_to_client;
    struct hashmap* client_to_tunnel;
    struct sockaddr_in free_tunnel;
    int n_elem;
    int has_free;
    int max_elem;
};

struct conn_table_inside {
    struct hashmap* fd_to_time;
    int free_tunnel;
    int n_elem;
};

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
void conn_table_clean(struct conn_table *tbl, time_t max_keepalive);

/**
 *
 */
void conn_table_inside_clean(struct conn_table_inside *tbl, int pollfd, time_t max_keepalive);

/**
 *
 */
void conn_table_inside_update_last_ping(struct conn_table_inside *con_tbl, int fd);

int conn_table_insert(const struct conn_table *tbl, const struct sockaddr_in * addr, int fd);

/**
 *
 */
struct sockaddr_in* conn_table_get_client_for_tunnel(struct conn_table *tbl, struct sockaddr_in *tunnel);

/**
 *
 */
struct sockaddr_in* conn_table_get_tunnel_for_client(struct conn_table *tbl, struct sockaddr_in *client);

/**
 *
 */
int conn_table_is_tunnel(struct conn_table *tbl, struct sockaddr_in *addr);

/**
 *
 */
struct sockaddr_in* conn_table_register_client_with_tunnel(struct conn_table *tbl, struct sockaddr_in *addr);

/*
 *
 */
struct sockaddr_in* conn_table_get_addr_by_fd(struct conn_table_inside *tbl, int fd);

/**
 *
 */
int conn_table_get_fd_by_addr(struct conn_table_inside *tbl, struct sockaddr_in * addr);

/**
 *
 */
void conn_table_inside_add_fd(struct conn_table_inside *tbl, int fd); // update name of function

/**
/*
 */
struct conn_table* conn_table_init(int max_elem);

/**
 *
 */
struct conn_table_inside* conn_table_inside_init();

/**
 * update time of last recepit of keepalive signal from an inside tunnel
 *
 * @param[in] tbl table to update
 * @param[in] addr address for which last ping should be updated
 */
void conn_table_update_last_ping(struct conn_table *tbl, struct sockaddr_in *addr);



#endif
