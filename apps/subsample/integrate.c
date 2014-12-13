#include "physics.h"
#include "vop.h"
#include "cosmo.h"
#include "integrate.h"
#include "Msgs.h"

/* Strided access to each field allows this to be used for */
/* different structs.  in and out can be the same. */

void
Integrate(const float *inmass, const float *inpos, const float *invel, 
	  const float *inacc, const float *inphi, const int instride,
	  float *outpos, float *outvel, const int outstride,
	  const int n, const double dt, const double dtv,
	  double *tpos, double *tvel, 
	  double *kep, double *pep)
{
    const float *endmass = inmass + n * instride/sizeof(float);;
    const double dtsync = *tpos - *tvel;
    double vcentered[NDIM];
    double ke = 0.0;
    double pe = 0.0;

    assert(instride % sizeof(float) == 0);
    assert(outstride % sizeof(float) == 0);
    
    for (; inmass < endmass; inmass += instride/sizeof(float)) {
	VVV(vcentered, = invel, + dtsync * inacc);
	ke += *inmass * Dot(vcentered, vcentered);
	pe += *inmass * *inphi;
	VVV(outvel, = invel, + dtv * inacc);
	VVV(outpos, = inpos, + dt * outvel);
	inpos += instride/sizeof(float);
	invel += instride/sizeof(float);
	inacc += instride/sizeof(float);
	inphi += instride/sizeof(float);
	outpos += outstride/sizeof(float);
	outvel += outstride/sizeof(float);
    }
    *tvel += dtv;
    *tpos += dt;
    *kep = 0.5 * ke;
    *pep = 0.5 * pe;
}


/* Scale velocities for integrator */
void
ConvertV(float *vel, int stride, int n, double scale, int to_physical)
{
    const float *end = vel + n * stride/sizeof(float);
    const float one_on_scale = (float)1.0 / scale;

    assert(stride % sizeof(float) == 0);

    if (to_physical) {
	for (; vel < end; vel += stride/sizeof(float)) {
	    /* convert to physical vel */
	    VS(vel, *= one_on_scale);
	}
    } else {
	for (; vel < end; vel += stride/sizeof(float)) {
	    /* convert from physical vel */
	    VS(vel, *= scale);
	}
    }
}

void
CosmoIntegrate(const float *inmass, const float *inpos, const float *invel, 
	       const float *inacc, const float *inphi, const int instride,
	       float *outpos, float *outvel, const int outstride,
	       const int n, const double dt, const double dtv,
	       cosmology *cosmo, 
	       double *tpos, double *tvel, double *kep, double *pep)
{
    const float *endmass = inmass + n * instride/sizeof(float);
    double vcentered[NDIM];
    double a0, a1, dt_kick, dt_kick_topos, dt_drift;
    double ke = 0.0;
    double pe = 0.0;

    assert(instride % sizeof(float) == 0);
    assert(outstride % sizeof(float) == 0);

    Msgf(("CosmoIntegrate tpos %g tvel %g dt %g dtv %g\n", 
	  *tpos, *tvel, dt, dtv));
    /* Works to start leapfrog, and if dt changes */
    a0 = cosmo->a_at_t(cosmo, *tpos);
    dt_kick_topos = a0*a0*cosmo->kick_t0_t1(cosmo, *tvel, *tpos);
    dt_kick = a0*a0*cosmo->kick_t0_t1(cosmo, *tvel, *tvel + dtv);
    dt_drift = cosmo->drift_t0_t1(cosmo, *tpos, *tpos + dt);
    a1 = cosmo->a_at_t(cosmo, *tpos+dt);

    for (; inmass < endmass; inmass += instride/sizeof(float)) {
	if (dt_kick_topos != 0.0) /* acc might be invalid if dt is 0 */
	    VVV(vcentered, = invel, + dt_kick_topos * inacc);
	else
	    VV(vcentered, = invel);
	ke += *inmass * Dot(vcentered, vcentered) / (a0*a0);
	pe += *inmass * *inphi;
	if (dt_kick != 0.0f)
	    VVV(outvel, = invel, + dt_kick * inacc);
	else
	    VV(outvel, = invel);
	VV(outpos, = (1.0/a0) * inpos); /* to comoving */
	VV(outpos, += dt_drift * outvel);
	VV(outpos, = a1 * outpos);	/* to physical */
	inpos += instride/sizeof(float);
	invel += instride/sizeof(float);
	inacc += instride/sizeof(float);
	inphi += instride/sizeof(float);
	outpos += outstride/sizeof(float);
	outvel += outstride/sizeof(float);
    }
    *tvel += dtv;
    *tpos += dt;
    *kep = 0.5 * ke;
    *pep = 0.5 * pe;
}


