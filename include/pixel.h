/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include "key.h"
#include "timers.h"
#include "tree.h"

#define NDIM 2

typedef float value_type;

typedef struct {
    value_type value;
    Key_t key;
} pixel;

typedef struct {
    unsigned char value;
} outpixel;

typedef struct {
    float mass;
    float pos[NDIM];
    Key_t key;
} body, *bodyptr;


/* This is the descriptor that goes into the SDF header. */

#define OUTBODYDESC \
    "struct {\n\
    unsigned char value;\n\
}"

#define HAS_KEY
