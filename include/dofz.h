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
