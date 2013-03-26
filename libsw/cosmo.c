#include <math.h>
#include <stdlib.h>
#include "Msgs.h"
#include "qromo.h"
#include "cosmo.h"

static struct cosmo_s C;

static double
adot(double a)
{
    return C.H0*sqrt(C.Omega_m/a + C.Omega_r/(a*a) + C.Lambda*a*a + (1.0 - C.Omega0 - C.Lambda));
}

static double
addot(double a)
{				/* factor of a? */
    return C.H0 * C.H0 * (C.Lambda*a-C.Omega_r/(a*a*a)-0.5*C.Omega_m/(a*a));
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

static double
kick_integrand(double t)
{
    double a;

    a = Anow(&C, t);
    return 1.0/a;
    
}

static double
drift_integrand(double t)
{
    double a;

    a = Anow(&C, t);
    return 1.0/(a*a);
}

double 
growthfac_from_Z(struct cosmo_s *c, double z)
{
    double a = 1.0/(1.0+z);
    C = *c;
    return 2.5*c->H0*c->H0*adot(a)*qromod(integrand, 0.0, a, midpntd)/a;
}

double 
velfac_from_Z(struct cosmo_s *c, double z)
{
    double d, a_dot;
    double a = 1.0/(1.0+z);
    C = *c;
    d = qromod(integrand, 0.0, a, midpntd);
    a_dot = adot(a);
    return addot(a)
      *a/(a_dot*a_dot) - 1.0 + a/(a_dot*a_dot*a_dot*d);
}

double 
velfac_approx_from_Z(struct cosmo_s *c, double z)
{
    double aomega;
    double a = 1.0/(1.0+z);
    C = *c;
    aomega = C.Omega_m + C.Omega_r/a + C.Lambda*a*a*a + (1.0 - C.Omega0 - C.Lambda)*a;
    return pow(C.Omega0/aomega, 0.6);
}

double
t_from_Z(struct cosmo_s *c, double z)
{
    double d;
    double a = 1.0/(1.0+z);
    C = *c;
    d = qromod(t_integrand, 0.0, a, midpntd);
    return (d);
}


double
dp_from_Z(struct cosmo_s *c, double z)
{
    double d;
    double a = 1.0/(1.0+z);
    if (a == 1.0) return 0.0;
    C = *c;
    d = qromod(dp_integrand, a, 1.0, midpntd);
    return (d);
}

double
comoving_distance_from_Z(struct cosmo_s *c, double z)
{
    return speed_of_light*(one_Gyr/one_kpc)*dp_from_Z(c, z);
}

double
hubble_from_Z(struct cosmo_s *c, double z)
{
    double a = 1.0/(1.0+z);
    C = *c;
    return adot(a)/a;
}

double
kick_delta(struct cosmo_s *c, double t0, double t1)
{
    double d;
    C = *c;
    Msgf(("kick_delta %lf %lf\n", t0, t1));
    if (t0 == t1) return 0.0;
    d = qromod(kick_integrand, t0, t1, midpntd);
    return (d);
}

double
drift_delta(struct cosmo_s *c, double t0, double t1)
{
    double d;
    C = *c;
    Msgf(("drift_delta %lf %lf\n", t0, t1));
    if (t0 == t1) return 0.0;
    d = qromod(drift_integrand, t0, t1, midpntd);
    return (d);
}

double 
Anow(struct cosmo_s *c, double time)
{
    struct cosmo_s foo;

    foo = *c;
    CosmoPush(&foo, time);
    return foo.a;
}    

double
Znow(struct cosmo_s *c, double time)
{
    return 1.0/Anow(c, time) - 1.0;
}

double
Hnow(struct cosmo_s *c, double time)
{
    struct cosmo_s foo;

    foo = *c;
    CosmoPush(&foo, time);
    C = *c;
    return adot(foo.a)/foo.a;
}

void CosmoPush(struct cosmo_s *p, double time)
{
    double Omega0 = p->Omega0;
    double Omega_r = p->Omega_r;
    double Omega_m = p->Omega_m;
    double Lambda = p->Lambda;
    double H0 = p->H0;
    double H, a2, a3, aold, anew, a2dot;
    double deltat, dt;
    int i;
    int nstep;

    /* The cosmo structure holds,H0, Omega0, Lambda' = Lambda/3H0^2, a
       and t.  We integrate (forward or backward) to the new 'time' */

    deltat = time - p->t;
    if (deltat == 0.0)
	return;

    /* Felten et al do all their integrals with dt=1/(400 H0).  We can
       do the same by choosing Nstep appropriately.  In fact, we can
       do better by ensuring dt < 1/(800 H). */
    aold = p->a;
    a2 = aold*aold;
    H = (H0 / aold) * sqrt(Omega_m/aold + Omega_r/a2 + Lambda*a2 + (1.0 - Omega0 - Lambda));
    nstep = (int)(800.*H*fabs(deltat)) + 1;
    Msgf(("Cosmo push %d steps, deltat=%g, H*deltat=%g\n", 
	  nstep, deltat, deltat*H));
    dt = deltat/(double)nstep;
    
    anew = p->a;
    for (i = 0; i < nstep; i++) {
	aold = anew;
	a2 = aold*aold;
	a3 = a2*aold;
	H = (H0 / aold) * sqrt(Omega_m/aold + Omega_r/a2 + Lambda*a2 + (1.0 - Omega0 - Lambda));
	/* Follow the advice of Felten et al.  Do this to second-order */
	a2dot = H0*H0*(-0.5*Omega_m/a2 - Omega_r/a3 + Lambda*aold);
	anew = aold + dt*H*aold + 0.5*dt*dt*a2dot;
    }
    Msgf(("After push Z=%g\n", 1./anew - 1.));
    p->a = anew;
    p->t = time;
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
