#include <stdint.h>
#include <stddef.h>
#include <netinet/ip.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <netinet/in.h>
#include "hashmap.h"
#include "conn_table.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>

#include "misc.h"

/*
 * CUSTOM COMPARATORS / HASH FUNCTIONS FOR HASHMAPS
 */

uint64_t hash_int(void* key) {
    uint64_t x = (uint64_t) *(int*)key;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccd;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53;
    x ^= x >> 33;
    return x;
}

int cmp_int(void *fst, void* snd) {
    return *(int*)fst == *(int*)snd;
}

uint64_t hash_sockaddr_in(void* key) {
   struct sockaddr_in* addr = (struct sockaddr_in*) key;

    // Combine the IP address and port for hashing
    in_addr_t h = addr->sin_addr.s_addr;
    h ^= addr->sin_port;

    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

int cmp_sockaddr_in(void *fst, void* snd) {
        struct sockaddr_in *a = (struct sockaddr_in *)fst;
        struct sockaddr_in *b = (struct sockaddr_in *)snd;
        return (a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port);
}


/*
 * CONNECTION TABLE FOR OUTSIDE CLIENT
 */

struct map_addr_pair {
    struct sockaddr_in key;
    struct sockaddr_in value;
    uint64_t last_activity;
};

struct conn_table* conn_table_init() {

    struct hashmap_params params = {
        .obj_size = sizeof(struct map_addr_pair),
        .cmp_fun = cmp_sockaddr_in,
        .hash_fun = hash_sockaddr_in
    };

    struct conn_table *tbl = malloc(sizeof(struct conn_table));
    struct hashmap *tunnel_to_client = hashmap_init(16, &params);
    struct hashmap *client_to_tunnel = hashmap_init(16, &params);
    tbl->tunnel_to_client = tunnel_to_client;
    tbl->client_to_tunnel = client_to_tunnel;
    tbl->n_elem = 0;

    return tbl;
}

struct sockaddr_in* conn_table_get_client_for_tunnel(struct conn_table *tbl, struct sockaddr_in *tunnel) {

    void* elem = hashmap_get(tbl->tunnel_to_client, tunnel);
    if (elem == NULL) return NULL;
    return &((struct map_addr_pair *)elem)->value;
}

/*
 * CONNECTION TABLE FOR INSIDE CLIENT
 */

struct conn_table_inside* conn_table_inside_init() {
    struct hashmap_params params = {
        .obj_size = sizeof(struct map_fd_time),
        .cmp_fun = cmp_int,
        .hash_fun = hash_int
    };
    struct hashmap *fd_to_time = hashmap_init(16, &params);
    struct conn_table_inside *tbl = malloc(sizeof(struct conn_table_inside));

    tbl->fd_to_time = fd_to_time;
    tbl->free_tunnel = 0;
    return tbl;
}

void conn_table_inside_add_fd(struct conn_table_inside *tbl, int fd) {
    struct map_fd_time s = {
        .key = fd,
        .val = get_seconds()
    };
    hashmap_insert(tbl->fd_to_time, &s);
}


struct sockaddr_in* conn_table_get_tunnel_for_client(struct conn_table *tbl, struct sockaddr_in *client) {

    void* elem = hashmap_get(tbl->client_to_tunnel, client);
    if (elem == NULL) return NULL;
    return &((struct map_addr_pair *)elem)->value;
}

int conn_table_is_tunnel(struct conn_table *tbl, struct sockaddr_in *addr) {
    int fst = conn_table_get_client_for_tunnel(tbl, addr) != NULL;
    int snd = memcmp(&tbl->free_tunnel, addr, sizeof(*addr)) == 0? 1 : 0;
    return fst | snd;
}

int conn_table_register_client_with_tunnel(struct conn_table *tbl, struct sockaddr_in *client) {

    // no free tunnel available
    if (tbl->has_free == 0) return -1;

    // register
    struct map_addr_pair a = {
        .key = *client,
        .value = tbl->free_tunnel,
        // last activity not needed
        // only measured for pings from inside
    };

    struct map_addr_pair b = {
        .key = tbl->free_tunnel,
        .value = *client,
        .last_activity = get_seconds()
    };

    hashmap_insert(tbl->client_to_tunnel, &a);
    hashmap_insert(tbl->tunnel_to_client, &b);
    tbl->has_free = 0;
    tbl->n_elem++;
    return 1;
}

void conn_table_clean(struct conn_table *tbl, time_t max_keepalive) {
    time_t time_cur = get_seconds();
    struct hashmap *map = tbl->tunnel_to_client;
    struct hashmap *map2 = tbl->client_to_tunnel;

    if (map->n_elem == 0) return;

    struct hash_node *next;
    for (int i = 0; i < map->len; i++) {
        struct hash_node *cur = map->elems[i];
        while (cur != NULL) {
            // never null !!!
            struct map_addr_pair* p = (struct map_addr_pair*) cur->elem;
            if (time_cur - p->last_activity >= max_keepalive) {
                next = cur->next;
                hashmap_remove(map2, &p->value);
                hashmap_remove(map, cur->elem);
                LOG(INFO_2, "removed connection %s:%d", inet_ntoa(p->value.sin_addr), ntohs(p->value.sin_port));
                cur = next;
            } else {
                cur = cur->next;
            }
        }
    }
}

void conn_table_inside_update_last_ping(struct conn_table_inside *con_tbl, int fd) {
    struct hashmap *map = con_tbl->fd_to_time;
    struct map_fd_time *e = hashmap_get(map, &fd);
    if (e != NULL) {
        e->val = get_seconds();
    }
}

// chatgpt suggests sth else bc. of concurrent removal!
void conn_table_inside_clean(struct conn_table_inside *tbl, int epoll_fd, time_t max_keepalive) {

    struct hashmap *map = tbl->fd_to_time;

    // if map elems = 0 --> return immediately
    if (map->n_elem == 0) return;
    time_t time_cur = get_seconds();
    struct hash_node *next;

    // iterate over elements and remove if needed
    for (int i = 0; i < map->len; i++) {
        struct hash_node *cur = map->elems[i];
        while (cur != NULL) {
            struct map_fd_time * s = (struct map_fd_time *)cur->elem;
            if (time_cur - s->val >= max_keepalive) {
                next = cur->next;

                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, s->key, NULL); // maybe instead of null need epoll event ?
                close(s->key);
                hashmap_remove(map, cur->elem);
                LOG(INFO_2, "closed tunnel"); // print number of active connections
                cur = next;
            } else {
                cur = cur->next;
            }
        }
    }
}

void conn_table_update_last_ping(struct conn_table *tbl, struct sockaddr_in *addr) {
    struct map_addr_pair *elem = hashmap_get(tbl->tunnel_to_client, addr);
    // null -> ping from spare connection
    if (elem != NULL) {
         elem->last_activity = get_seconds();
    }
}
