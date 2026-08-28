/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

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
Key_t GetKeyFast(const void *p);
Key_t GetKeySphericalFast(const void *p);

