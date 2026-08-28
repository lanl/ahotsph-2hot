/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "macr.h"
#include "error.h"
#include "Malloc.h"
#include "singlio.h"
#include "SDF.h"
#include "cosmo.h"
#include "version.h"
#include <gsl/gsl_spline.h>
#include <gsl/gsl_integration.h>

#define MAXPSTABLE 4096

#ifndef M_PI
#define	M_PI	3.14159265358979323846
#endif

static cosmology cosmo;
static double Ktab[MAXPSTABLE];	/* h/Mpc */
static double Pstab[MAXPSTABLE];
static gsl_spline *D2spline;
static gsl_interp_accel *D2acc;
static int Ntable;
static double Sigma_tophat_r;		/* Mpc/h */
static double Sigma_gaussian_r;		/* Mpc/h */
static double Xi_r;			/* Mpc/h */

void read_ps(char *ps_file, double *ktab, double *pstab, int *ntable,
	     double *min_k, double *max_k)
{
    FILE *fp;
    double log10k, log10ps;
    char line[256];

    Fopen(fp, ps_file, "r");
    *ntable = 0;
    while (fgets(line, sizeof(line), fp)) {
	if (line[0] == '#') continue;
	if (sscanf(line, "%lg %lg\n", &log10k, &log10ps) != 2) 
	    Error("Did not parse %s", line);
	/* k from table is h/Mpc */
	ktab[*ntable] = log10k;
	pstab[*ntable] = log10ps;
      (*ntable)++;
      if (*ntable >= MAXPSTABLE) Error("Not enough space in ps table\n");
    }
    Fclose(fp);

    *min_k = pow(10.0, ktab[0]);
    *max_k = pow(10.0, ktab[*ntable-1]);
}

double
delta2(double kh)
{
  double logd2 = gsl_spline_eval(D2spline, log10(kh), D2acc);
  return pow(10.0, logd2);
}

double
sigma_tophat(double kh, void *params)
{
  double x = kh*Sigma_tophat_r;
  double window = 3.0*(sin(x)-x*cos(x))/(x*x*x);
  return delta2(kh) * window * window / kh; 
}

double
sigma_gaussian(double kh, void *params)
{
  double x = kh*Sigma_gaussian_r;
  double window = exp(-x*x);
  return delta2(kh) * window * window / kh; 
}

double
xi_integrand(double kh, void *params)
{
  double x = kh*Xi_r;
  double window = sin(x)/x;
  return delta2(kh) * window / kh;
}

double
Reed_G1(double sigma)
{
    double val = pow(log(1.0/sigma)-0.4,2.0);
    return exp(-val/(2.0*0.6*0.6));
}

double
Reed_G2(double sigma)
{
    double val = pow(log(1.0/sigma)-0.75,2.0);
    return exp(-val/(2.0*0.2*0.2));
}

int
main(int argc, char *argv[])
{
    int i;
    double min_k, max_k;
    double sigma2_gaussian, xi;
    double mass_tophat_r;
    double sigma, sigmaplus, sigmaminus, n_eff;
    double massplus, massminus, ds_dm;
    int decade;
    int e;
    char ps_file[256];
    double critical_density, critical_density_Mpc_h;
    double rho0;
    double redshift, a;
    double delta_c;
    double ST_A, ST_a, ST_p;
    double LANL_A, LANL_a, LANL_b, LANL_c;
    double Tinker_A, Tinker_a, Tinker_b, Tinker_c, Tinker_alpha;
    double Bhat_A, Bhat_a, Bhat_p, Bhat_q0;
    double Reed_A, Reed_a, Reed_b, Reed_p;
    double f_PS, f_ST, f_Jenkins, f_LANL, f_Bhat, f_Reed, f_Tinker;
    double igf;
    int ic_version;
    int use_tbl;
    struct cosmo_s cosmo1;
    SDF *sdfp;
    FILE *outfp;
    char outfile[256];

    memset(&cosmo, 0, sizeof(cosmo));
    memset(&cosmo1, 0, sizeof(cosmo1));
    if (argc != 3 && argc != 4) {
	fprintf(stderr, "Usage: ps_file.dat fft.ctl [a]\n");
	exit(1);
    }
    if (argc == 3) a = 1.0;
    else a = atof(argv[3]);
    redshift = (1.0/a)-1.0;

    if (!(sdfp = SDFopen(0, argv[2])))
	SinglError("Sorry, couldn't SDFopen %s\n%s\n", argv[1], SDFerrstring);

    SDFgetintOrDefault(sdfp, "ic_version", &ic_version, 1);
    SDFgetintOrDefault(sdfp, "version", &ic_version, ic_version);
    SDFgetintOrDefault(sdfp, "use_tbl", &use_tbl, 1);
    if (ic_version == 1) {
	SDFgetdoubleOrDie(sdfp, "h_100",  &cosmo.h_100);
	cosmo.H0 = cosmo.h_100*0.1*(one_Gyr/one_kpc); /* in Gyr^-1 */
	SDFgetdoubleOrDefault(sdfp, "Gnewt",  &cosmo.Gnewt, GNEWT);
	cosmo.Omega0 = 1.0;
	cosmo.Omega0_r = 0.0;
	SDFgetdoubleOrDie(sdfp, "Omega0", &cosmo.Omega0_m);
	SDFgetdoubleOrDie(sdfp, "Lambda_prime",  &cosmo.Omega0_lambda);
	/* Need these to calculate growth factor later */
	cosmo1.H0 = cosmo.H0;
	cosmo1.Gnewt = cosmo.Gnewt;
	cosmo1.Omega0 = cosmo.Omega0_m;
	cosmo1.Omega_m = cosmo.Omega0_m;
	cosmo1.Lambda = cosmo.Omega0_lambda;
    } else if (ic_version == 2) {
	SDFgetdoubleOrDie(sdfp, "h_100",  &cosmo.h_100);
	cosmo.H0 = cosmo.h_100*0.1*(one_Gyr/one_kpc); /* in Gyr^-1 */
	cosmo.a = a;
	SDFgetdoubleOrDefault(sdfp, "Gnewt",  &cosmo.Gnewt, GNEWT);
	SDFgetdoubleOrDie(sdfp, "Omega0",  &cosmo.Omega0);
	SDFgetdoubleOrDie(sdfp, "Omega0_r", &cosmo.Omega0_r);
	SDFgetdoubleOrDie(sdfp, "Omega0_m", &cosmo.Omega0_m);
	SDFgetdoubleOrDie(sdfp, "Omega0_lambda",  &cosmo.Omega0_lambda);
    } else Error("Bad file version\n");

    SDFclose(sdfp);

    strncpy(ps_file, argv[1], sizeof(ps_file));
    strncpy(outfile, ps_file, sizeof(outfile));
    outfile[strlen(ps_file)-4] = '\0';
    sprintf(outfile, "%s_%.04f_sigma.dat", outfile, a);
    Fopen(outfp, outfile, "w");

    fprintf(stderr, "%s %.04f\n", outfile, a);

    delta_c = 1.68647;
    critical_density = (3./(8.*M_PI*cosmo.Gnewt))*cosmo.H0*cosmo.H0;
    rho0 = cosmo.Omega0_m*critical_density;
    critical_density_Mpc_h = critical_density*1e9/(cosmo.h_100*cosmo.h_100);
    
    ST_A = 0.3222;
    ST_a = 0.707;
    ST_p = 0.3;
    
#if 0 
    /* 2006 */
    LANL_A = 0.72646;
    LANL_a = 1.6394;
    LANL_b = 0.25436;
    LANL_c = 1.2023;
#else
    LANL_A = 0.741678;
    LANL_a = 1.57558;
    LANL_b = 0.282673;
    LANL_c = 1.19174;
#endif

#if 1
    /* from code */
    Tinker_A = 0.1858659;
    Tinker_a = 1.466904;
    Tinker_b = 2.571104;
    Tinker_c = 1.193958;
#else
    /* from paper */
    Tinker_A = 0.186;
    Tinker_a = 1.47;
    Tinker_b = 2.57;
    Tinker_c = 1.19;
#endif

#define DELTA_HALO 200.0
    if (redshift > 0.0) {
	double r = (redshift < 3.0) ? redshift : 3.0;
	Tinker_A *= pow(1.0+r, -0.14);
	Tinker_a *= pow(1.0+r, -0.06);
	Tinker_alpha = -pow(0.75/log10(DELTA_HALO/75.0), 1.2);
	Tinker_alpha = pow(10.0, Tinker_alpha);
	Tinker_b *= pow(1.0+r, -Tinker_alpha);
    }

    Bhat_A = 0.333/pow(1.0+redshift, 0.11);
    Bhat_a = 0.788/pow(1.0+redshift, 0.01);
    Bhat_p = 0.807;
    Bhat_q0 = 1.795;

    Reed_A = 0.3222;
    Reed_a = 0.707;
    Reed_b = 0.7648;
    Reed_p = 0.3;

    read_ps(ps_file, Ktab, Pstab, &Ntable, &min_k, &max_k);

    /* Scale power spectrum to desired redshift */
    if (ic_version == 1) {
	igf = growthfac_from_Z(&cosmo1, 0.0)
	    / growthfac_from_Z(&cosmo1, redshift);
    } else if (ic_version == 2) {
	if (use_tbl) tbl_init(&cosmo, "cosmology.tbl");
	else cosmo1_init(&cosmo);
	igf = cosmo.growthfac_at_z(&cosmo, 0.0)
	    / cosmo.growthfac_at_z(&cosmo, redshift);
    } else Error("Unknown ic_version\n");
    for (i = 0; i < Ntable; i++) {
	Pstab[i] = log10(pow(10.0, Pstab[i])/(igf*igf));
    }

    /* Interpolate tabulated power spectrum */
    D2acc = gsl_interp_accel_alloc();
    D2spline = gsl_spline_alloc(gsl_interp_cspline, Ntable);
    gsl_spline_init(D2spline, Ktab, Pstab, Ntable);

    fprintf(outfp, "# Omega0_m %.6f, Lambda %.6f, redshift %.6f, h %.6f, growth_fac %.6f\n", 
	    cosmo.Omega0_m, cosmo.Omega0_lambda, redshift, cosmo.h_100, igf);
    fprintf(outfp, "# Power spectrum read from %s, %d entries\n# min_k = %g, max_k = %g\n", 
	    ps_file, Ntable, min_k, max_k);
    fprintf(outfp, "# critical_density %.8g rho0 %.8g [(Msun/h)/(Mpc/h)^3]\n", 
	    critical_density_Mpc_h*1e10, critical_density_Mpc_h*cosmo.Omega0_m*1e10);

    fprintf(outfp, "# Version %s %s %s\n", Version, Compiled_date, Compiled_time);

    fprintf(outfp, "# r*h(Mpc) mass*h/1e10 sigma_tophat sigma_gaussian ln(1/sigma) f_PS f_ST f_Jenkins f_LANL f_Bhat f_Reed f_Tinker ds_dm xi\n");
    fflush(outfp);

    gsl_set_error_handler_off();
    gsl_integration_workspace *w = gsl_integration_workspace_alloc(1000);
    double s2, err, ftol = 2e-7;
    gsl_function fn, fng, fnx;
    fn.function = &sigma_tophat;
    fn.params = &Sigma_tophat_r;
    fng.function = &sigma_gaussian;
    fng.params = &Sigma_gaussian_r;
    fnx.function = &xi_integrand;
    fnx.params = &Xi_r;

    for (decade = -2; decade < 3; decade++) {
	int laste = (decade == 2) ? 300 : 1000;
	for (e = 0; e < laste; e++) {
	    /* n_effapprox dlog = 0.01 */
	    Sigma_tophat_r = pow(10., decade) * pow(10., e/1000.0 + 0.000309);
	    Sigma_tophat_r *= pow(10.0, .01/6.0);
	    gsl_integration_qag(&fn, min_k, max_k, 0.0, ftol, 1000, 3, w, &s2, &err); 
	    sigmaplus = sqrt(s2);

	    Sigma_tophat_r = pow(10., decade) * pow(10., e/1000.0 + 0.000309);
	    Sigma_tophat_r *= pow(10.0, -.01/6.0);
	    gsl_integration_qag(&fn, min_k, max_k, 0.0, ftol, 1000, 3, w, &s2, &err); 
	    sigmaminus = sqrt(s2);

	    /* neffapprox based on calc from genmf.f, D. Reed 6/2006 */
	    n_eff = 6.0*fabs(log(1./sigmaplus)-log(1./sigmaminus)) / (0.01*log(10.0)) - 3.0;
	    Sigma_tophat_r = pow(10., decade) * pow(10., e/1000.0 + 0.000309);
	    gsl_integration_qag(&fn, min_k, max_k, 0.0, ftol, 1000, 3, w, &s2, &err); 
	    sigma = sqrt(s2);
	    mass_tophat_r = (4.0*M_PI/3.0)*rho0
		*pow(Sigma_tophat_r*1000.0/cosmo.h_100, 3.0);
	    massplus = mass_tophat_r*1.01;
	    massminus = mass_tophat_r*0.99;
	    Sigma_tophat_r = pow(3.0*massplus/(4.0*M_PI*rho0),1.0/3.0);
	    Sigma_tophat_r *= cosmo.h_100/1000.0;
	    gsl_integration_qag(&fn, min_k, max_k, 0.0, ftol, 1000, 3, w, &s2, &err); 
	    sigmaplus = sqrt(s2);
	    Sigma_tophat_r = pow(3.0*massminus/(4.0*M_PI*rho0),1.0/3.0);
	    Sigma_tophat_r *= cosmo.h_100/1000.0;
	    gsl_integration_qag(&fn, min_k, max_k, 0.0, ftol, 1000, 3, w, &s2, &err); 
	    sigmaminus = sqrt(s2);

	    ds_dm = -(mass_tophat_r/sigma)*(sigmaplus-sigmaminus)/(massplus-massminus);

	    Sigma_tophat_r = pow(10., decade) * pow(10., e/1000.0 + 0.000309);
	    if (gsl_integration_qag(&fn, min_k, max_k, 0.0, ftol, 1000, 3, w, &s2, &err)) {
		fprintf(stderr, "gsl_integration_failed at r=%g\n", Sigma_tophat_r);
		continue;
	    }
	    sigma = sqrt(s2);
	    Sigma_gaussian_r = Sigma_tophat_r / 3.0;
	    Xi_r = Sigma_tophat_r;
#if 0
	    if (gsl_integration_qag(&fng, min_k, max_k, 0.0, ftol, 1000, 3, w, &sigma2_gaussian, &err))
		Error("gaussian gsl_integration_failed\n");
	    if (gsl_integration_qag(&fnx, min_k, max_k, 0.0, 5e-6, 1000, 3, w, &xi, &err))
		Error("xi gsl_integration_failed\n");
#else
	    sigma2_gaussian = 0.0;
	    xi = 0.0;
#endif

	    mass_tophat_r = (4.0*M_PI/3.0)*rho0
		*pow(Sigma_tophat_r*1000.0/cosmo.h_100, 3.0);
	    
	    f_PS = sqrt(2.0/M_PI)*(delta_c/sigma)*
		exp(-delta_c*delta_c/(2.0*sigma*sigma));
	    
	    f_ST = ST_A * sqrt(2.0*ST_a/M_PI)*
		(1.0+pow(sigma*sigma/(ST_a*delta_c*delta_c), ST_p))*
		(delta_c/sigma)*exp(-ST_a*delta_c*delta_c/(2.0*sigma*sigma));
	    
	    f_LANL = LANL_A * (pow(sigma, -LANL_a) + LANL_b)
		*exp(-LANL_c/(sigma*sigma));
	    
	    f_Tinker = Tinker_A * (pow(sigma/Tinker_b, -Tinker_a) + 1.0)
		*exp(-Tinker_c/(sigma*sigma));
	    
	    f_Jenkins = 0.315*exp(-pow(fabs(log(1.0/sigma)+0.61),3.8));

	    f_Bhat = Bhat_A * sqrt(2.0/M_PI)*
		(1.0+pow(sigma*sigma/(Bhat_a*delta_c*delta_c), Bhat_p))*
		pow(delta_c*sqrt(Bhat_a)/sigma, Bhat_q0)*
		exp(-Bhat_a*delta_c*delta_c/(2.0*sigma*sigma));

	    f_Reed = Reed_A * sqrt(2.0*Reed_a/M_PI)*
		(1.0+pow(sigma*sigma/(Reed_a*delta_c*delta_c), Reed_p) 
		 + 0.6 * Reed_G1(sigma) + 0.4 * Reed_G2(sigma))*(delta_c/sigma)*
		exp(-Reed_b*delta_c*delta_c/(2.0*sigma*sigma))*
		exp(-(0.03/pow(n_eff+3.0,2.0)) * pow(delta_c/sigma, 0.6));
	    
	    fprintf(outfp, "%14.10lg %16.8g %14.10lg %14.10lg %14.10lg %14.10lg %14.10lg %14.10lg %14.10lg %14.10lg %14.10lg %14.10lg %14.10lg %14.10lg\n", 
		    Sigma_tophat_r, mass_tophat_r*cosmo.h_100,
		    sigma, sqrt(sigma2_gaussian), log(1.0/sigma), f_PS, f_ST, 
		    f_Jenkins, f_LANL, f_Bhat, f_Reed, f_Tinker, ds_dm, xi);
	}
    }
    Fclose(outfp);
    
    exit(0);
}
