/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include "Malloc.h"
#include "macr.h"
#include "version.h"
#include <gsl/gsl_multimin.h>
#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_sf_erf.h>
#include <gsl/gsl_spline.h>
#include <gsl/gsl_integration.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_linalg.h>

/* Maximum likelihood using gsl_multimin_fminimizer() */

#define MTAB_INCREMENT 1000000
#define RHO_CRIT 2.7753666e+11

#define pow2(x) ({ __typeof__ (x) _x = (x); _x*_x; })
#define pow3(x) ({ __typeof__ (x) _x = (x); _x*_x*_x; })

#define MAX_MODELS 128


typedef struct sample_s {
    double mass;
    double weight;
} sample_s;

typedef struct bin_s {
    double lower;
    double upper;
} bin_s;

typedef struct mdata_s {
    char name[256];
    char type[8];
    int ncols;
    int n;
    int nalloc;
    int binned;
    double mmin;
    double ntot;
    double sum;
    sample_s *sample;
    bin_s *bin;
} mdata_s;

typedef struct pl_params_s {
    int datasets;
    double ssum;
    double ntot;
    double norm;
    double xmin[MAX_MODELS];
    double sum[MAX_MODELS];
    double rho0[MAX_MODELS];
    double vol[MAX_MODELS];
    double initial_val[MAX_MODELS];
    double initial_step[MAX_MODELS];
    gsl_interp_accel *acc[MAX_MODELS];
    gsl_interp_accel *dacc[MAX_MODELS];
    gsl_spline *spline[MAX_MODELS];
    gsl_spline *dspline[MAX_MODELS];
    mdata_s *data[MAX_MODELS];
} pl_params_s;

typedef struct ff_params_s {
    const gsl_vector *v;
    pl_params_s *p;
    int j;
} ff_params_s;

typedef struct model_params {
    char tag[8];
    double rho0;
    double vol;
    double mmin;
    int m_sig_index;
} model_params;

typedef struct minimize_s {
    double x[10];
    double fval;
    int status;
} minimize_s;

minimize_s minimize(double (*f)(const gsl_vector *v, void *params),
		    int np, pl_params_s *par);

/* power law likelihood */
double
pl_ff(double x, void *params)
{
    ff_params_s *ffp = (ff_params_s *)params;
    double alpha = gsl_vector_get(ffp->v, 0);
    
    double f = pow(x, -alpha);
    return f;
}

/* power law likelihood */
double
pl_f(const gsl_vector *v, void *params)
{
    double alpha = gsl_vector_get(v, 0);
    pl_params_s *p = (pl_params_s *)params;

    if (alpha < 0) return DBL_MAX;

#if 1
    ff_params_s ffparams = {.v = v, .p = p};

    gsl_integration_workspace *w 
	= gsl_integration_workspace_alloc(1000);
    double result, error;
    gsl_function f;
    f.function = &pl_ff;
    f.params = &ffparams;
    gsl_integration_qag(&f, p->xmin[0], 1e16, 0, 1e-5, 1000, 6, w, &result, &error); 
    gsl_integration_workspace_free(w);

    double norm = 1.0/result;
#else
    double norm = (alpha - 1.0) * pow(p->xmin, (alpha - 1.0));
#endif
    double loglikelihoods = 0.0;
    for (int i = 0; i < p->data[0]->n; i++) {
	double x = p->data[0]->sample[i].mass;
	double w = p->data[0]->sample[i].weight;
	if (x >= p->xmin[0]) 
	    loglikelihoods += w * log(norm * pow(x, -alpha));
    }
    return -loglikelihoods;
}

/* power law likelihood */
double
gpl_ff(double x, void *params)
{
    ff_params_s *ffp = (ff_params_s *)params;
    double alpha = gsl_vector_get(ffp->v, 0);
    double lambda = gsl_vector_get(ffp->v, 1);
    
    double f = pow(x, -alpha) * exp(-lambda * x);
    return f;
}

/* power law likelihood */
double
gpl_f(const gsl_vector *v, void *params)
{
    double alpha = gsl_vector_get(v, 0);
    double lambda = gsl_vector_get(v, 1);
    pl_params_s *p = (pl_params_s *)params;

    if (alpha <= 0.0 || lambda <= 0.0) return DBL_MAX;

    ff_params_s ffparams = {.v = v, .p = p};

#if 0
    gsl_integration_workspace *w 
	= gsl_integration_workspace_alloc(1000);
    double result, error;
    gsl_function f;
    f.function = &pl_ff;
    f.params = &ffparams;
    gsl_integration_qag(&f, p->xmin, 1e17, 0, 1e-6, 1000, 6, w, &result, &error); 
    gsl_integration_workspace_free(w);
    double norm = 1.0/result;
#else
    double norm = pow(lambda,1.0-alpha)
	/gsl_sf_gamma_inc(1.0-alpha, lambda*p->xmin[0]);
#endif

    long double loglikelihoods = 0.0;
    for (int i = 0; i < p->data[0]->n; i++) {
	double x = p->data[0]->sample[i].mass;
	double w = p->data[0]->sample[i].weight;
	long double f = norm * gpl_ff(x, &ffparams);
	if (f > 0.0)
	    loglikelihoods += w * logl(f);
	else
	    loglikelihoods += w * logl(LDBL_MIN);
    }
    if (!isfinite(loglikelihoods)) loglikelihoods = -LDBL_MAX;
    return -loglikelihoods;
}

/* stretched exponential likelihood */
double
se_f(const gsl_vector *v, void *params)
{
    double lambda = gsl_vector_get(v, 0);
    double beta = gsl_vector_get(v, 1);
    pl_params_s *p = (pl_params_s *)params;

    double loglikelihoods = 0.0;
    for (int i = 0; i < p->data[0]->n; i++) {
	double x = p->data[0]->sample[i].mass;
	double w = p->data[0]->sample[i].weight;
	double f = pow(x, (beta - 1.0)) * beta * lambda
	    * exp(lambda * (pow(p->xmin[0], beta) - pow(x, beta)));
	if (f > 0.0)
	    loglikelihoods += w * log(f);
	else
	    loglikelihoods -= w * 300.0;
   }
    if (!isfinite(loglikelihoods)) loglikelihoods = -DBL_MAX;
    return -loglikelihoods;
}

#define SQRTPI 1.772453850905516
#define SQRT2_PI 0.7978845608028654
#define LN10 2.302585092994046

double
lanl_n(double m, void *params)
{
    ff_params_s *ffp = (ff_params_s *)params;
    double A = gsl_vector_get(ffp->v, 0);
    double a = gsl_vector_get(ffp->v, 1);
    double b = gsl_vector_get(ffp->v, 2);
    double c = gsl_vector_get(ffp->v, 3);
    pl_params_s *p = ffp->p;

    double logm = log(m);
    double sigma = gsl_spline_eval(p->spline[ffp->j], logm, p->acc[ffp->j]);
    double dsdm = gsl_spline_eval(p->dspline[ffp->j], logm, p->dacc[ffp->j]);
    m = pow(m, 0.982);
    double rhomm = A*(p->rho0[ffp->j]/m)*(p->vol[ffp->j]/m);
    double f = rhomm * (pow(sigma, -a) + b) * exp(-c / (sigma * sigma)) * dsdm;
    if (f < 0.0 || !isfinite(f)) Error("Bad f %g\n", f);
    else if (f == 0.0) return DBL_MIN;
    else return f;
 }

/* LANL likelihood */
double
lanl_f(const gsl_vector *v, void *params)
{
    double A = gsl_vector_get(v, 0);
    double a = gsl_vector_get(v, 1);
    double b = gsl_vector_get(v, 2);
    double c = gsl_vector_get(v, 3);
    pl_params_s *p = (pl_params_s *)params;

    if (A < 0.0 || a < 0.0 || b < 0.0 || c < 0.0) {
	return DBL_MAX;
    }
    ff_params_s ffparams = {.v = v, .p = p};

    gsl_integration_workspace *w 
	= gsl_integration_workspace_alloc(1000);
    double resultn, error;
    gsl_function fn;
    fn.function = &lanl_n;
    fn.params = &ffparams;

    double nntot = 0.0;
    for (int j = 0; j < p->datasets; j++) {
	ffparams.j = j;
	gsl_integration_qag(&fn, p->xmin[j], 5e16, 0.0, 2e-5, 1000, 6, w, &resultn, &error);
	nntot += resultn;
    }
    gsl_integration_workspace_free(w);

    double loglikelihoods = 0.0;
    for (int j = 0; j < p->datasets; j++) {
	ffparams.j = j;
	for (int i = 0; i < p->data[j]->n; i++) {
	    double x = p->data[j]->sample[i].mass;
	    double w = p->data[j]->sample[i].weight;
	    loglikelihoods += w * log(lanl_n(x, &ffparams));
	}
    }
    loglikelihoods -= nntot;
    return -loglikelihoods;
}

double
gps_n(double m, void *params)
{
    ff_params_s *ffp = (ff_params_s *)params;
    double A = gsl_vector_get(ffp->v, 0);
    double a = gsl_vector_get(ffp->v, 1);
    double c = gsl_vector_get(ffp->v, 2);
    pl_params_s *p = ffp->p;

    double logm = log(m);
    double sigma = gsl_spline_eval(p->spline[ffp->j], logm, p->acc[ffp->j]);
    double dsdm = gsl_spline_eval(p->dspline[ffp->j], logm, p->dacc[ffp->j]);
    m = pow(m, 0.982);
    double rhomm = A*(p->rho0[ffp->j]/m)*(p->vol[ffp->j]/m);
    double f = rhomm * pow(sigma, -a) * exp(-c / (sigma * sigma)) * dsdm;
    if (f < 0.0 || !isfinite(f)) Error("Bad f %g\n", f);
    else if (f == 0.0) return DBL_MIN;
    else return f;
 }

/* Generalized Press-Schecter likelihood */
double
gps_f(const gsl_vector *v, void *params)
{
    double A = gsl_vector_get(v, 0);
    double a = gsl_vector_get(v, 1);
    double c = gsl_vector_get(v, 2);
    pl_params_s *p = (pl_params_s *)params;

    if (A < 0.0 || a < 0.0 || c < 0.0) {
	return DBL_MAX;
    }
    ff_params_s ffparams = {.v = v, .p = p};

    gsl_integration_workspace *w 
	= gsl_integration_workspace_alloc(1000);
    double resultn, error;
    gsl_function fn;
    fn.function = &gps_n;
    fn.params = &ffparams;

    double nntot = 0.0;
    for (int j = 0; j < p->datasets; j++) {
	ffparams.j = j;
	gsl_integration_qag(&fn, p->xmin[j], 5e16, 0.0, 2e-5, 1000, 6, w, &resultn, &error);
	nntot += resultn;
    }
    gsl_integration_workspace_free(w);

    double loglikelihoods = 0.0;
    for (int j = 0; j < p->datasets; j++) {
	ffparams.j = j;
	for (int i = 0; i < p->data[j]->n; i++) {
	    double x = p->data[j]->sample[i].mass;
	    double w = p->data[j]->sample[i].weight;
	    loglikelihoods += w * log(gps_n(x, &ffparams));
	}
    }
    loglikelihoods -= nntot;
    return -loglikelihoods;
}

double
jenkins_n(double m, void *params)
{
    ff_params_s *ffp = (ff_params_s *)params;
    double a = gsl_vector_get(ffp->v, 0);
    double b = gsl_vector_get(ffp->v, 1);
    double c = gsl_vector_get(ffp->v, 2);
    pl_params_s *p = ffp->p;

    double logm = log(m);
    double sigma = gsl_spline_eval(p->spline[ffp->j], logm, p->acc[ffp->j]);
    double dsdm = gsl_spline_eval(p->dspline[ffp->j], logm, p->dacc[ffp->j]);
    double rhomm = a*p->rho0[ffp->j]/(m*m);
    double f = rhomm * exp(-pow(fabs(log(1.0/sigma)+b),c)) * dsdm;
    return f;
 }

/* Jenkins likelihood */
double
jenkins_f(const gsl_vector *v, void *params)
{
    double a = gsl_vector_get(v, 0);
    /* double b = gsl_vector_get(v, 1); */
    /* double c = gsl_vector_get(v, 2); */
    pl_params_s *p = (pl_params_s *)params;

    if (a < 0.0) {
	return DBL_MAX;
    }
    ff_params_s ffparams = {.v = v, .p = p};

    gsl_integration_workspace *w 
	= gsl_integration_workspace_alloc(1000);
    double nn[p->datasets];
    double result, error;
    gsl_function f;
    f.function = &jenkins_n;
    f.params = &ffparams;

    for (int j = 0; j < p->datasets; j++) {
	ffparams.j = j;
	gsl_integration_qag(&f, p->xmin[j], 2e16, 0, 4e-5, 1000, 6, w, &result, &error);
	nn[j] = p->vol[j]*result;
    }
    gsl_integration_workspace_free(w);

    double loglikelihoods = 0.0;
    double subl[p->datasets];
    memset(subl, 0, p->datasets*sizeof(double));
    for (int j = 0; j < p->datasets; j++) {
	ffparams.j = j;
	for (int i = 0; i < p->data[j]->n; i++) {
	    double x = p->data[j]->sample[i].mass;
	    double w = p->data[j]->sample[i].weight;
	    double f = w * jenkins_n(x, &ffparams) * p->vol[j];
	    if (f > 0.0) subl[j] += log(f) ;
	    else subl[j] += log(DBL_MIN);
	}
	subl[j] -= nn[j];
    }
    for (int j = 0; j < p->datasets; j++) {
	printf("-l[%d] %12.2f %5.3f\n", j, -subl[j], nn[j]/p->data[j]->n);
	loglikelihoods += subl[j];
    }
    if (!isfinite(loglikelihoods)) loglikelihoods = -DBL_MAX;
    return -loglikelihoods;
}

double
tinker_n(double m, void *params)
{
    ff_params_s *ffp = (ff_params_s *)params;
    double A = gsl_vector_get(ffp->v, 0);
    double a = gsl_vector_get(ffp->v, 1);
    double b = gsl_vector_get(ffp->v, 2);
    double c = gsl_vector_get(ffp->v, 3);
    pl_params_s *p = ffp->p;

    double logm = log(m);
    double sigma = gsl_spline_eval(p->spline[ffp->j], logm, p->acc[ffp->j]);
    double dsdm = gsl_spline_eval(p->dspline[ffp->j], logm, p->dacc[ffp->j]);
    m = pow(m, 0.982);
    double rhomm = A*(p->rho0[ffp->j]/m)*(p->vol[ffp->j]/m);
    double f = rhomm * (pow(sigma/b, -a) + 1.0) * exp(-c / (sigma * sigma)) * dsdm;
    if (f < 0.0 || !isfinite(f)) Error("Bad f %g\n", f);
    else if (f == 0.0) return DBL_MIN;
    else return f;
 }

/* Tinker likelihood */
double
tinker_f(const gsl_vector *v, void *params)
{
    double A = gsl_vector_get(v, 0);
    double a = gsl_vector_get(v, 1);
    double b = gsl_vector_get(v, 2);
    double c = gsl_vector_get(v, 3);
    pl_params_s *p = (pl_params_s *)params;

    if (A < 0.0 || a < 0.0 || b <= 0.0 || c < 0.0 || c > 2.0) {
	return DBL_MAX;
    }
    ff_params_s ffparams = {.v = v, .p = p};

    gsl_integration_workspace *w 
	= gsl_integration_workspace_alloc(1000);
    double resultn, error;
    gsl_function fn;
    fn.function = &tinker_n;
    fn.params = &ffparams;

    double nntot = 0.0;
    for (int j = 0; j < p->datasets; j++) {
	ffparams.j = j;
	gsl_integration_qag(&fn, p->xmin[j], 5e16, 0.0, 2e-5, 1000, 6, w, &resultn, &error);
	nntot += resultn;
    }
    gsl_integration_workspace_free(w);

    double loglikelihoods = 0.0;
    for (int j = 0; j < p->datasets; j++) {
	ffparams.j = j;
	for (int i = 0; i < p->data[j]->n; i++) {
	    double x = p->data[j]->sample[i].mass;
	    double w = p->data[j]->sample[i].weight;
	    loglikelihoods += w * log(tinker_n(x, &ffparams));
	}
    }
    loglikelihoods -= nntot;
    return -loglikelihoods;
}

double
bhat_n(double m, void *params)
{
    ff_params_s *ffp = (ff_params_s *)params;
    double A = gsl_vector_get(ffp->v, 0);
    double a = gsl_vector_get(ffp->v, 1);
    double p = gsl_vector_get(ffp->v, 2);
    double q0 = gsl_vector_get(ffp->v, 3);
    double delta_c = 1.68647;
    pl_params_s *pp = ffp->p;

    double logm = log(m);
    double sigma = gsl_spline_eval(pp->spline[ffp->j], logm, pp->acc[ffp->j]);
    double dsdm = gsl_spline_eval(pp->dspline[ffp->j], logm, pp->dacc[ffp->j]);
    double rhomm = A * SQRT2_PI * (pp->rho0[ffp->j]/m) * (pp->vol[ffp->j]/m);
    double f = rhomm * (1.0 + pow(sigma * sigma / (a * delta_c * delta_c), p))
	* pow(delta_c * sqrt(a) / sigma, q0)
	* exp(-a * delta_c * delta_c /(2.0 * sigma * sigma)) * dsdm;
    return f;
 }


/* Bhat likelihood */
double
bhat_f(const gsl_vector *v, void *params)
{
    double A = gsl_vector_get(v, 0);
    double a = gsl_vector_get(v, 1);
    /* double p = gsl_vector_get(v, 2); */
    /* double q0 = gsl_vector_get(v, 3); */
    pl_params_s *pp = (pl_params_s *)params;

    if (A < 0.0 || a < 0.0) {
	return DBL_MAX;
    }
    ff_params_s ffparams = {.v = v, .p = pp};

    gsl_integration_workspace *w 
	= gsl_integration_workspace_alloc(1000);
    double resultn, error;
    gsl_function fn;
    fn.function = &bhat_n;
    fn.params = &ffparams;

    double nntot = 0.0;
    for (int j = 0; j < pp->datasets; j++) {
	ffparams.j = j;
	gsl_integration_qag(&fn, pp->xmin[j], 5e16, 0.0, 2e-5, 1000, 6, w, &resultn, &error);
	nntot += resultn;
    }
    gsl_integration_workspace_free(w);

    double loglikelihoods = 0.0;
    for (int j = 0; j < pp->datasets; j++) {
	ffparams.j = j;
	for (int i = 0; i < pp->data[j]->n; i++) {
	    double x = pp->data[j]->sample[i].mass;
	    double w = pp->data[j]->sample[i].weight;
	    loglikelihoods += w * log(bhat_n(x, &ffparams));
	}
    }
    loglikelihoods -= nntot;
    return -loglikelihoods;
}

/* truncated power law likelihood */
double
tpl_f(const gsl_vector *v, void *params)
{
    double alpha = gsl_vector_get(v, 0);
    double lambda = gsl_vector_get(v, 1);
    pl_params_s *p = (pl_params_s *)params;

    if (alpha <= 0.0 || lambda <= 0.0 || lambda * p->xmin[0] > 100.0) {
	return DBL_MAX;
    }

    double loglikelihoods = 0.0;
    for (int i = 0; i < p->data[0]->n; i++) {
	double x = p->data[0]->sample[i].mass;
	double w = p->data[0]->sample[i].weight;
	double f = pow(lambda, 1.0 - alpha) /
	    (pow(x, alpha) * exp(lambda * x)
	     * gsl_sf_gamma_inc(1.0 - alpha, lambda * p->xmin[0]));
	if (f > 0.0) loglikelihoods += w * log(f);
	else loglikelihoods += w * log(DBL_MIN);
    }
    if (!isfinite(loglikelihoods)) loglikelihoods = -DBL_MAX;
    return -loglikelihoods;
}

static int 
cmp(const void *a, const void *b) {
    sample_s *aa = (sample_s *)a;
    sample_s *bb = (sample_s *)b;
    return (aa->mass > bb->mass);
}

model_params
get_model_params(char *name)
{
    if (strstr(name, "250Mpc")) {
	model_params ret = {.tag = "250Mpc", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(250.0), .m_sig_index = 1};
	ret.mmin = 399.5*ret.rho0*ret.vol/pow3(4096.0);
	return ret;
    }
    if (strstr(name, "500Mpc")) {
	model_params ret = {.tag = "500Mpc", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(500.0), .m_sig_index = 1};
	ret.mmin = 399.5*ret.rho0*ret.vol/pow3(4096.0);
	return ret;
    }
    if (strstr(name, "1Gpc")) {
	model_params ret = {.tag = "1Gpc", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(1000.0), .m_sig_index = 1};
	ret.mmin = 399.5*ret.rho0*ret.vol/pow3(4096.0);
	return ret;
    }
    if (strstr(name, "2Gpc")) {
	model_params ret = {.tag = "2Gpc", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(2000.0), .m_sig_index = 1};
	ret.mmin = 399.5*ret.rho0*ret.vol/pow3(4096.0);
	return ret;
    }
    if (strstr(name, "4Gpc")) {
	model_params ret = {.tag = "4Gpc", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(4000.0), .m_sig_index = 1};
	ret.mmin = 399.5*ret.rho0*ret.vol/pow3(4096.0);
	return ret;
    }
    if (strstr(name, "8Gpc")) {
	model_params ret = {.tag = "8Gpc", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(8000.0), .m_sig_index = 1};
	ret.mmin = 399.5*ret.rho0*ret.vol/pow3(4096.0);
	return ret;
    }
    if (strstr(name, "lcdm_pp")) {
	model_params ret = {.tag = "pp", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(250.0), .m_sig_index = 0};
	ret.mmin = 1999.5*ret.rho0*ret.vol/pow3(4096.0);
	return ret;
    }
    if (strstr(name, "lcdm_po")) {
	model_params ret = {.tag = "po", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(500.0), .m_sig_index = 0};
	ret.mmin = 1999.5*ret.rho0*ret.vol/pow2(4096.0);
	return ret;
    }
    if (strstr(name, "lcdm_pn")) {
	model_params ret = {.tag = "pn", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(1000.0), .m_sig_index = 0};
	ret.mmin = 1999.5*ret.rho0*ret.vol/pow3(4096.0);
	return ret;
    }
    if (strstr(name, "lcdm_pq")) {
	model_params ret = {.tag = "pq", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(2000.0), .m_sig_index = 0};
	ret.mmin = 1999.5*ret.rho0*ret.vol/pow3(4096.0);
	return ret;
    }
    if (strstr(name, "lcdm_pr")) {
	model_params ret = {.tag = "pr", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(4000.0), .m_sig_index = 0};
	ret.mmin = 1999.5*ret.rho0*ret.vol/pow3(4096.0);
	return ret;
    }
    if (strstr(name, "lcdm_pt")) {
	model_params ret = {.tag = "pt", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(1000.0), .m_sig_index = 1, .mmin = 4.40832e+11};
	return ret;
    }
    if (strstr(name, "lcdm_pu")) {
	model_params ret = {.tag = "pu", .rho0 = 0.272187*RHO_CRIT, .vol = pow3(8000.0), .m_sig_index = 1, .mmin = 2.25705984e+14};
	return ret;
    }
    if (strstr(name, "lcdm_qj")) {
	model_params ret = {.tag = "qj", .rho0 = 0.284798*RHO_CRIT, .vol = pow3(4000.0), .m_sig_index = 2, .mmin = 2.944629998e+13};
	return ret;
    }
    if (strstr(name, "lcdm_ql")) {
	model_params ret = {.tag = "ql", .rho0 = 0.284798*RHO_CRIT, .vol = pow3(8000.0), .m_sig_index = 2, .mmin = 2.355703998e+14};
	return ret;
    }
    if (strstr(name, "lcdm_qm")) {
	model_params ret = {.tag = "qm", .rho0 = 0.284798*RHO_CRIT, .vol = pow3(1280.0), .m_sig_index = 2};
	ret.mmin = 399.5*ret.rho0*ret.vol/pow3(1280.0);
	return ret;
    }
    if (strstr(name, "lcdm_qn")) {
	model_params ret = {.tag = "qn", .rho0 = 0.284798*RHO_CRIT, .vol = pow3(16000.0), .m_sig_index = 2};
	ret.mmin = 399.5*ret.rho0*ret.vol/pow2(4096.0);
	return ret;
    }
    if (strstr(name, "lcdm_qo")) {
	model_params ret = {.tag = "qo", .rho0 = 0.284798*RHO_CRIT, .vol = pow3(250.0), .m_sig_index = 2};
	ret.mmin = 399.5*ret.rho0*ret.vol/pow2(4096.0);
	return ret;
    }
    if (strstr(name, "lcdm_ei")) {
	model_params ret = {.tag = "ei", .rho0 = 0.3*RHO_CRIT, .vol = pow3(768.0), .m_sig_index = 3};
	return ret;
    }
    if (strstr(name, "lcdm_fb")) {
	model_params ret = {.tag = "fb", .rho0 = 0.3*RHO_CRIT, .vol = pow3(384.0), .m_sig_index = 3};
	return ret;
    }
    if (strstr(name, "lcdm_fc")) {
	model_params ret = {.tag = "fc", .rho0 = 0.3*RHO_CRIT, .vol = pow3(192.0), .m_sig_index = 3};
	return ret;
    }
    if (strstr(name, "lcdm_fd")) {
	model_params ret = {.tag = "fd", .rho0 = 0.3*RHO_CRIT, .vol = pow3(1536.0), .m_sig_index = 3};
	return ret;
    }
    if (strstr(name, "lcdm_fe")) {
	model_params ret = {.tag = "fe", .rho0 = 0.3*RHO_CRIT, .vol = pow3(3072.0), .m_sig_index = 3};
	return ret;
    }
    if (strstr(name, "lcdm_ff")) {
	model_params ret = {.tag = "ff", .rho0 = 0.3*RHO_CRIT, .vol = pow3(400.0), .m_sig_index = 3};
	return ret;
    }
    if (strstr(name, "lcdm_fg")) {
	model_params ret = {.tag = "fg", .rho0 = 0.3*RHO_CRIT, .vol = pow3(384.0), .m_sig_index = 3};
	return ret;
    }
    if (strstr(name, "lcdm_fl")) {
	model_params ret = {.tag = "fl", .rho0 = 0.3*RHO_CRIT, .vol = pow3(96.0), .m_sig_index = 3};
	return ret;
    }
    if (strstr(name, "lcdm_fm")) {
	model_params ret = {.tag = "fm", .rho0 = 0.3*RHO_CRIT, .vol = pow3(271.529), .m_sig_index = 3};
	return ret;
    }
    if (strstr(name, "lcdm_fn")) {
	model_params ret = {.tag = "fn", .rho0 = 0.3*RHO_CRIT, .vol = pow3(4344.46), .m_sig_index = 3};
	return ret;
    }
    if (strstr(name, "lcdm_fr")) {
	model_params ret = {.tag = "fr", .rho0 = 0.3*RHO_CRIT, .vol = pow3(135.764), .m_sig_index = 3};
	return ret;
    }
    Error("model %s not found\n", name);
}

void
invert_matrix(const gsl_matrix *a, gsl_matrix *a_inv)
{
    int dummy;
    gsl_permutation *p = gsl_permutation_alloc(a->size1);
    gsl_matrix *aa = gsl_matrix_alloc(a->size1, a->size2);

    gsl_matrix_memcpy(aa, a);
    gsl_linalg_LU_decomp(aa, p, &dummy);
    gsl_linalg_LU_invert(aa, p, a_inv);

    gsl_permutation_free(p);
    gsl_matrix_free(aa);
}

typedef struct options_s {
    double expansion_fac;
    int do_lanl;
    int do_tinker;
    int do_bhat;
    int do_gps;
    int fof_correct;
    char path[256];
} options_s;

void
parse_options(int argc, char *argv[], options_s *opt)
{
    struct option long_options[] = {
	{"expansion_fac", 1, 0, 'a'},
	{"bhat", 0, &opt->do_bhat, 1},
	{"gps", 0, &opt->do_gps, 1},
	{"lanl", 0, &opt->do_lanl, 1},
	{"nolanl", 0, &opt->do_lanl, 0},
	{"tinker", 0, &opt->do_tinker, 1},
	{"fof_correct", 0, &opt->fof_correct, 1},
	{"path", 1, 0, 'p'},
	{0, 0, 0, 0}
    };
    int option_index = 0;

    while (1) {
	
	int c = getopt_long(argc, argv, "a:p:bglntf", long_options, &option_index);
	if (c == -1) break;
	
	if (c == 'a') {
	    opt->expansion_fac = atof(optarg);
	} else if (c == 'p') {
	    strncpy(opt->path, optarg, sizeof(opt->path));
	} else if (c == 'f') {
	    opt->fof_correct = 1;
	} else if (c == 'b') {
	    opt->do_bhat = 1;
	} else if (c == 'g') {
	    opt->do_gps = 1;
	} else if (c == 'l') {
	    opt->do_lanl = 1;
	} else if (c == 'n') {
	    opt->do_lanl = 0;
	} else if (c == 't') {
	    opt->do_tinker = 1;
	} else if (c) {
	    fprintf(stderr, "Unknown option\n");
	    exit(1);
	}
    }
}

/* Read histogram or list of masses */
/* Binned data uses count in bin as weight */
/* Unbinned data is compressed by using weight to represent multiple identical values */
void
read_data(mdata_s *m, options_s *opt)
{
    if (strstr(m->name, ".txt")) {
	strncpy(m->type, "txt", sizeof(m->type));
	m->ncols = 1;
	m->binned = 0;
    } else if (strstr(m->name, "bgc2") || strstr(m->name, ".halos")) {
	strncpy(m->type, "sdf", sizeof(m->type));
	m->ncols = 1;
	m->binned = 0;
    } else if (strstr(m->name, ".so200b")) {
	strncpy(m->type, "so200b", sizeof(m->type));
	m->ncols = 5;
	m->binned = 1;
    } else if (strstr(m->name, ".mf200b")) {
	strncpy(m->type, "mf200b", sizeof(m->type));
	m->ncols = 4;
	m->binned = 1;
    } else {
	Error("Unknown type for file %s\n", m->name);
    }
    
    char fname[256];
    if (m->name[0] != '/' && m->name[0] != '.') {
	snprintf(fname, sizeof(fname), "%s/%s", opt->path, m->name);
    } else {
	strncpy(fname, m->name, sizeof(fname));
    }

    m->mmin = get_model_params(m->name).mmin;

    m->n = 0;
    m->nalloc = MTAB_INCREMENT;
    m->sample = Malloc(m->nalloc * sizeof(sample_s));
    if (m->binned) m->bin = Malloc(m->nalloc * sizeof(bin_s));
    
    FILE *input;
    char buffer[1024];
    Fopen(input, fname, "r");
    while (fgets(buffer, 1024, input)) {
	int ret;
	if (buffer[0] == '#') continue;
	if (!strcmp(m->type, "so200b"))  {
	    long int count;
	    double vol, lower, upper ;
	    ret = sscanf(buffer, "%ld %lf %lf %lf %*f %*f %*f", 
			 &count, &vol, &lower, &upper);
	    if (m->bin[m->n].lower < m->mmin) continue;
	    m->sample[m->n].mass = pow(10.0, (log10(lower)+log10(upper))/2.0);
	    m->sample[m->n].weight = count;
	    m->bin[m->n].lower = lower;
	    m->bin[m->n].upper = upper;
	} else if (!strcmp(m->type, "mf200b"))  {
	    ret = sscanf(buffer, "%lf %*f %lf %lf %lf %*f", 
			 &m->sample[m->n].mass, &m->sample[m->n].weight, 
			 &m->bin[m->n].lower, &m->bin[m->n].upper);
	    if (m->bin[m->n].lower < m->mmin) continue;
	} else {
	    ret = sscanf(buffer, "%lf", &m->sample[m->n].mass);
	    /* If input is sorted, this will merge identical values */
	    if (m->n && m->sample[m->n].mass == m->sample[m->n-1].mass) {
		m->n--;
		m->sample[m->n].weight += 1.0;
	    } else {
		m->sample[m->n].weight = 1.0;
	    }
	}
	if (ret != m->ncols) {
	    fprintf(stderr, "Did not parse line %d, %s\n", m->n, buffer);
	    exit(1);
	}
	m->n++;
	if (!(m->n % MTAB_INCREMENT)) {
	    m->nalloc += MTAB_INCREMENT;
	    m->sample = Realloc(m->sample, m->nalloc * sizeof(sample_s));
	    if (m->binned) m->bin = Realloc(m->bin, m->nalloc * sizeof(bin_s));
	}
    }
    Fclose(input);
    m->sample = Realloc(m->sample, m->nalloc * sizeof(sample_s));
    if (m->binned) m->bin = Realloc(m->bin, m->nalloc * sizeof(bin_s));
    /* Use element 0 as min later */
    if (!m->binned) qsort(m->sample, m->n, sizeof(sample_s), cmp);
    
}


int
main(int argc, char *argv[])
{
    options_s opt = {.expansion_fac = 1.0, .do_lanl = 1, .do_tinker = 0, .do_bhat = 0,
		     .do_gps = 0, .path = "/scratch3/mswarren/mf2013"};

    parse_options(argc, argv, &opt);

    int nn = argc-optind;
    if (nn >= MAX_MODELS) Error("Increase MAX_MODELS\n");
    mdata_s mtab[nn];
    for (mdata_s *m = mtab; m < mtab + nn; m++) {
	strncpy(m->name, argv[m-mtab+optind], sizeof(m->name));
	read_data(m, &opt);
    }

    char msname[4][256] = {"%s/lcdm_pn/lcdm_pn_sigma_%.03f.dat", 
			   "%s/lcdm_pt/lcdm_pt_sigma_%.03f.dat", 
			   "%s/lcdm_qj/lcdm_qj_sigma_%.03f.dat",
			   "%s/lcdm_hv_sigma_%.03f.dat"};
    gsl_interp_accel *acc[nn];
    gsl_spline *spline[nn];
    gsl_interp_accel *dacc[nn];
    gsl_spline *dspline[nn];
    
    double *mm[nn];
    double *ss[nn];
    double *ds[nn];
    int msn[nn];
    for (int j = 0; j < 4; j++) {
	int msn_alloc = MTAB_INCREMENT;
	mm[j] = Malloc(msn_alloc * sizeof(double));
	ss[j] = Malloc(msn_alloc * sizeof(double));
	ds[j] = Malloc(msn_alloc * sizeof(double));
	msn[j] = 0;
	char buffer[1024];
	char fname[256];
	FILE *input;
	snprintf(fname, sizeof(fname), msname[j], opt.path, opt.expansion_fac);
	printf("sigma %d is %s\n", j, fname);
	Fopen(input, fname, "r");
	while (fgets(buffer, 1024, input)) {
	    if (buffer[0] == '#') continue;
	    /* # r*h(Mpc) mass*h sigma_tophat sigma_gaussian ln(1/sigma) f_PS f_ST f_Jenkins f_LANL f_Bhat f_Reed f_Tinker ds_dm */
	    int ret = sscanf(buffer, "%*f %lf %lf %*f %*f %*f %*f %*f %*f %*f %*f %*f %lf\n", mm[j]+msn[j], ss[j]+msn[j], ds[j]+msn[j]);
	    if (ret != 3) {
		fprintf(stderr, "Did not parse line %d, %s\n", msn[j], buffer);
		exit(1);
	    }
	    msn[j]++;
	    if (!(msn[j] % MTAB_INCREMENT)) {
		msn_alloc += MTAB_INCREMENT;
		mm[j] = Realloc(mm[j], msn_alloc * sizeof(double));
		ss[j] = Realloc(ss[j], msn_alloc * sizeof(double));
		ds[j] = Realloc(ds[j], msn_alloc * sizeof(double));
	    }
	}
	Fclose(input);
	mm[j] = Realloc(mm[j], msn[j] * sizeof(double));
	ss[j] = Realloc(ss[j], msn[j] * sizeof(double));
	ds[j] = Realloc(ds[j], msn[j] * sizeof(double));

	for (int i = 0; i < msn[j]; i++) {
	    mm[j][i] = log(1e10*mm[j][i]);
	}

	if (msn[j] > 0) {
	    acc[j] = gsl_interp_accel_alloc();
	    spline[j] = gsl_spline_alloc(gsl_interp_cspline, msn[j]);
	    gsl_spline_init(spline[j], mm[j], ss[j], msn[j]);
	    dacc[j] = gsl_interp_accel_alloc();
	    dspline[j] = gsl_spline_alloc(gsl_interp_cspline, msn[j]);
	    gsl_spline_init(dspline[j], mm[j], ds[j], msn[j]);
	}
    }
    
    if (opt.fof_correct) {
	for (mdata_s *m = mtab; m < mtab + nn; m++) {
	    model_params mp = get_model_params(m->name);
	    double pmass = mp.rho0*mp.vol/pow3(4096.0);
	    for (sample_s *s = m->sample; s < m->sample + m->n; s++) {
		s->mass *= 1.0-pow(s->mass/pmass, -0.6);
	    }
	}
    }


    double ntot = 0.0;
    double ssum = 0.0;
    for (mdata_s *m = mtab; m < mtab + nn; m++) {
	m->sum = 0.0;
	for (sample_s *s = m->sample; s < m->sample + m->n; s++) {
	    m->ntot += s->weight;
	    m->sum += s->mass * s->weight;
	}
	ntot += m->ntot;
	ssum += m->sum;
    }

    if (opt.do_lanl) {
	int np_lanl = 4;
	pl_params_s *p, params_lanl = {.datasets = nn,
				       .ntot = ntot,
				       .ssum = ssum,
				       .initial_val[0] = 0.74185,
				       .initial_val[1] = 1.57013,
				       .initial_val[2] = 0.28123,
				       .initial_val[3] = 1.19125,
				       .initial_step[0] = 1.0,
				       .initial_step[1] = 1.0,
				       .initial_step[2] = 1.0,
				       .initial_step[3] = 1.0};
	p = &params_lanl;
	
	for (int j = 0; j < nn; j++) {
	    model_params mp = get_model_params(mtab[j].name);
	    printf("model %d is %s %s\n", j, mp.tag, mtab[j].name);
	    if (mtab[j].binned) {
		p->xmin[j] = mtab[j].bin[0].lower;
	    } else {
		p->xmin[j] = mtab[j].sample[0].mass;
	    }
	    p->data[j] = &mtab[j];
	    p->rho0[j] = mp.rho0;
	    p->vol[j] = mp.vol;
	    p->spline[j] = spline[mp.m_sig_index];
	    p->acc[j] = acc[mp.m_sig_index];
	    p->dspline[j] = dspline[mp.m_sig_index];
	    p->dacc[j] = dacc[mp.m_sig_index];
	}
	gsl_integration_workspace *w 
	    = gsl_integration_workspace_alloc(1000);
	double resultn, error;
	gsl_vector *v = gsl_vector_alloc(np_lanl);
	for (int i = 0; i < np_lanl; i++) {
	    gsl_vector_set (v, i, p->initial_val[i]);
	}
	ff_params_s ffparams = {.v = v, .p = p};
	gsl_function fn;
	fn.function = &lanl_n;
	fn.params = &ffparams;
	for (int j = 0; j < nn; j++) {
	    ffparams.j = j;
	    gsl_integration_qag(&fn, p->xmin[j], 5e16, 0.0, 2e-5, 1000, 6, w, &resultn, &error); 
	    printf("n %9.0f sum %12g xmin %12g ratio %5.3f\n", 
		   p->data[j]->ntot, p->data[j]->sum, p->xmin[j], 
		   resultn/p->data[j]->ntot);
	}
	gsl_integration_workspace_free(w);
	
	minimize_s s_lanl = minimize(lanl_f, np_lanl, &params_lanl);
	
	if (!s_lanl.status) {
	    printf("LANL for 1/(1+z) = %.03f version %s\nmodels", 
		   opt.expansion_fac, mffit_version);
	    for (int j = 0; j < nn; j++) {
		printf(" %s", get_model_params(mtab[j].name).tag);
	    }
	    printf("\nlanl_A = %.5f;", s_lanl.x[0]);
	    printf(" lanl_a = %.5f;", s_lanl.x[1]);
	    printf(" lanl_b = %.5f;", s_lanl.x[2]);
	    printf(" lanl_c = %.5f;\n", s_lanl.x[3]);
	    printf("lanl_L = %.3f;\n", -s_lanl.fval);
	    printf("%.5f %.5f %.5f %.5f FIT\n", s_lanl.x[0], s_lanl.x[1], s_lanl.x[2], s_lanl.x[3]);

	    gsl_matrix *a = gsl_matrix_alloc(np_lanl, np_lanl);
	    gsl_matrix *ai = gsl_matrix_alloc(np_lanl, np_lanl);
	    /* Calculate Hessian numerically */
	    double h = 0.0001;
	    for (int ii = 0; ii < np_lanl; ii++) {
		for (int jj = ii; jj < np_lanl; jj++) {
		    gsl_vector_set(v, 0, s_lanl.x[0]);
		    gsl_vector_set(v, 1, s_lanl.x[1]);
		    gsl_vector_set(v, 2, s_lanl.x[2]);
		    gsl_vector_set(v, 3, s_lanl.x[3]);
		    double xx;
		    if (ii == jj) {
			double x0 = -lanl_f(v, &params_lanl);
			gsl_vector_set(v, ii, s_lanl.x[ii]+h);
			double xp = -lanl_f(v, &params_lanl);
			gsl_vector_set(v, ii, s_lanl.x[ii]-h);
			double xm = -lanl_f(v, &params_lanl);
			xx = (xp+xm-2.0*x0)/h;
			/* printf("%.3f %.3f\n", xp, xm); */
			gsl_matrix_set(a,ii,jj,-xx);
		    } else {
			gsl_vector_set(v, ii, s_lanl.x[ii]+h);
			gsl_vector_set(v, jj, s_lanl.x[jj]+h);
			double xpp = -lanl_f(v, &params_lanl);
			gsl_vector_set(v, ii, s_lanl.x[ii]+h);
			gsl_vector_set(v, jj, s_lanl.x[jj]-h);
			double xpm = -lanl_f(v, &params_lanl);
			gsl_vector_set(v, ii, s_lanl.x[ii]-h);
			gsl_vector_set(v, jj, s_lanl.x[jj]+h);
			double xmp = -lanl_f(v, &params_lanl);
			gsl_vector_set(v, ii, s_lanl.x[ii]-h);
			gsl_vector_set(v, jj, s_lanl.x[jj]-h);
			double xmm = -lanl_f(v, &params_lanl);
			xx = ((xpp-xpm)-(xmp-xmm))/(4.0*h*h);
			if (xx < 0.0) xx = -xx; /* Is this correct? */
			/* printf("%.3f %.3f %.3f %.3f\n", xpm, xmp, xpp, xmm);*/
			gsl_matrix_set(a,ii,jj,-xx);
			gsl_matrix_set(a,jj,ii,-xx);
		    }
		    /* printf("x%d%d %12g\n", ii, jj, xx); */
		}
	    }
	    invert_matrix(a, ai);
	    printf("lanl_A_err = %.5f;", sqrt(gsl_matrix_get(ai,0,0)));
	    printf(" lanl_a_err = %.5f;", sqrt(gsl_matrix_get(ai,1,1)));
	    printf(" lanl_b_err = %.5f;", sqrt(gsl_matrix_get(ai,2,2)));
	    printf(" lanl_c_err = %.5f;\n", sqrt(gsl_matrix_get(ai,3,3)));

	    printf("C/1e-6 for h=%g\n", h);
	    for (int ii = 0; ii < np_lanl; ii++) {
		for (int jj = 0; jj < np_lanl; jj++) {
		    if (jj >= ii) printf("%7.3f ", gsl_matrix_get(ai,ii,jj)/1e-6);
		    else printf("....... ");
		}
		printf("\n");
	    }
	    gsl_matrix_free(a);
	    gsl_matrix_free(ai);
	} else printf("failed\n");
    }

    if (opt.do_gps) {
	int np_gps = 3;
	pl_params_s *p, params_gps = {.datasets = nn,
				       .ntot = ntot,
				       .ssum = ssum,
				       .initial_val[0] = 0.74185,
				       .initial_val[1] = 1.57013,
				       .initial_val[2] = 1.19125,
				       .initial_step[0] = 1.0,
				       .initial_step[1] = 1.0,
				       .initial_step[2] = 1.0,
				       .initial_step[3] = 1.0};
	p = &params_gps;
	
	for (int j = 0; j < nn; j++) {
	    model_params mp = get_model_params(mtab[j].name);
	    printf("model %d is %s %s\n", j, mp.tag, mtab[j].name);
	    if (mtab[j].binned) {
		p->xmin[j] = mtab[j].bin[0].lower;
	    } else {
		p->xmin[j] = mtab[j].sample[0].mass;
	    }
	    p->data[j] = &mtab[j];
	    p->rho0[j] = mp.rho0;
	    p->vol[j] = mp.vol;
	    p->spline[j] = spline[mp.m_sig_index];
	    p->acc[j] = acc[mp.m_sig_index];
	    p->dspline[j] = dspline[mp.m_sig_index];
	    p->dacc[j] = dacc[mp.m_sig_index];
	}
	gsl_integration_workspace *w 
	    = gsl_integration_workspace_alloc(1000);
	double resultn, error;
	gsl_vector *v = gsl_vector_alloc(np_gps);
	for (int i = 0; i < np_gps; i++) {
	    gsl_vector_set (v, i, p->initial_val[i]);
	}
	ff_params_s ffparams = {.v = v, .p = p};
	gsl_function fn;
	fn.function = &gps_n;
	fn.params = &ffparams;
	for (int j = 0; j < nn; j++) {
	    ffparams.j = j;
	    gsl_integration_qag(&fn, p->xmin[j], 5e16, 0.0, 2e-5, 1000, 6, w, &resultn, &error); 
	    printf("n %9.0f sum %12g xmin %12g ratio %5.3f\n", 
		   p->data[j]->ntot, p->data[j]->sum, p->xmin[j], 
		   resultn/p->data[j]->ntot);
	}
	gsl_integration_workspace_free(w);
	
	minimize_s s_gps = minimize(gps_f, np_gps, &params_gps);
	
	if (!s_gps.status) {
	    printf("GPS for 1/(1+z) = %.03f version %s\nmodels", 
		   opt.expansion_fac, mffit_version);
	    for (int j = 0; j < nn; j++) {
		printf(" %s", get_model_params(mtab[j].name).tag);
	    }
	    printf("\ngps_A = %.5f;", s_gps.x[0]);
	    printf(" gps_a = %.5f;", s_gps.x[1]);
	    printf(" gps_c = %.5f;\n", s_gps.x[2]);
	    printf("gps_L = %.3f;\n", -s_gps.fval);
	    printf("%.5f %.5f %.5f FIT\n", s_gps.x[0], s_gps.x[1], s_gps.x[2]);
	} else printf("failed\n");
    }

    if (opt.do_tinker) {
	int np_tinker = 4;
	pl_params_s *p, params_tinker = {.datasets = nn,
				       .ntot = ntot,
				       .ssum = ssum,
				       .initial_val[0] = 0.2,
				       .initial_val[1] = 1.5,
				       .initial_val[2] = 2.5,
				       .initial_val[3] = 1.19,
				       .initial_step[0] = 0.2,
				       .initial_step[1] = 1.0,
				       .initial_step[2] = 1.0,
				       .initial_step[3] = 1.0};
	p = &params_tinker;

	for (int j = 0; j < nn; j++) {
	    model_params mp = get_model_params(mtab[j].name);
	    printf("model %d is %s %s\n", j, mp.tag, mtab[j].name);
	    if (mtab[j].binned) {
		p->xmin[j] = mtab[j].bin[0].lower;
	    } else {
		p->xmin[j] = mtab[j].sample[0].mass;
	    }
	    p->data[j] = &mtab[j];
	    p->rho0[j] = mp.rho0;
	    p->vol[j] = mp.vol;
	    p->spline[j] = spline[mp.m_sig_index];
	    p->acc[j] = acc[mp.m_sig_index];
	    p->dspline[j] = dspline[mp.m_sig_index];
	    p->dacc[j] = dacc[mp.m_sig_index];
	}
	gsl_integration_workspace *w 
	    = gsl_integration_workspace_alloc(1000);
	double resultn, error;
	gsl_vector *v = gsl_vector_alloc(np_tinker);
	for (int i = 0; i < np_tinker; i++) {
	    gsl_vector_set (v, i, p->initial_val[i]);
	}
	ff_params_s ffparams = {.v = v, .p = p};
	gsl_function fn;
	fn.function = &tinker_n;
	fn.params = &ffparams;
	for (int j = 0; j < nn; j++) {
	    ffparams.j = j;
	    gsl_integration_qag(&fn, p->xmin[j], 5e16, 0.0, 2e-5, 1000, 6, w, &resultn, &error); 
	    printf("n %.0f sum %12g xmin %12g ratio %5.3f\n", 
		   p->data[j]->ntot, p->data[j]->sum, p->xmin[j], 
		   resultn/p->data[j]->ntot);
	}
	gsl_integration_workspace_free(w);
	
	minimize_s s_tinker = minimize(tinker_f, np_tinker, &params_tinker);
	
	if (!s_tinker.status) {
	    printf("Tinker for 1/(1+z) = %.03f version %s\nmodels", 
		   opt.expansion_fac, mffit_version);
	    for (int j = 0; j < nn; j++) {
		printf(" %s", get_model_params(mtab[j].name).tag);
	    }
	    printf("\ntinker_A = %.5f;", s_tinker.x[0]);
	    printf(" tinker_a = %.5f;", s_tinker.x[1]);
	    printf(" tinker_b = %.5f;", s_tinker.x[2]);
	    printf(" tinker_c = %.5f;\n", s_tinker.x[3]);
	    printf("tinker_L = %.3f;\n", -s_tinker.fval);

	    gsl_matrix *a = gsl_matrix_alloc(np_tinker, np_tinker);
	    gsl_matrix *ai = gsl_matrix_alloc(np_tinker, np_tinker);
	    /* Calculate Hessian numerically */
	    double h = 0.0001;
	    for (int ii = 0; ii < np_tinker; ii++) {
		for (int jj = ii; jj < np_tinker; jj++) {
		    gsl_vector_set(v, 0, s_tinker.x[0]);
		    gsl_vector_set(v, 1, s_tinker.x[1]);
		    gsl_vector_set(v, 2, s_tinker.x[2]);
		    gsl_vector_set(v, 3, s_tinker.x[3]);
		    double xx;
		    if (ii == jj) {
			double x0 = -tinker_f(v, &params_tinker);
			gsl_vector_set(v, ii, s_tinker.x[ii]+h);
			double xp = -tinker_f(v, &params_tinker);
			gsl_vector_set(v, ii, s_tinker.x[ii]-h);
			double xm = -tinker_f(v, &params_tinker);
			xx = (xp+xm-2.0*x0)/h;
			/* printf("%.3f %.3f\n", xp, xm); */
			gsl_matrix_set(a,ii,jj,-xx);
		    } else {
			gsl_vector_set(v, ii, s_tinker.x[ii]+h);
			gsl_vector_set(v, jj, s_tinker.x[jj]+h);
			double xpp = -tinker_f(v, &params_tinker);
			gsl_vector_set(v, ii, s_tinker.x[ii]+h);
			gsl_vector_set(v, jj, s_tinker.x[jj]-h);
			double xpm = -tinker_f(v, &params_tinker);
			gsl_vector_set(v, ii, s_tinker.x[ii]-h);
			gsl_vector_set(v, jj, s_tinker.x[jj]+h);
			double xmp = -tinker_f(v, &params_tinker);
			gsl_vector_set(v, ii, s_tinker.x[ii]-h);
			gsl_vector_set(v, jj, s_tinker.x[jj]-h);
			double xmm = -tinker_f(v, &params_tinker);
			xx = ((xpp-xpm)-(xmp-xmm))/(4.0*h*h);
			if (xx < 0.0) xx = -xx; /* Is this correct? */
			/* printf("%.3f %.3f %.3f %.3f\n", xpm, xmp, xpp, xmm);*/
			gsl_matrix_set(a,ii,jj,-xx);
			gsl_matrix_set(a,jj,ii,-xx);
		    }
		    /* printf("x%d%d %12g\n", ii, jj, xx); */
		}
	    }
	    invert_matrix(a, ai);
	    printf("tinker_A_err = %.5f;", sqrt(gsl_matrix_get(ai,0,0)));
	    printf(" tinker_a_err = %.5f;", sqrt(gsl_matrix_get(ai,1,1)));
	    printf(" tinker_b_err = %.5f;", sqrt(gsl_matrix_get(ai,2,2)));
	    printf(" tinker_c_err = %.5f;\n", sqrt(gsl_matrix_get(ai,3,3)));
	    printf("%.5f %.5f %.5f %.5f FIT\n", s_tinker.x[0], s_tinker.x[1], s_tinker.x[2], s_tinker.x[3]);

	    printf("C/1e-6 for h=%g\n", h);
	    for (int ii = 0; ii < np_tinker; ii++) {
		for (int jj = 0; jj < np_tinker; jj++) {
		    if (jj >= ii) printf("%7.3f ", gsl_matrix_get(ai,ii,jj)/1e-6);
		    else printf("....... ");
		}
		printf("\n");
	    }
	    gsl_matrix_free(a);
	    gsl_matrix_free(ai);
	} else printf("failed\n");
    }

#if 0
    int np_jenkins = 3;
    pl_params_s *p, params_jenkins = {.datasets = nn,
				      .ntot = ntot,
				      .ssum = ssum,
				      .initial_val[0] = 0.315,
				      .initial_val[1] = 0.61,
				      .initial_val[2] = 3.8,
				      .initial_step[0] = 0.25,
				      .initial_step[1] = 0.25,
				      .initial_step[2] = 2.0};
    p = &params_jenkins;

    for (int j = 0; j < nn; j++) {
	model_params mp = get_model_params(argv[j+optind]);
	printf("model %d is %s %s\n", j, mp.tag, argv[j+optind]);
	p->n[j] = mn[j];
	p->sum[j] = sum[j];
	p->xmin[j] = mtab[j][0];
	p->data[j] = mtab[j];
	p->rho0[j] = mp.rho0;
	p->vol[j] = mp.vol;
	p->spline[j] = spline[mp.m_sig_index];
	p->acc[j] = acc[mp.m_sig_index];
	p->dspline[j] = dspline[mp.m_sig_index];
	p->dacc[j] = dacc[mp.m_sig_index];
    }
    gsl_integration_workspace *w 
	= gsl_integration_workspace_alloc(1000);
    double result, resultn, error;
    gsl_vector *v = gsl_vector_alloc(np_jenkins);
    for (int i = 0; i < np_jenkins; i++) {
	gsl_vector_set (v, i, p->initial_val[i]);
    }
    ff_params_s ffparams = {.v = v, .p = p};
    gsl_function f, fn;
    f.function = &jenkins_ff;
    f.params = &ffparams;
    fn.function = &jenkins_n;
    fn.params = &ffparams;
    ffparams.j = 0;
    for (int j = 0; j < nn; j++) {
	ffparams.j = j;
	gsl_integration_qag(&f, p->xmin[j], 2e16, 0, 2e-5, 1000, 6, w, &result, &error); 
	gsl_integration_qag(&fn, p->xmin[j], 2e16, 0, 2e-5, 1000, 6, w, &resultn, &error); 
	printf("n %9d sum %12g xmin %12g ratio %5.3f nratio %12g\n", 
	       p->n[j], p->sum[j], p->xmin[j], 
	       p->vol[j]*result/p->sum[j], p->vol[j]*resultn/p->n[j]);
    }
    gsl_integration_workspace_free(w);

    minimize_s s_jenkins = minimize(jenkins_f, np_jenkins, &params_jenkins);
    
    if (!s_jenkins.status) {
	printf("JENKINS for 1/(1+z) = %.03f models ", opt.expansion_fac);
	for (int j = 0; j < nn; j++) {
	    printf("%s ", get_model_params(argv[j+optind]).tag);
	}
	printf("\na = %g b = %g c = %g L = %.10g\n",
	       s_jenkins.x[0], s_jenkins.x[1], s_jenkins.x[2],
	       -s_jenkins.fval);
    } else printf("failed\n");
#endif

    if (opt.do_bhat) {
	int np_bhat = 4;
	pl_params_s *p, params_bhat = {.datasets = nn,
				       .ntot = ntot,
				       .ssum = ssum,
				       .initial_val[0] = 0.333,
				       .initial_val[1] = 0.788,
				       .initial_val[2] = 0.807,
				       .initial_val[3] = 1.795,
				       .initial_step[0] = 0.25,
				       .initial_step[1] = 0.5,
				       .initial_step[2] = 0.5,
				       .initial_step[3] = 1.0};
	p = &params_bhat;
	
	for (int j = 0; j < nn; j++) {
	    model_params mp = get_model_params(argv[j+optind]);
	    printf("model %d is %s %s\n", j, mp.tag, argv[j+optind]);
	    p->xmin[j] = mtab[j].sample[0].mass;
	    p->data[j] = &mtab[j];
	    p->rho0[j] = mp.rho0;
	    p->vol[j] = mp.vol;
	    p->spline[j] = spline[mp.m_sig_index];
	    p->acc[j] = acc[mp.m_sig_index];
	    p->dspline[j] = dspline[mp.m_sig_index];
	    p->dacc[j] = dacc[mp.m_sig_index];
	}
	gsl_integration_workspace *w 
	    = gsl_integration_workspace_alloc(1000);
	double resultn, error;
	gsl_vector *v = gsl_vector_alloc(np_bhat);
	for (int i = 0; i < np_bhat; i++) {
	    gsl_vector_set (v, i, p->initial_val[i]);
	}
	ff_params_s ffparams = {.v = v, .p = p};
	gsl_function fn;
	fn.function = &bhat_n;
	fn.params = &ffparams;
	for (int j = 0; j < nn; j++) {
	    ffparams.j = j;
	    gsl_integration_qag(&fn, p->xmin[j], 5e16, 0.0, 2e-5, 1000, 6, w, &resultn, &error); 
	    printf("n %.0f sum %12g xmin %12g ratio %5.3f\n", 
		   p->data[j]->ntot, p->data[j]->sum, p->xmin[j], 
		   resultn/p->data[j]->ntot);
	}
	gsl_integration_workspace_free(w);
	
	minimize_s s_bhat = minimize(bhat_f, np_bhat, &params_bhat);
	
	if (!s_bhat.status) {
	    printf("Bhat for 1/(1+z) = %s models ", argv[1]);
	    for (int j = 0; j < nn; j++) {
		printf("%s ", get_model_params(argv[j+2]).tag);
	    }
	    printf("\nbhat_A = %g;", s_bhat.x[0]);
	    printf(" bhat_a = %g;", s_bhat.x[1]);
	    printf(" bhat_p = %g;", s_bhat.x[2]);
	    printf(" bhat_q0 = %g;\n", s_bhat.x[3]);
	    printf("bhat_L = %.3f;\n", -s_bhat.fval);
	} else printf("failed\n");
    }

#if 0
    int np = 1;
    pl_params_s params = {.n = mn, .data = mtab, 
			  .xmin[0] = mtab[0][0], 
			  .xmin[1] = mtab[1][0], 
			  .norm = 1.0,
			  .spline = spline, .acc = acc,
			  .dspline = dspline, .dacc = dacc,
			  .initial_val[0] = 1.1,
			  .initial_val[1] = 1.4e-5,
			  .initial_step[0] = 1.1,
			  .initial_step[1] = 1.4e-5};
    minimize_s s = minimize(pl_f, np, &params);

    if (!s.status)
	printf("alpha = %g L = %.10g\n", s.x[0], -s.fval);
    else printf("failed\n");
#endif

#if 0
    int np_gpl = 2;
    pl_params_s params_gpl = {.n = mn, .data = mtab, 
			      .xmin = mtab[0], .norm = 1.0,
			      .spline = spline, .acc = acc,
			      .dspline = dspline, .dacc = dacc,
			      .initial_val[0] = 1.0+1.0/logmean,
			      .initial_val[1] = 1.0/mean,
			      .initial_step[0] = 1.0+1.0/logmean,
			      .initial_step[1] = 1.0/mean};
    minimize_s s_gpl = minimize(gpl_f, np_gpl, &params_gpl);
    
    if (!s_gpl.status)
	printf("alpha = %g lambda = %g L = %.10g\n", 
	       s_gpl.x[0], s_gpl.x[1], -s_gpl.fval);
    else printf("failed\n");
#endif

    exit(0);
}


minimize_s minimize(double (*f)(const gsl_vector *v, void *params),
		    int np, pl_params_s *par)
{
    minimize_s ret;
     
    const gsl_multimin_fminimizer_type *T = 
	gsl_multimin_fminimizer_nmsimplex2;
    gsl_multimin_fminimizer *s = NULL;
    gsl_vector *ss, *x;
    gsl_multimin_function minex_func;
    
    size_t iter = 0;
    int status;
    double size;
    
    x = gsl_vector_alloc (np);
    ss = gsl_vector_alloc (np);
    for (int i = 0; i < np; i++) {
	gsl_vector_set (x, i, par->initial_val[i]);
	gsl_vector_set (ss, i, par->initial_step[i]);
    }
    
    /* Initialize method and iterate */
    minex_func.n = np;
    minex_func.f = f;
    minex_func.params = par;
    
    s = gsl_multimin_fminimizer_alloc (T, np);
    gsl_multimin_fminimizer_set (s, &minex_func, x, ss);
     
    do {
	iter++;
	status = gsl_multimin_fminimizer_iterate(s);
	if (status) break;
	size = gsl_multimin_fminimizer_size (s);
	status = gsl_multimin_test_size (size, (iter < 400) ? 1e-6 : 2e-5);
#if 1
	if (status == GSL_SUCCESS) printf ("converged to minimum at\n");
	printf ("%5ld %8.5f %8.5f %8.5f %8.5f f() = %14.4f size = %.3g\n", 
		iter, 
		gsl_vector_get (s->x, 0), 
		np > 1 ? gsl_vector_get (s->x, 1) : 0, 
		np > 2 ? gsl_vector_get (s->x, 2) : 0, 
		np > 3 ? gsl_vector_get (s->x, 3) : 0, 
		s->fval, size);
#endif
    }
    while (status == GSL_CONTINUE && iter < 1000);
    for (int i = 0; i < np; i++) {
	ret.x[i] = gsl_vector_get (s->x, i);
    }
    ret.fval = s->fval;
    ret.status = status;
    
    gsl_vector_free(x);
    gsl_vector_free(ss);
    gsl_multimin_fminimizer_free (s);
    return ret;
}

/*
Normalization

Integral of f from xmin to Infinity equals mean = sum(mass)/n
  
 */
