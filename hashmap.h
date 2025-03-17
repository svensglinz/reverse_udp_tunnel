#ifndef UDP_REVERSE_HASHMAP_H
#define UDP_REVERSE_HASHMAP_H

#include <stddef.h>
#include <netinet/in.h>

struct hash_node {
    struct hash_node *next;
    char elem[];
};

struct hashmap {
    struct hash_node** elems;
    uint16_t obj_size;
    size_t len;
    size_t n_elem;
    uint64_t (*hash_fun)(void*); // hash function
    int (*cmp_fun)(void*, void*); // comparator
};

struct hashmap_params {
    uint16_t obj_size;
    int (*cmp_fun)(void*, void*);
    uint64_t (*hash_fun)(void*);
};

/*
 *
 */
struct hashmap* hashmap_init(size_t map_size, struct hashmap_params *params);

/*
 *
 */
int hashmap_contains(struct hashmap* map, void* key);

/*
 *
 */
void hashmap_resize(struct hashmap *map);

/*
 *
 */
void hashmap_rehash(struct hashmap *map, size_t size);

/*
 *
 */
struct hash_node* hashmap_insert(struct hashmap* map, void *elem);

/*
 *
 */
void* hashmap_get(struct hashmap* map, void* key);

/*
 *
 */
int hashmap_remove(struct hashmap* map, void* key);

/*
 *
 */
int hashmap_shrink(struct hashmap *map);

#endif

