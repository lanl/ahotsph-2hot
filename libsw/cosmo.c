/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include "cosmo.h"

#include <math.h>
#include <stdlib.h>

#include "Malloc.h"
#include "Msgs.h"
#include "error.h"
#include "qromo.h"

static struct cosmo_s C;

static double adot(double a) {
    return C.H0
           * sqrt(C.Omega_m / a + C.Omega_r / (a * a) + C.Lambda * a * a
                  + (1.0 - C.Omega0 - C.Lambda));
}

static double addot(double a) { /* factor of a? */
    return C.H0 * C.H0 * (C.Lambda * a - C.Omega_r / (a * a * a) - 0.5 * C.Omega_m / (a * a));
}

static double integrand(double a) {
    double x;

    x = adot(a);
    return 1.0 / (x * x * x);
}

static double t_integrand(double a) {
    double x;

    x = adot(a);
    return 1.0 / x;
}

static double dp_integrand(double a) {
    double x;

    x = adot(a);
    return 1.0 / (a * x);
}

static double kick_integrand(double t) {
    double a;

    a = Anow(&C, t);
    return 1.0 / a;
}

static double drift_integrand(double t) {
    double a;

    a = Anow(&C, t);
    return 1.0 / (a * a);
}

double growthfac_from_Z(struct cosmo_s *c, double z) {
    double a = 1.0 / (1.0 + z);
    C = *c;
    return 2.5 * c->H0 * c->H0 * adot(a) * qromod(integrand, 0.0, a, midpntd) / a;
}

double velfac_from_Z(struct cosmo_s *c, double z) {
    double d, a_dot;
    double a = 1.0 / (1.0 + z);
    C = *c;
    d = qromod(integrand, 0.0, a, midpntd);
    a_dot = adot(a);
    return addot(a) * a / (a_dot * a_dot) - 1.0 + a / (a_dot * a_dot * a_dot * d);
}

double velfac_approx_from_Z(struct cosmo_s *c, double z) {
    double aomega;
    double a = 1.0 / (1.0 + z);
    C = *c;
    aomega = C.Omega_m + C.Omega_r / a + C.Lambda * a * a * a + (1.0 - C.Omega0 - C.Lambda) * a;
    return pow(C.Omega0 / aomega, 0.6);
}

double t_from_Z(struct cosmo_s *c, double z) {
    double d;
    double a = 1.0 / (1.0 + z);
    C = *c;
    d = qromod(t_integrand, 0.0, a, midpntd);
    return (d);
}


double dp_from_Z(struct cosmo_s *c, double z) {
    double d;
    double a = 1.0 / (1.0 + z);
    if (a == 1.0)
        return 0.0;
    C = *c;
    d = qromod(dp_integrand, a, 1.0, midpntd);
    return (d);
}

double comoving_distance_from_Z(struct cosmo_s *c, double z) {
    return speed_of_light * (one_Gyr / one_kpc) * dp_from_Z(c, z);
}

double hubble_from_Z(struct cosmo_s *c, double z) {
    double a = 1.0 / (1.0 + z);
    C = *c;
    return adot(a) / a;
}

double kick_delta(struct cosmo_s *c, double t0, double t1) {
    double d;
    C = *c;
    Msgf(("kick_delta %lf %lf\n", t0, t1));
    if (t0 == t1)
        return 0.0;
    d = qromod(kick_integrand, t0, t1, midpntd);
    return (d);
}

double drift_delta(struct cosmo_s *c, double t0, double t1) {
    double d;
    C = *c;
    Msgf(("drift_delta %lf %lf\n", t0, t1));
    if (t0 == t1)
        return 0.0;
    d = qromod(drift_integrand, t0, t1, midpntd);
    return (d);
}

double Anow(struct cosmo_s *c, double time) {
    struct cosmo_s foo;

    foo = *c;
    CosmoPush(&foo, time);
    return foo.a;
}

double Znow(struct cosmo_s *c, double time) { return 1.0 / Anow(c, time) - 1.0; }

double Hnow(struct cosmo_s *c, double time) {
    struct cosmo_s foo;

    foo = *c;
    CosmoPush(&foo, time);
    C = *c;
    return adot(foo.a) / foo.a;
}

void CosmoPush(struct cosmo_s *p, double time) {
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
    a2 = aold * aold;
    H = (H0 / aold) * sqrt(Omega_m / aold + Omega_r / a2 + Lambda * a2 + (1.0 - Omega0 - Lambda));
    nstep = (int)(800. * H * fabs(deltat)) + 1;
    Msgf(("Cosmo push %d steps, deltat=%g, H*deltat=%g\n", nstep, deltat, deltat * H));
    dt = deltat / (double)nstep;

    anew = p->a;
    for (i = 0; i < nstep; i++) {
        aold = anew;
        a2 = aold * aold;
        a3 = a2 * aold;
        H = (H0 / aold)
            * sqrt(Omega_m / aold + Omega_r / a2 + Lambda * a2 + (1.0 - Omega0 - Lambda));
        /* Follow the advice of Felten et al.  Do this to second-order */
        a2dot = H0 * H0 * (-0.5 * Omega_m / a2 - Omega_r / a3 + Lambda * aold);
        anew = aold + dt * H * aold + 0.5 * dt * dt * a2dot;
    }
    Msgf(("After push Z=%g\n", 1. / anew - 1.));
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

static double t_at_z(cosmology *c, double z) {
    struct cosmo_s *p = c->private;
    return t_from_Z(p, z);
}

static double t_at_a(cosmology *c, double a) {
    struct cosmo_s *p = c->private;
    return t_from_Z(p, 1.0 / a - 1.0);
}

static double z_at_t(cosmology *c, double t) {
    struct cosmo_s *p = c->private;
    return Znow(p, t);
}

static double a_at_t(cosmology *c, double t) {
    struct cosmo_s *p = c->private;
    return Anow(p, t);
}

static double H_at_z(cosmology *c, double z) {
    struct cosmo_s *p = c->private;
    return hubble_from_Z(p, z);
}

static double H_at_t(cosmology *c, double t) {
    struct cosmo_s *p = c->private;
    return Hnow(p, t);
}

static double conformal_distance_at_z(cosmology *c, double z) {
    struct cosmo_s *p = c->private;
    return comoving_distance_from_Z(p, z);
}

static double conformal_distance_at_t(cosmology *c, double t) {
    struct cosmo_s *p = c->private;
    double z = z_at_t(c, t);
    return comoving_distance_from_Z(p, z);
}

static double growthfac_at_z(cosmology *c, double z) {
    struct cosmo_s *p = c->private;
    return growthfac_from_Z(p, z) / growthfac_from_Z(p, 0.0);
}


static double growthfac_at_t(cosmology *c, double t) {
    struct cosmo_s *p = c->private;
    double z = z_at_t(c, t);
    return growthfac_from_Z(p, z) / growthfac_from_Z(p, 0.0);
}

static double velfac_at_z(cosmology *c, double z) {
    struct cosmo_s *p = c->private;
    return velfac_from_Z(p, z);
}


static double velfac_at_t(cosmology *c, double t) {
    struct cosmo_s *p = c->private;
    double z = z_at_t(c, t);
    return velfac_from_Z(p, z);
}

static double kick_t0_t1(cosmology *c, double t0, double t1) {
    struct cosmo_s *p = c->private;
    return kick_delta(p, t0, t1);
}

static double drift_t0_t1(cosmology *c, double t0, double t1) {
    struct cosmo_s *p = c->private;
    return drift_delta(p, t0, t1);
}

static void cosmo1_free(cosmology *c) {
    Free(c->private);
    c->private = NULL;
}

void cosmo1_init(cosmology *c) {
    struct cosmo_s *p = Calloc(1, sizeof(struct cosmo_s));

    c->private = p;
    p->t = c->t;
    if (c->a <= 0.0)
        Error("cosmo1_init bad value for a, %g\n", c->a);
    p->a = c->a;
    if (c->H0 == 0.0 && c->h_100 == 0.0)
        Error("cosmo1_init H0 and h_100 both zero\n");
    if (c->h_100 == 0.0)
        c->h_100 = 10.0 * c->H0 * (one_kpc / one_Gyr);
    if (c->h_100 > 1.0 || c->h_100 < 0.4)
        Error("cosmo1_init bad value for h_100, %g\n", c->h_100);
    if (c->H0 == 0.0)
        c->H0 = c->h_100 * 0.1 * (one_Gyr / one_kpc);
    /* definition of Mpc and Gyr differs slightly from cosmo.h to class.h */
    double ferr = fabs(c->h_100 * 0.1 * (one_Gyr / one_kpc) / c->H0 - 1.0);
    if (ferr > 2e-5)
        Error("cosmo1_init H0 and h_100 inconsistent (%g > 2e-5)\n", ferr);
    p->H0 = c->H0;
    /* cosmo1 names Omega0 incorrectly */
    p->Omega0 = c->Omega0_m;
    p->Omega_m = c->Omega0_m;
    if (c->Omega0_r != 0.0)
        Error("cosmo1_init called with nonzero Omega0_r\n");
    p->Omega_r = 0.0;
    if (c->Omega0_fld != 0.0)
        Error("cosmo1_init called with nonzero Omega0_fld\n");
    p->Omega_de = 0.0;
    if (c->w0_fld != 0.0)
        Error("cosmo1_init called with nonzero w0_fld\n");
    p->w0 = 0.0;
    if (c->wa_fld != 0.0)
        Error("cosmo1_init called with nonzero wa_fld\n");
    p->wa = 0.0;
    p->Lambda = c->Omega0_lambda;
    if (c->Gnewt == 0.0)
        p->Gnewt = c->Gnewt = GNEWT;
    else
        p->Gnewt = c->Gnewt;
    p->Zel_f = 0.0;
    if (fabs(1.0 - p->Omega_m - p->Lambda) > 1e-6)
        Error("cosmo1_init Omega0 is not 1.0\n");
    if (p->t == 0.0)
        p->t = t_at_a(c, p->a);
    else {
        double terr = fabs(1.0 - t_at_a(c, p->a) / p->t);
        if (terr > 1e-6)
            Error("cosmo1_init time and expansion inconsistent (%g > 1e-6)\n", terr);
    }

    /* Function pointers */
    c->background_at_z = NULL;
    c->background_at_t = NULL;
    c->background_at_tau = NULL;
    c->t_at_z = t_at_z;
    c->z_at_t = z_at_t;
    c->a_at_t = a_at_t;
    c->t_at_a = t_at_a;
    c->H_at_z = H_at_z;
    c->H_at_t = H_at_t;
    c->conformal_distance_at_z = conformal_distance_at_z;
    c->conformal_distance_at_t = conformal_distance_at_t;
    c->angular_diameter_distance_at_z = NULL;
    c->angular_diameter_distance_at_t = NULL;
    c->luminosity_distance_at_z = NULL;
    c->luminosity_distance_at_t = NULL;
    c->growthfac_at_z = growthfac_at_z;
    c->growthfac_at_t = growthfac_at_t;
    c->velfac_at_z = velfac_at_z;
    c->velfac_at_t = velfac_at_t;
    c->kick_t0_t1 = kick_t0_t1;
    c->drift_t0_t1 = drift_t0_t1;
    c->free = cosmo1_free;
}
