/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

void ewald_background(
    const float xx[NDIM], float mass, double phicorr, double acc[NDIM], double *phi);
void ewald_le(const float xx[NDIM], double acc[NDIM], double *phi, float *Q, int nimage);
void calculate_cartesian_moments(body *btab, int nobj, double L, float *Q, int msb);
void cube_acc(const float m, const float *f, double a, double *acc);
void cubic_acc(const float *f, float a, float *acc);
void cubic_accd(const float *f, float a, double *accd);
void kubic_acc(const float *f, float a, float *acc);
void kubic_accd(const float *f, float a, double *accd);
