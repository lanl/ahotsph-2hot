/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef Randoms2DOTh
#define Randoms2DOTh

#define NTAB 32

typedef struct {
    long idum, idum2;
    long iy, iv[NTAB];
    int did_init;
    int next_norml_ok;
    float next_norml;
} ran_state;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
void ran_init(int seed, ran_state *st);
float uniform_rand(ran_state *s);
float normal_rand(ran_state *s);
float sphere_rand(ran_state *st, int ndim, float *x);
float cube_rand(ran_state *st, int ndim, float *x);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
