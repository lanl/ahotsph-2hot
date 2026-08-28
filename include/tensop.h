/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef NDIM
#error NDIM must be defined before reading this file.
#endif

#if (NDIM != 3) && (NDIM != 2)
#error NDIM must be either 2 or 3
#endif

#if (NDIM == 3)

#define TS(t, s) \
    do {         \
        t.xx s;  \
        t.yy s;  \
        t.zz s;  \
        t.xy s;  \
        t.xz s;  \
        t.yz s;  \
    } while (0)

#define TT(a, b)     \
    do {             \
        a->xx b->xx; \
        a->yy b->yy; \
        a->zz b->zz; \
        a->xy b->xy; \
        a->xz b->xz; \
        a->yz b->yz; \
    } while (0)

#endif /* NDIM == 3 */

#if (NDIM == 2)

#define TS(t, s) \
    do {         \
        t.xx s;  \
        t.yy s;  \
        t.xy s;  \
    } while (0)

#define TT(a, b)     \
    do {             \
        a->xx b->xx; \
        a->yy b->yy; \
        a->xy b->xy; \
    } while (0)

#endif /* NDIM == 2 */
