#include "class.h"
#include "cosmo.h"
#include "Malloc.h"
#include "error.h"

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

void
class_params(cosmology *c, char *class_ini)
{
    struct file_content fc;
    struct class_s *p;
    double *pvec;
    ErrorMsg errmsg;

    fc.size = 0;
    
    p = Calloc(1,sizeof(struct class_s));

    class_fail(parser_read_file(class_ini,&fc,errmsg),
	       errmsg,errmsg);

    class_fail(input_init(&fc, &p->pr, &p->ba, &p->th, &p->pt, &p->bs, &p->tr, 
			  &p->pm, &p->sp, &p->nl, &p->le, &p->op, errmsg), 
	       errmsg, errmsg);
    
    class_fail(parser_free(&fc),errmsg,errmsg);

    p->ba.background_verbose = 0;
    
    class_fail(background_init(&p->pr, &p->ba),errmsg,errmsg);

    pvec = Malloc(p->ba.bg_size*sizeof(double));
    class_fail(background_functions(&p->ba, 1.0, p->ba.long_info, pvec), 
	       errmsg, errmsg);
    c->Omega0_m = pvec[p->ba.index_bg_Omega_m];
    c->Omega0_r = pvec[p->ba.index_bg_Omega_r];
    Free(pvec);

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
    c->Gnewt = GM_cgs*(g_Msol10/g_Msol)*pow(sec_Gyr, 2)/pow(cm_kpc,3);

    class_fail(background_free(&p->ba),errmsg,errmsg);
    Free(p);
}
