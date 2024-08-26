/*
 * Copyright 1993 Michael S. Warren and John K. Salmon.  All Rights Reserved.
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
