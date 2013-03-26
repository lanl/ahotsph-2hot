#include "class.h"
#include "cosmo.h"
#include "Malloc.h"
#include "error.h"

#define ERRTOL 5e-8

#define class_fail(function,						\
		   error_message_from_function,				\
		   error_message_output)				\
  do {									\
    if (function == _FAILURE_) {					\
      ErrorMsg Transmit_Error_Message;					\
      sprintf(Transmit_Error_Message,"%s(L:%d) : error in %s;\n=>%s",	\
	      __func__,__LINE__,#function,error_message_from_function);	\
      Error("%s",Transmit_Error_Message);	                        \
    }									\
  } while(0);


struct class_s {
    struct precision pr;        /* for precision parameters */
    struct background ba;       /* for cosmological background */
    struct thermo th;           /* for thermodynamics */
    struct perturbs pt;         /* for source functions */
    struct bessels bs;          /* for bessel functions */
    struct transfers tr;        /* for transfer functions */
    struct primordial pm;       /* for primordial spectra */
    struct spectra sp;          /* for output spectra */
    struct nonlinear nl;        /* for non-linear spectra */
    struct lensing le;          /* for lensed spectra */
    struct output op;           /* for output files */
};

static void
class_background_at_tau(cosmology *c, double tau)
{
    struct class_s *p = c->private;
    int idx;
    double *pvec;
    ErrorMsg errmsg;

    pvec = Malloc(p->ba.bg_size*sizeof(double));
    class_fail(background_at_tau(&p->ba, tau, p->ba.long_info, p->ba.inter_normal, 
				 &idx, pvec), errmsg, errmsg);

    c->a = pvec[p->ba.index_bg_a];
    c->z = 1.0/c->a - 1.0;
    c->t = pvec[p->ba.index_bg_time]/_Gyr_over_Mpc_;
    c->tau = tau;
    c->H = pvec[p->ba.index_bg_H]*_Gyr_over_Mpc_;
    c->conf_distance = pvec[p->ba.index_bg_conf_distance]*1000.0; /* kpc */
    c->kick = pvec[p->ba.index_bg_kick]/_Gyr_over_Mpc_;
    c->drift = pvec[p->ba.index_bg_drift]/_Gyr_over_Mpc_;
    c->Omega_r = pvec[p->ba.index_bg_Omega_r];
    c->Omega_m = pvec[p->ba.index_bg_Omega_m];
    Free(pvec);
}


static void
class_background_at_z(cosmology *c, double z)
{
    struct class_s *p = c->private;
    double tau;
    double err;
    ErrorMsg errmsg;

    class_fail(background_tau_of_z(&p->ba, z, &tau), errmsg, errmsg);
    class_background_at_tau(c, tau);
    err = (1.0+c->z)/(1.0+z) - 1.0;
    if (fabs(err) > ERRTOL)
	Error("Poor precision in background_at_z, relerr = %g\n", err);
}

static void
class_background_at_t(cosmology *c, double t)
{
    struct class_s *p = c->private;
    double tau;
    double err;
    ErrorMsg errmsg;

    class_fail(background_tau_of_t(&p->ba, t*_Gyr_over_Mpc_, &tau),
	       errmsg, errmsg);
    class_background_at_tau(c, tau);
    err = c->t/t - 1.0;
    if (fabs(err) > ERRTOL)
	Error("Poor precision in background_at_t, relerr = %g\n", err);
}

static double
class_t_at_z(cosmology *c, double z)
{
    if (c->z != z) class_background_at_z(c, z);
    return(c->t);
}

static double
class_z_at_t(cosmology *c, double t)
{
    if (c->t != t) class_background_at_t(c, t);
    return(c->z);
}

static double
class_a_at_t(cosmology *c, double t)
{
    if (c->t != t) class_background_at_t(c, t);
    return(c->a);
}

static double
class_t_at_a(cosmology *c, double a)
{
    if (c->a != a) class_background_at_z(c, 1.0/a-1.0);
    return(c->t);
}

static double
class_H_at_z(cosmology *c, double z)
{
    struct class_s *p = c->private;
    double *pvec;
    ErrorMsg errmsg;
    double a = 1.0/(1.0+z);
    double H;

    pvec = Malloc(p->ba.bg_size_normal*sizeof(double));
    class_fail(background_functions(&p->ba, a, p->ba.short_info, pvec), 
	       errmsg, errmsg);
    H = pvec[p->ba.index_bg_H]*_Gyr_over_Mpc_;
    Free(pvec);
    return(H);
}

static double
class_H_at_t(cosmology *c, double t)
{
    if (c->t != t) class_background_at_t(c, t);
    return(c->H);
}


static double
class_conformal_distance_at_z(cosmology *c, double z)
{
    if (c->z != z) class_background_at_z(c, z);
    return(c->conf_distance);
}

static double
class_conformal_distance_at_t(cosmology *c, double t)
{
    if (c->t != t) class_background_at_t(c, t);
    return(c->conf_distance);
}

static double
class_angular_diameter_distance_at_z(cosmology *c, double z)
{
    if (c->z != z) class_background_at_z(c, z);
    return(c->conf_distance/(1.0+z));
}

static double
class_angular_diameter_distance_at_t(cosmology *c, double t)
{
    if (c->t != t) class_background_at_t(c, t);
    return(c->conf_distance/(1.0+c->z));
}

static double
class_luminosity_distance_at_z(cosmology *c, double z)
{
    if (c->z != z) class_background_at_z(c, z);
    return(c->conf_distance*(1.0+z));
}

static double
class_luminosity_distance_at_t(cosmology *c, double t)
{
    if (c->t != t) class_background_at_t(c, t);
    return(c->conf_distance*(1.0+c->z));
}

static double
class_growthfac_at_z(cosmology *c, double z)
{
    struct class_s *p = c->private;
    double pk, pk0, *pk_ic=NULL;
    double k = 1e-7;
    ErrorMsg errmsg;

    class_fail(spectra_pk_at_k_and_z(&p->ba, &p->pm, &p->sp, k, 0.0, &pk0, pk_ic),
	       errmsg, errmsg);

    class_fail(spectra_pk_at_k_and_z(&p->ba, &p->pm, &p->sp, k, z, &pk, pk_ic),
	       errmsg, errmsg);

    return sqrt(pk/pk0);
}

static double
class_growthfac_at_t(cosmology *c, double t)
{
    return(class_growthfac_at_z(c, class_z_at_t(c, t)));
}

static double
class_kick_t0_t1(cosmology *c, double t0, double t1)
{
    double k0, k1;
    class_background_at_t(c, t0);
    k0 = c->kick;
    class_background_at_t(c, t1);
    k1 = c->kick;
    return(k1-k0);
}


static double
class_drift_t0_t1(cosmology *c, double t0, double t1)
{
    double d0, d1;
    class_background_at_t(c, t0);
    d0 = c->drift;
    class_background_at_t(c, t1);
    d1 = c->drift;
    return(d1-d0);
}

static void
class_free(cosmology *c)
{
    struct class_s *p = c->private;
    ErrorMsg errmsg;

    class_fail(spectra_free(&p->sp),errmsg,errmsg);
    class_fail(primordial_free(&p->pm),errmsg,errmsg);
    class_fail(transfer_free(&p->tr),errmsg,errmsg);
    class_fail(perturb_free(&p->pt),errmsg,errmsg);
    class_fail(thermodynamics_free(&p->th),errmsg,errmsg);
    class_fail(background_free(&p->ba),errmsg,errmsg);
    Free(p);
}


void
class_init(cosmology *c, char *class_ini, char *class_pre, double zmax)
{
    struct file_content fc;
    struct file_content fc_input;
    struct file_content fc_precision;
    struct class_s *p;
    ErrorMsg errmsg;

    fc.size = 0;
    fc_input.size = 0;
    fc_precision.size = 0;
    
    memset(c, 0, sizeof(cosmology));
    p = Calloc(1,sizeof(struct class_s));

    if (class_ini)
	class_fail(parser_read_file(class_ini,&fc_input,errmsg),
		   errmsg,errmsg);

    if (class_pre)
	class_fail(parser_read_file(class_pre,&fc_precision,errmsg),
		 errmsg, errmsg);
    
    if (class_ini || class_pre)
	class_fail(parser_cat(&fc_input,&fc_precision,&fc,errmsg),
		   errmsg, errmsg);

    class_fail(parser_free(&fc_input),errmsg,errmsg);
    class_fail(parser_free(&fc_precision),errmsg,errmsg);
    
    class_fail(input_init(&fc, &p->pr, &p->ba, &p->th, &p->pt, &p->bs, &p->tr, 
			  &p->pm, &p->sp, &p->nl, &p->le, &p->op, errmsg), 
	       errmsg, errmsg);
    
    class_fail(parser_free(&fc),errmsg,errmsg);

    p->ba.background_verbose = 0;
    p->th.thermodynamics_verbose = 0;
    p->pt.perturbations_verbose = 0;
    p->tr.transfer_verbose = 0;
    p->pm.primordial_verbose = 0;
    p->sp.spectra_verbose = 0;
    
    if (p->sp.z_max_pk < zmax) p->sp.z_max_pk = zmax;

    class_fail(background_init(&p->pr, &p->ba),errmsg,errmsg);
    
    class_fail(thermodynamics_init(&p->pr,&p->ba,&p->th),errmsg,errmsg);
    
    class_fail(perturb_init(&p->pr,&p->ba,&p->th,&p->pt),errmsg,errmsg);
    
    class_fail(transfer_init(&p->pr,&p->ba,&p->th,&p->pt,&p->bs,&p->tr),
	       errmsg,errmsg);
    
    class_fail(primordial_init(&p->pr,&p->pt,&p->pm),errmsg,errmsg);
    
    class_fail(spectra_init(&p->pr,&p->ba,&p->pt,&p->tr,&p->pm,&p->sp),
	       errmsg,errmsg);

    c->Omega0 = p->ba.Omega0_g + p->ba.Omega0_b;
    if (p->ba.has_cdm == _TRUE_) {
	c->Omega0 += p->ba.Omega0_cdm;
    }
    if (p->ba.has_ncdm == _TRUE_) {
	c->Omega0 += p->ba.Omega0_ncdm_tot;
    }
    if (p->ba.has_lambda == _TRUE_) {
	c->Omega0 += p->ba.Omega0_lambda;
    }
    if (p->ba.has_fld == _TRUE_) {
	c->Omega0 += p->ba.Omega0_fld;
    }
    if (p->ba.has_ur == _TRUE_) {
	c->Omega0 += p->ba.Omega0_ur;
    }
    c->h_100 = p->ba.h;
    c->H0 = p->ba.H0*_Gyr_over_Mpc_;
    c->Omega0_cdm = p->ba.Omega0_cdm;
    c->Omega0_ncdm_tot = p->ba.Omega0_ncdm_tot;
    c->Omega0_b = p->ba.Omega0_b;
    c->Omega0_g = p->ba.Omega0_g;
    c->Omega0_ur = p->ba.Omega0_ur;
    c->Omega0_lambda = p->ba.Omega0_lambda;
    c->Omega0_fld = p->ba.Omega0_fld;
    c->w0_fld = p->ba.w0_fld;
    c->wa_fld = p->ba.wa_fld;
    c->age = p->ba.age;
    c->Gnewt = GNEWT;

    c->private = p;
    class_background_at_z(c, 0.0);
    c->Omega0_m = c->Omega_m;
    c->Omega0_r = c->Omega_r;

    /* Function pointers */
    c->background_at_z = class_background_at_z;
    c->background_at_t = class_background_at_t;
    c->background_at_tau = class_background_at_tau;
    c->t_at_z = class_t_at_z;
    c->z_at_t = class_z_at_t;
    c->a_at_t = class_a_at_t;
    c->t_at_a = class_t_at_a;
    c->H_at_z = class_H_at_z;
    c->H_at_t = class_H_at_t;
    c->conformal_distance_at_z = class_conformal_distance_at_z;
    c->conformal_distance_at_t = class_conformal_distance_at_t;
    c->angular_diameter_distance_at_z = class_angular_diameter_distance_at_z;
    c->angular_diameter_distance_at_t = class_angular_diameter_distance_at_t;
    c->luminosity_distance_at_z = class_luminosity_distance_at_z;
    c->luminosity_distance_at_t = class_luminosity_distance_at_t;
    c->growthfac_at_z = class_growthfac_at_z;
    c->growthfac_at_t = class_growthfac_at_t;
    c->kick_t0_t1 = class_kick_t0_t1;
    c->drift_t0_t1 = class_drift_t0_t1;
    c->free = class_free;
}

