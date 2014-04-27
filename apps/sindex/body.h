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

void FixRsizeExact(const float rmin[NDIM], const float rmax[NDIM]);
void CellCorner(Key_t key, float corner[NDIM], float size[NDIM]);
Key_t GetKeyFast(const body *p);
Key_t GetKeySphericalFast(const body *p);

