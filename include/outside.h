#ifndef UDP_REVERSE_OUTSIDE_H
#define UDP_REVERSE_OUTSIDE_H

#include "args.h"

int run_outside(const struct args* a);
// put in header outside.h!
void* forward_to_inside(const struct args* args);
void* forward_to_client(const struct args *args);
void * clean_table();

#endif
