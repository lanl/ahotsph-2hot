#include <inttypes.h>

#pragma once

/* Limted to 16M elements for 32-bit ints */
typedef struct {
    unsigned int base : 24;
    unsigned int length : 8;
} segment24x8;

typedef struct {
    unsigned int base;
    unsigned int length;
} segment;
