#ifndef UDP_REVERSE_INSIDE_H
#define UDP_REVERSE_INSIDE_H

#include "args.h"

int run_inside(struct args* a);
void *send_keepalive(void *args);
void *send_to_outside(void *args);
void *cleanup(void *args);

#endif
