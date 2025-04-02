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

struct conn_table* conn_table_init(int max_elem) {

    struct hashmap_params params = {
        .obj_size = sizeof(struct map_addr_pair),
        .cmp_fun = cmp_sockaddr_in,
        .hash_fun = hash_sockaddr_in,
        .cleanup_fun = NULL
    };

    struct hashmap_params paramsPending = {
        .obj_size = sizeof(struct sockaddr_in),
        .cmp_fun = cmp_sockaddr_in,
        .hash_fun = hash_sockaddr_in,
        .cleanup_fun = NULL
    };

    conntable_t *tbl = calloc(sizeof(struct conn_table), 1);
    hashmap_t *tunnel_to_client = hashmap_init(16, &params);
    hashmap_t *client_to_tunnel = hashmap_init(16, &params);
    hashmap_t *pending = hashmap_init(16, &paramsPending);
    tbl->tunnel_to_client = tunnel_to_client;
    tbl->client_to_tunnel = client_to_tunnel;
    tbl->pending = pending;
    tbl->has_free = 0;
    tbl->n_elem = 0;
    tbl->max_elem = max_elem;

    return tbl;
}

struct sockaddr_in* conn_table_get_client_for_tunnel(conntable_t *tbl, struct sockaddr_in *tunnel) {

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
        .hash_fun = hash_int,
        .cleanup_fun = NULL

    };
    hashmap_t *fd_to_time = hashmap_init(16, &params);
    conntable_inside_t *tbl = calloc(sizeof(struct conn_table_inside), 1);

    tbl->fd_to_time = fd_to_time;
    tbl->free_tunnel = 0;
    tbl->n_elem = 0;
    return tbl;
}

void conn_table_inside_add_fd(conntable_inside_t *tbl, int fd) {
    struct map_fd_time s = {
        .key = fd,
        .val = get_seconds()
    };
    hashmap_insert(tbl->fd_to_time, &s);
    tbl->n_elem++;
}


struct sockaddr_in* conn_table_get_tunnel_for_client(conntable_t *tbl, struct sockaddr_in *client) {

    void* elem = hashmap_get(tbl->client_to_tunnel, client);

    if (elem != NULL) {
      return &((struct map_addr_pair *)elem)->value;
    }

    // first connection. Register spare tunnel with client
    if (tbl->has_free == 0 || tbl->n_elem >= tbl->max_elem) {
      // first time device attempts connection
      if (hashmap_get(tbl->pending, client) == NULL) {
          LOG(INFO_2, "Could not establish connection for %s:%d. No free tunnel available", inet_ntoa(client->sin_addr), ntohs(client->sin_port));
          LOG(INFO_2, "This error will not be repeated");
          hashmap_insert(tbl->pending, client);
      }
      return NULL;
    }

    // register
    // remove connection from pending list
    hashmap_remove(tbl->pending, client);
    LOG(INFO_2, "allocating spare tunnel for connection %s:%d", inet_ntoa(client->sin_addr), ntohs(client->sin_port));

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
   hashnode_t *n = hashmap_insert(tbl->tunnel_to_client, &b);
   tbl->has_free = 0;
   tbl->n_elem++;

   return &((struct map_addr_pair*)n->elem)->key;
}

int conn_table_is_tunnel(conntable_t *tbl, struct sockaddr_in *addr) {

    int fst = conn_table_get_client_for_tunnel(tbl, addr) != NULL;
    int snd = tbl->free_tunnel.sin_addr.s_addr == addr->sin_addr.s_addr;
    int trd = tbl->free_tunnel.sin_port == addr->sin_port;
    return fst || (snd && trd);
}

// returns the registered tunnel
struct sockaddr_in* conn_table_register_client_with_tunnel(conntable_t *tbl, struct sockaddr_in *client) {

    // no free tunnel available
    if (tbl->has_free == 0 || tbl->n_elem >= tbl->max_elem) return NULL;

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
    hashnode_t *n = hashmap_insert(tbl->tunnel_to_client, &b);
    tbl->has_free = 0;
    tbl->n_elem++;

    return &((struct map_addr_pair*)n->elem)->key;
}

void conn_table_clean(conntable_t *tbl, time_t max_keepalive) {
    time_t time_cur = get_seconds();
    hashmap_t *map = tbl->tunnel_to_client;
    hashmap_t *map2 = tbl->client_to_tunnel;

    if (map->n_elem == 0) return;

    struct map_addr_pair* p;
    HASHMAP_FOREACH(map, p) {
         if (time_cur - p->last_activity >= max_keepalive) {
              hashmap_remove(map2, &p->value);
              hashmap_remove(map, p);
              tbl->n_elem--;
              LOG(INFO_2, "removed connection %s:%d", inet_ntoa(p->value.sin_addr), ntohs(p->value.sin_port));
              LOG(INFO_2, "Tunnel info: %d active", tbl->n_elem);
        }
    }
}

void conn_table_inside_update_last_ping(conntable_inside_t *con_tbl, int fd) {
    hashmap_t *map = con_tbl->fd_to_time;
    struct map_fd_time *e = hashmap_get(map, &fd);
    if (e != NULL) {
        e->val = get_seconds();
    }
}

void conn_table_inside_clean(conntable_inside_t *tbl, int epoll_fd, time_t max_keepalive) {

   // if map elems = 0, return immediately
    if (map->n_elem == 0) return;
    time_t time_cur = get_seconds();

    hashmap_t *map = tbl->fd_to_time;
    struct map_fd_time *s;

    // iterate over elements and remove if needed
    HASHMAP_FOREACH(map, s) {
        if (time_cur - s->val >= max_keepalive) {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, s->key, NULL);
            close(s->key);
            hashmap_remove(map, s);
            tbl->n_elem--;
            LOG(INFO_2, "closed tunnel");
            LOG(INFO_2, "Tunnel Info: %d active", tbl->n_elem);
        }
    }
}

void conn_table_update_last_ping(conntable_t *tbl, struct sockaddr_in *addr) {
    struct map_addr_pair *elem = hashmap_get(tbl->tunnel_to_client, addr);
    // null -> ping from spare connection
    if (elem != NULL) {
         elem->last_activity = get_seconds();
    }
}
