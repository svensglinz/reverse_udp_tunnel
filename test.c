#include "hashmap.h"
#include <stdio.h>

struct map_fd_time {
    int key;
    time_t value;
};

long hash_fun_fd(void* elem) {
    size_t x = ((struct map_fd_time *)elem)->key;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccd;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53;
    x ^= x >> 33;
    return x;
}

int cmp_fun_fd(void *fst, void* snd) {
    return ((struct map_fd_time *)fst)->key == ((struct map_fd_time *)snd)->key;
}

int main() {
    struct hashmap *map = hashmap_init(sizeof(struct map_fd_time), 16, hash_fun_fd, cmp_fun_fd);

    for (int i = 0; i < 100; i++) {
        printf("insertind %d\n", i);
        hashmap_insert(map, &(struct map_fd_time){.key = i, .value = i});
    }
    fflush(stdout);
    for (int i = 0; i < 10; i++) {
            printf("hashed value is %d\n", map->hash_fun(&(struct map_fd_time){.key = 1}));
        hashmap_remove(map, &(struct map_fd_time){.key = i});
    }
    printf("size is %d\n", map->n_elem);
}
