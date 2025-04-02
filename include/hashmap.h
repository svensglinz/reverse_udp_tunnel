#ifndef UDP_REVERSE_HASHMAP_H
#define UDP_REVERSE_HASHMAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
* @brief Iterator over all elements in the hashmap
*
* @details The element pointed to by `val` is safe to delete
* during the iteration (ie. hashmap_delete(map, val) can be called)
*
* @param[in] map the hashmap to iterate over
* @apram[in] val a pointer variable to hold each element during iteration
*/

#define HASHMAP_FOREACH(map, val) \
hashnode_t *_it; \
void *_next; \
for (size_t _i = 0; _i < (map)->len; _i++) \
    for (_it = (map)->elems[_i], val = (void *)_it->elem; _next = _it->next; _it) \
         for ( ; _it; _it = _next, _next = _it ? _it->next : NULL, val = (void *)_it->elem)

typedef struct hashnode {
    struct hashnode *next;
    char elem[];
} hashnode_t;

typedef struct hashmap {
    hashnode_t** elems;
    uint16_t obj_size;
    size_t len;
    size_t n_elem;
    uint64_t (*hash_fun)(void*); // hash function
    int (*cmp_fun)(void*, void*); // comparator
    void (*cleanup_fun)(void *); // destructor
} hashmap_t;

/**
 * @hashmap_t_params
 * @brief Structure that holds configuration options for the hashmap.
 *
 * @details This structure allows you to configure various aspects of the hashmap,
 * including the size of elements (key, value), comparator and hash functions,
 * and a cleanup function.
 *
 * @param obj_size Size of each element (key, value) in bytes.
 * @param cmp_fun  Function pointer to a comparator function used to compare keys.
 * The function receives pointers to the elements passed in hashmap_insert() as an argument
 * It should return 0 if the keys are not equal, and non-zero if they are.
 *
 * @param hash_fun Function pointer to a hash function used to generate hash values from keys.
 * The function receives a pointer to the element passed in hashmap_insert() as an argument
 *
 * @param cleanup_fun Function pointer to a cleanup function called before an element is removed from the hashmap.
 * The function receives a pointer to the element passed in hashmap_insert() as an argument
 * If no cleanup is required, set this to `NULL`.
 *
 * @note The `cleanup_fun` is particularly useful when storing dynamically allocated data in the hashmap.
 * It ensures that memory is properly freed before the element is removed.
 */
struct hashmap_params {
    uint16_t obj_size;
    int (*cmp_fun)(void*, void*);
    uint64_t (*hash_fun)(void*);
    void (*cleanup_fun)(void*);
};

/**
 * Initialize hashmap
 *
 * @param[in] map_size Initial number of buckets
 * @param[in] params
 * @returns a pointer to the initialized hashmap
 */
hashmap_t* hashmap_init(size_t map_size, const struct hashmap_params *params);

/**
 * Checks if a key is contained in a hashmap.
 *
 * @param[in] map Pointer to the hashmap to search in.
 * @param[in] key Pointer to the key to search for.
 * @retval 1 The hashmap contains the key.
 * @retval 0 The hashmap does not contain the key.
 * @return  Undefined behavior if `map` or `key` is NULL.
 */
int hashmap_contains(const hashmap_t* map, void* key);

/**
 * resize `map` to the desired size and rehash all elements
 *
 * @param[in] map map to resize/rehash
 * @param[in] size new size of the map
 */
void hashmap_rehash(hashmap_t *map, size_t size);

/**
 * insert a new element into the map
 *
 * @param[in] map map to insert element into
 * @param[in] elem new element that is inserted into the map. The map
 * copies the content of elem.
 * @return a pointer to the node where the element was inserted. This
 * pointer remains valid as long as the element is contained inside the map.
 */
hashnode_t* hashmap_insert(hashmap_t* map, void *elem);

/*
 * retreive an element from the hashmap
 */
void* hashmap_get(const hashmap_t* map, void* key);

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
int hashmap_remove(hashmap_t* map, void* key);

/**
 * shrinks the hashmap to a load factor of just below 0.75 if
 * the current load factor is < 0.25
 *
 * @param[in] map map to shrink
 * @retval 0 criteria for shrinking were met and map was shrunk
 * @retval -1 criteria for shrinking were not met and map was not changed
 */
int hashmap_shrink(hashmap_t *map);

/**
 * Free hashmap and return all memory
 * @param[in] map hashmap to free
 */
void hashmap_free(hashmap_t *map);

#endif

