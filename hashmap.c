#include <stdint.h>
#include <stddef.h>
#include <netinet/ip.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "hashmap.h"

struct hashmap* hashmap_init(size_t size, struct hashmap_params *params) {
    struct hashmap* m = calloc(sizeof(struct hashmap), 1);
    m->len = size;
    m->elems = calloc(sizeof(struct hash_node *), size);
    m->obj_size = params->obj_size;
    m->hash_fun = params->hash_fun;
    m->cmp_fun = params->cmp_fun;
    return m;
}

struct hash_node* hashmap_insert(struct hashmap* map, void* elem) {
    if (hashmap_contains(map, elem)) {
        return NULL;
    }

    // allocate node
    struct hash_node *node = malloc(sizeof(struct hash_node) + map->obj_size);
    memcpy(node->elem, elem, map->obj_size);
    node->next = NULL;
    uint64_t hash = map->hash_fun(elem);
    size_t idx = hash % map->len;

    struct hash_node *top;

    // insert new node at top
    top = map->elems[idx];
    map->elems[idx] = node;
    node->next = top;
    map->n_elem++;

    // check if resizing is necessary
    if ((float) map->n_elem / map->len >= .75) {
        hashmap_resize(map);
    }
    return node;
}

void* hashmap_get(struct hashmap* map, void* key) {
    uint64_t hash = map->hash_fun(key);
    size_t idx = hash % map->len;
    struct hash_node* cur;

    cur = map->elems[idx];
    while (cur != NULL && !map->cmp_fun(key, cur->elem)) {
        cur = cur->next;
    }
    return cur == NULL? NULL : cur->elem;
}

int hashmap_remove(struct hashmap* map, void* elem) {
    long hash = map->hash_fun(elem);
    size_t idx = hash % map->len;
    struct hash_node* cur;
    struct hash_node* prev;

    cur = map->elems[idx];
    prev = NULL;

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
        free(cur); //remove comnet again
        map->n_elem--;
        return 0;
    }
    return -1;
}

// returns: 1 --> contains, 0 --> does not contain
int hashmap_contains(struct hashmap* map, void* elem) {
    long hash = map->hash_fun(elem);
    size_t idx = hash % map->len;
    struct hash_node* cur;

    cur = map->elems[idx];

    while (cur != NULL && !map->cmp_fun(elem, cur->elem)) {
        cur = cur->next;
    }
    return cur == NULL? 0: 1;
}

void hashmap_resize(struct hashmap* map) {
    hashmap_rehash(map, map->len << 1);
}

void hashmap_rehash(struct hashmap* map, size_t size) {
    // double size
    struct hash_node** elems_new = malloc(sizeof(struct hash_node *) * size);
    struct hash_node** elems_old = map->elems;
    size_t len_old = map->len;

    // update attributes
    map->elems = elems_new;
    map->len = size;

    struct hash_node *cur;
    struct hash_node *top;
    struct hash_node *next_node;

    // rehash
    for (size_t i = 0; i < len_old; i++) {
        cur = elems_old[i];
        while (cur != NULL) {

            // save next elem before modifying next pointer
            next_node = cur->next;

            // insert into map (inplace code)
            long hash = map->hash_fun(cur->elem);
            size_t idx = hash % map->len;
            top = map->elems[idx];
            map->elems[idx] = cur;
            cur->next = top;
            cur = next_node;
        }
    }
}

int hashmap_shrink(struct hashmap *map) {
    // dont shrink
    if ((float) map->n_elem / map->len >= 0.25) {
        return - 1;
    }

    // resize to ~ 0.75 load factor
    size_t size_opt = map->n_elem / 0.75;
    int i = sizeof(size_t) * 8 - 1;
    while (i >= 0 && !(size_opt & (0x1UL << i))) {
        i--;
    }

    size_t size_round = 2 << (i + 1);
    hashmap_rehash(map, size_round);
    return 0;
}
