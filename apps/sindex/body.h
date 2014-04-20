#include <stdint.h>
#include "key.h"

#define NDIM 3
#define BITS_PER_DIM ((KEYBITS-1)/NDIM)

#define IDENTMASK ((1LL<<48)-1)	/* use high bits for other purposes */

typedef struct  {
    float pos[NDIM];		/* position of body */
    float vel[NDIM];		/* velocity of body */
    int64_t ident;		/* unique identifier */
} __attribute__ ((packed)) body;

float FixRsizeExact(float *rmin, float *rmax);
Key_t GetKeyFast(const body *p);

