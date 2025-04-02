#include <stdint.h>
#include <stddef.h>
#include <netinet/ip.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "hashmap.h"

hashmap_t* hashmap_init(const size_t map_size, const struct hashmap_params *params) {
    hashmap_t* m = calloc(sizeof(hashmap_t), 1);
    m->len = map_size;
    m->elems = calloc(sizeof(hashnode_t *), map_size);
    m->obj_size = params->obj_size;
    m->hash_fun = params->hash_fun;
    m->cmp_fun = params->cmp_fun;
    m->cleanup_fun = params->cleanup_fun;
    return m;
}

hashnode_t* hashmap_insert(hashmap_t* map, void* elem) {
    if (hashmap_contains(map, elem)) {
        return NULL;
    }

    // allocate node
    hashnode_t *node = malloc(sizeof(hashnode_t) + map->obj_size);
    memcpy(node->elem, elem, map->obj_size);
    node->next = NULL;

    const uint64_t hash = map->hash_fun(elem);
    const size_t idx = hash % map->len;

    // insert new node at top
    hashnode_t *top = map->elems[idx];
    map->elems[idx] = node;
    node->next = top;
    map->n_elem++;

    // check if resizing is necessary
    if ((float) map->n_elem / map->len >= .75) {
        hashmap_rehash(map, map->len << 1);
    }
    return node;
}

void* hashmap_get(const hashmap_t* map, void* key) {
    const uint64_t hash = map->hash_fun(key);
    const size_t idx = hash % map->len;

    hashnode_t *cur = map->elems[idx];
    while (cur != NULL && !map->cmp_fun(key, cur->elem)) {
        cur = cur->next;
    }
    return cur == NULL? NULL : cur->elem;
}

int hashmap_remove(hashmap_t* map, void* elem) {
    const long hash = map->hash_fun(elem);
    const size_t idx = hash % map->len;

    hashnode_t *cur = map->elems[idx];
    hashnode_t *prev = NULL;

    while (cur != NULL && !map->cmp_fun(elem, cur->elem)) {
        prev = cur;
        cur = cur->next;
    }
    // found element
    if (cur != NULL) {
        // check if removed element was first
        if (prev == NULL) {
            map->elems[idx] = cur->next;
        } else {
            prev->next = cur->next;
        }
        if (map->cleanup_fun != NULL) {
            map->cleanup_fun(elem);
        }
        free(cur);
        map->n_elem--;
        return 0;
    }
    return -1;
}

int hashmap_contains(const hashmap_t* map, void* elem) {
    const long hash = map->hash_fun(elem);
    const size_t idx = hash % map->len;

    hashnode_t *cur = map->elems[idx];

    while (cur != NULL && !map->cmp_fun(elem, cur->elem)) {
        cur = cur->next;
    }
    return cur == NULL? 0: 1;
}

void hashmap_rehash(hashmap_t* map, const size_t size) {
    hashnode_t** elems_new = malloc(sizeof(hashnode_t *) * size);
    hashnode_t** elems_old = map->elems;
    const size_t len_old = map->len;

    // update attributes
    map->elems = elems_new;
    map->len = size;

    // rehash
    for (size_t i = 0; i < len_old; i++) {
        hashnode_t *cur = elems_old[i];
        while (cur != NULL) {

            // save next elem before modifying next pointer
            hashnode_t *next = cur->next;

            const long hash = map->hash_fun(cur->elem);
            const size_t idx = hash % map->len;
            hashnode_t *top = map->elems[idx];
            map->elems[idx] = cur;
            cur->next = top;
            cur = next;
        }
    }
}

int hashmap_shrink(hashmap_t *map) {
    // dont shrink
    if ((float) map->n_elem / map->len >= 0.25) {
        return - 1;
    }

    // resize to ~ 0.75 load factor
    const size_t size_opt = map->n_elem / 0.75;
    int i = sizeof(size_t) * 8 - 1;
    while (i >= 0 && !(size_opt & (0x1UL << i))) {
        i--;
    }

    const size_t size_round = 2 << (i + 1);
    hashmap_rehash(map, size_round);
    return 0;
}

void hashmap_free(hashmap_t *map) {
    void *val;
    HASHMAP_FOREACH(map, val) {
        hashmap_remove(map, val);
    }
    free(map->elems);
    free(map);
}