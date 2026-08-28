/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#error Obsolete, use cosmo.h

double growthfac_from_Z(double omega0, double h0, double lambda_prime, double z);
double velfac_from_Z(double omega0, double h0, double lambda_prime, double z);
double t_from_Z(double omega0, double h0, double lambda_prime, double z);
double dp_from_Z(double omega0, double h0, double lambda_prime, double z);
double hubble_from_Z(double omega0, double h0, double lambda_prime, double z);

b /* Radiation/Ultra-relativistic */
/* Neff = 3.04 with T_cmb = 2.726 */
/* This is Omega_r h^2 */
#define omega_r 4.1834e-5

#define one_kpc (3.08567802e16)     /* km */
#define one_Gyr (3.1558149984e16)   /* sec */
#define speed_of_light (299792.458) /* km/sec */
