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

/**
 * @struct hashmap_params
 *
 * structure that holds configuration options
 * for the hashmap (size of elements (key, value), comparator and hash function)
 */
struct hashmap_params {
    uint16_t obj_size;
    int (*cmp_fun)(void*, void*);
    uint64_t (*hash_fun)(void*);
};

/**
 * @param[in] map_size Initial number of buckets
 * @param[in] params
 * @return a pointer to the initialized hashmap
 */
struct hashmap* hashmap_init(size_t map_size, struct hashmap_params *params);

/**
 * Checks if a key is contained in a hashmap.
 *
 * @param[in] map Pointer to the hashmap to search in.
 * @param[in] key Pointer to the key to search for.
 * @retval 1 The hashmap contains the key.
 * @retval 0 The hashmap does not contain the key.
 * @return  Undefined behavior if `map` or `key` is NULL.
 */
int hashmap_contains(struct hashmap* map, void* key);

/**
 * resize `map` to the desired size and rehash all elements
 *
 * @param[in] map map to resize/rehash
 * @param[in] size new size of the map
 */
void hashmap_rehash(struct hashmap *map, size_t size);

/**
 * insert a new element into the map
 *
 * @param[in] map map to insert element into
 * @param[in] elem new element that is inserted into the map. The map
 * copies the content of elem.
 * @return a pointer to the node where the element was inserted. This
 * pointer remains valid as long as the element is contained inside the map.
 */
struct hash_node* hashmap_insert(struct hashmap* map, void *elem);

/*
 * retreive an element from the hashmap
 */
void* hashmap_get(struct hashmap* map, void* key);

/**
 * remove an element from the hashmap
 *
 * @param[in] map map to remove element from
 * @param[in] key key of the element to be removed
 *
 * @retval 0 element was removed
 * @retval -1 element was not removed
 * @return undefined behavior if `map` or `key` is null
 */
int hashmap_remove(struct hashmap* map, void* key);

/**
 * shrinks the hashmap to a load factor of just below 0.75 if
 * the current load factor is < 0.25
 *
 * @param[in] map map to shrink
 * @retval 0 criteria for shrinking were met and map was shrunk
 * @retval -1 criteria for shrinking were not met and map was not changed
 */
int hashmap_shrink(struct hashmap *map);

#endif

