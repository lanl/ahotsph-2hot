#include <math.h>
#include <stdlib.h>
#include "qromo.h"
#include "dofz.h"

static double Omega0;
static double Omega_m;
static double Omega_r;
static double Omega_de;
static double w0;
static double wa;
static double Lambda_prime;
static double H0;

static double
adot(double a)
{
    return H0*sqrt(Omega_m/a + Omega_r/(a*a) + Lambda_prime*a*a + (1.0 - Omega0 - Lambda_prime));
}

static double
addot(double a)
{
    return H0 * H0 * (Lambda_prime*a-Omega_r/(a*a*a)-0.5*Omega_m/(a*a));
}

static double
integrand(double a)
{
    double x;

    x = adot(a);
    return 1.0/(x*x*x);
    
}

static double
t_integrand(double a)
{
    double x;

    x = adot(a);
    return 1.0/x;
}

static double
dp_integrand(double a)
{
    double x;

    x = adot(a);
    return 1.0/(a*x);
    
}

double 
growthfac_from_Z(double omega0, double h0, double lambda_prime, double z)
{
    double z0gf;
    double a = 1.0/(1.0+z);
    double h = 10.0*h0*(one_kpc/one_Gyr);
    Omega0 = omega0;
    Omega_r = omega_r / (h * h);
    Omega_m = omega0-Omega_r;
    H0 = h0;
    Lambda_prime = lambda_prime;
    return 2.5*H0*H0*adot(a)*qromod(integrand, 0.0, a, midpntd)/a;
}

double 
velfac_from_Z(double omega0, double h0, double lambda_prime, double z)
{
    double d, a_dot;
    double a = 1.0/(1.0+z);
    double h = 10.0*h0*(one_kpc/one_Gyr);
    Omega0 = omega0;
    Omega_r = omega_r / (h * h);
    Omega_m = omega0-Omega_r;
    H0 = h0;
    Lambda_prime = lambda_prime;
    d = qromod(integrand, 0.0, a, midpntd);
    a_dot = adot(a);
    return addot(a)
      *a/(a_dot*a_dot) - 1.0 + a/(a_dot*a_dot*a_dot*d);
}

double
t_from_Z(double omega0, double h0, double lambda_prime, double z)
{
    double d;
    double a = 1.0/(1.0+z);
    double h = 10.0*h0*(one_kpc/one_Gyr);
    Omega0 = omega0;
    Omega_r = omega_r / (h * h);
    Omega_m = omega0-Omega_r;
    H0 = h0;
    Lambda_prime = lambda_prime;
    d = qromod(t_integrand, 0.0, a, midpntd);
    return (d);
}

double
dp_from_Z(double omega0, double h0, double lambda_prime, double z)
{
    double d;
    double a = 1.0/(1.0+z);
    double h = 10.0*h0*(one_kpc/one_Gyr);
    if (a == 1.0) return 0.0;
    Omega0 = omega0;
    Omega_r = omega_r / (h * h);
    Omega_m = omega0-Omega_r;
    H0 = h0;
    Lambda_prime = lambda_prime;
    d = qromod(dp_integrand, a, 1.0, midpntd);
    return (d);
}

double
hubble_from_Z(double omega0, double h0, double lambda_prime, double z)
{
    double a = 1.0/(1.0+z);
    double h = 10.0*h0*(one_kpc/one_Gyr);
    Omega0 = omega0;
    Omega_r = omega_r / (h * h);
    Omega_m = omega0-Omega_r;
    H0 = h0;
    Lambda_prime = lambda_prime;
    return adot(a)/a;
}

#if 0
/* Crays don't have acosh */
static double Acosh(double x)
{
    return log(x + sqrt(x*x-1.0));
}

static double
growthfac_from_Z(double Omega0, double H0, double Z)
{
    /* This is just the growing mode */
    /* See Weinberg 15.9.27--15.9.31 or Peebles LSS 11.16 */
    double d, d0;

    if (Omega0 == 1.0) {
	d = 1.0/(1.0+Z);
	d0 = 1.0;
    } else if(Omega0 < 1.0) {
	/* Using doubles can cause roundoff problems near Omega0=1 */
	double psi, coshpsi;
	coshpsi = 1.0 + 2.0*(1.0 - Omega0)/(Omega0*(1.0+Z));
	psi = Acosh(coshpsi);
	d = - 3.0 * psi * sinh(psi)/((coshpsi-1.0)*(coshpsi-1.0))
	  + (5.0+coshpsi)/(coshpsi-1.0);
	coshpsi = 1.0 + 2.0*(1.0 - Omega0)/Omega0;
	psi = Acosh(coshpsi);
	d0 = - 3.0 * psi * sinh(psi)/((coshpsi-1.0)*(coshpsi-1.0))
	  + (5.0+coshpsi)/(coshpsi-1.0);
    } else {
	double theta, costheta;
	costheta = 1.0 - 2.0*(Omega0-1.0)/(Omega0*(1.0+Z));
	theta = acos(costheta);
	d = - 3.0 * theta * sin(theta)/((1.0-costheta)*(1.0-costheta))
	  + (5.0+costheta)/(1.0-costheta);
	costheta = 1.0 - 2.0*(Omega0-1.0)/Omega0;
	theta = acos(costheta);
	d0 = - 3.0 * theta * sin(theta)/((1.0-costheta)*(1.0-costheta))
	  + (5.0+costheta)/(1.0-costheta);
    }
    return d/d0;
}

static double
t_from_Z(double Omega0, double H0, double Z)
{
    double t, theta, psi;

    if(Omega0 == 1.0){
	t = (2.0/3.0) * pow(1.0+Z, -1.5);
    }else if(Omega0 < 1.0){
	psi = Acosh( 1.0 + 2.0*(1.0 - Omega0)/(Omega0*(1.0+Z)) );
	t = (Omega0/2.0)*pow(1.0-Omega0, -1.5)*(sinh(psi) - psi) ;
    }else{
	theta = acos( 1.0 - 2.0*(Omega0-1.)/(Omega0*(1.0+Z)) );
	t = (Omega0/2.0)*pow(Omega0-1.0, -1.5)*(theta-sin(theta));
    }
    t /= H0;
    return t;
}
#endif


#ifdef STANDALONE

int
main(int argc, char *argv[])
{
    double z;
    double omega0 = atof(argv[1]);
    double h0 = atof(argv[2]);
    double lp = atof(argv[3]);

    for (z = 0; z <= 100; z++) {
	printf("%g %g %g %g %g\n", z, growthfac_from_Z(omega0, h0, lp, z),
	       velfac_from_Z(omega0, h0, lp, z), t_from_Z(omega0, h0, lp, z),
	       dp_from_Z(omega0, h0, lp, z));
    }
    exit(0);
}

#endif
