/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

/* Make mock galaxy catalog from halos */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include "SDF.h"
#include "SDFread.h"
#include "SDFwrite.h"
#include "Malloc.h"
#include "stk.h"
#include "mpmy.h"
#include "singlio.h"
#include "version.h"

#define NDIM 3
/* Rockstar values */
#define CRITICAL_DENSITY 2.77519737e11 // 3H^2/8piG in (Msun / h) / (Mpc / h)^3
#define Gc (4.30117902e-9) //Actually, Gc * (Msun / Mpc) in (km/s)^2

#define SQRT3 1.7320508075688772

typedef struct  {
    float mass;			/* mass of halo */
    float r;			/* radius of halo */
    float r200b;
    float rvir;
    float rs;			/* NFW scale radius */
    float vrms;			/* rms particle velocity */
    float pos[NDIM];		/* position of halo */
    float vel[NDIM];		/* velocity of halo */
    int64_t pid;		/* parent pid */
} __attribute__ ((packed)) halo;

typedef struct  {
    float pos[NDIM];		/* position of galaxy */
    float vel[NDIM];		/* velocity of galaxy */
} __attribute__ ((packed)) galaxy;

#define GALAXYDESC \
"struct {\n\
    float x, y, z;		/* position of galaxy */\n\
    float vx, vy, vz;		/* velocity of galaxy */\n\
}"

typedef struct options_s {
    double M_cut, M1, M_min;
    double sigma_logM;
    double alpha, Dgamma, Dv;
    double cvir_fac;
    int seed;
    double delta_halo;
    double Omega0_m;
    double box_size;
    double x_min, y_min, z_min;
    double x_max, y_max, z_max;
    char model[64];
    char density_profile[64];
    char velocity_distribution[64];
    char mass_name[64];
    char radius_name[64];
    char parent_name[64];
    char hdr[FILENAME_MAX];
    char in[FILENAME_MAX];
    char out[FILENAME_MAX];
} options_s;

#define scan(s, v, f) sscanf(s, #v "=" #f, &opt->v)
#define scans(s, v, f) sscanf(s, #v "=" #f, opt->v)

void
parse_opt(int argc, char *argv[], options_s *opt)
{
    for (int i = 1; i < argc; i++) {
	char *p = argv[i];
	int ret;
	ret = 0;
	ret += scan(p, M_cut, %lf);
	ret += scan(p, M1, %lf);
	ret += scan(p, M_min, %lf);
	ret += scan(p, sigma_logM, %lf);
	ret += scan(p, alpha, %lf);
	ret += scan(p, Dgamma, %lf);
	ret += scan(p, Dv, %lf);
	ret += scan(p, cvir_fac, %lf);
	ret += scan(p, seed, %d);
	ret += scan(p, Omega0_m, %lf);
	ret += scan(p, delta_halo, %lf);
	ret += scan(p, box_size, %lf);
	ret += scan(p, x_min, %lf);
	ret += scan(p, x_min, %lf);
	ret += scan(p, y_min, %lf);
	ret += scan(p, z_min, %lf);
	ret += scan(p, x_max, %lf);
	ret += scan(p, y_max, %lf);
	ret += scan(p, z_max, %lf);
	ret += scans(p, model, %64s);
	ret += scans(p, density_profile, %64s);
	ret += scans(p, velocity_distribution, %64s);
	ret += scans(p, mass_name, %64s);
	ret += scans(p, radius_name, %64s);
	ret += scans(p, parent_name, %64s);
	ret += scans(p, hdr, %256s);
	ret += scans(p, in, %256s);
	ret += scans(p, out, %256s);
	if (ret != 1) singlPrintf("Failed to parse %s\n", p);
    }
}

SDF *
ReadData(char *name, halo **htab, int64_t *gnobj, int *nobj, options_s *opt)
{
    int massconf, xconf, yconf, zconf;
    int rconf, rsconf, r200bconf, rvirconf, vrmsconf;
    int vxconf, vyconf, vzconf;
    int pidconf;
    SDF *sdfp;

    singlPrintf("Reading %s\n", name);
    sdfp = SDFreadf64(opt->hdr, name, (void *)htab, gnobj, nobj, sizeof(halo),
		      opt->mass_name, offsetof(halo, mass), &massconf,
		      opt->radius_name, offsetof(halo, r), &rconf,
		      "r200b", offsetof(halo, r200b), &r200bconf,
		      "rvir", offsetof(halo, rvir), &rvirconf,
		      "rs", offsetof(halo, rs), &rsconf,
		      "vrms", offsetof(halo, vrms), &vrmsconf,
		      "x", offsetof(halo, pos[0]), &xconf,
		      "y", offsetof(halo, pos[1]), &yconf,
		      "z", offsetof(halo, pos[2]), &zconf,
		      "vx", offsetof(halo, vel[0]), &vxconf,
		      "vy", offsetof(halo, vel[1]), &vyconf,
		      "vz", offsetof(halo, vel[2]), &vzconf,
		      opt->parent_name, offsetof(halo, pid), &pidconf,
		      NULL);

    if (massconf==0 || rconf || xconf==0 || yconf==0 || zconf==0) {
	SinglError("Could not find %s %s %s %s %s in data file!\n",
		   (massconf==0)? opt->mass_name : "",
		   (rconf==0)? opt->radius_name : "",
		   (xconf==0)? "x" : "",
		   (yconf==0)? "y" : "",
		   (zconf==0)? "z" : "");
    }
    if (rsconf == 0 || r200bconf == 0 || rvirconf == 0) {
	SinglError("Missing r\n");
    }
    if (vrmsconf == 0) {
	SinglError("Missing vrms\n");
    }
    if (vxconf != vyconf || vxconf != vzconf) {
	SinglError("Missing velocity components!\n");
    }
    if (pidconf == 0) {
	SinglError("No %s in file.\n", opt->parent_name);
    }
    singlPrintf("Data read, gnobj=%ld\n", *gnobj);
    
    return sdfp;
}

float
spherical_rand(float pos[NDIM], const gsl_rng *rng)
{
    float rsq;
    do {
	rsq = 0.0f;
	for (int k = 0; k < NDIM; k++) {
	    pos[k] = gsl_rng_uniform(rng)*2.0f - 1.0f;
	    rsq += pos[k] * pos[k];
	}
    } while (rsq > 1.0f);
    return rsq;
}

double
N_cen(double halo_m, const options_s *opt)
{
    /* HOD.x case 2 */
    double arg = (log10(halo_m)-log10(opt->M_min))/opt->sigma_logM;
    return 0.5 * (1.0 + erf(arg));
}

double
N_sat(double halo_m, double ncen_expected, const options_s *opt)
{
    if (!strcmp(opt->model, "manera12")) {
	/* HOD.x case 2 */
	return (halo_m >= opt->M_cut)
	    ? ncen_expected * pow(((halo_m-opt->M_cut)/opt->M1), opt->alpha) 
	    : 0.0;
    } else if (!strcmp(opt->model, "reddick13")) {
	/* HOD.x not quite case 3 or 6 */
	return ncen_expected * exp(-opt->M_cut/halo_m) * pow((halo_m/opt->M1), opt->alpha);
    } else Error("Don't know what to do\n");
}

void
isothermal_density(float halo_r, const float *halo_pos, const options_s *opt, const gsl_rng *rng, float *pos)
{
    /* Simple isotropic isothermal distribution */
    double rr[NDIM];
    gsl_ran_dir_3d(rng, &rr[0], &rr[1], &rr[2]);
    float scale = halo_r * gsl_rng_uniform(rng);
    for (int k = 0; k < NDIM; k++) {
	pos[k] = halo_pos[k] + rr[k] * scale;
	if (pos[k] >= opt->box_size) pos[k] -= opt->box_size;
	if (pos[k] < 0.0) pos[k] += opt->box_size;
    }
}

double
nfw(double r, double rs)
{
    double c = r / rs;
    double one_plus_c = 1.0 + c;
    return(rs / (r * one_plus_c * one_plus_c));
}

void
nfw_density(float halo_r, float r, float rs, const float *halo_pos, const options_s *opt, const gsl_rng *rng, float *pos)
{
    double rr[NDIM], u, ur, mr;
    gsl_ran_dir_3d(rng, &rr[0], &rr[1], &rr[2]);
    rs /= opt->cvir_fac;

    /* NFW density via rejection method */
    double mm = nfw(rs, rs) * rs * rs;
    do {
	u = gsl_rng_uniform(rng);
	ur = gsl_rng_uniform(rng) * r;
	mr = nfw(ur, rs) * ur * ur / mm;
    } while (u > mr);

    for (int k = 0; k < NDIM; k++) {
	pos[k] = halo_pos[k] + rr[k] * ur;
	if (pos[k] >= opt->box_size) pos[k] -= opt->box_size;
	if (pos[k] < 0.0) pos[k] += opt->box_size;
    }
}

void
constant_velocity(float vcirc, const float *halo_vel, const options_s *opt, const gsl_rng *rng, float *vel)
{
    double vv[NDIM];
    gsl_ran_dir_3d(rng, &vv[0], &vv[1], &vv[2]);
    for (int k = 0; k < NDIM; k++) {
	vel[k] = halo_vel[k] + vv[k] * vcirc;
    }
}

void
gaussian_velocity(float vrms, const float *halo_vel, const options_s *opt, const gsl_rng *rng, float *vel)
{
    double vv[NDIM];
    gsl_ran_dir_3d(rng, &vv[0], &vv[1], &vv[2]);
    for (int k = 0; k < NDIM; k++) {
	vel[k] = halo_vel[k] + gsl_ran_gaussian(rng, vrms/SQRT3) * vv[k];
    }
}

void
exponential_velocity(float vrms, const float *halo_vel, const options_s *opt, const gsl_rng *rng, float *vel)
{
    /* zurek94 http://adsabs.harvard.edu/abs/1994ApJ...431..559Z */
    /* Need to add infall term */
    double vv[NDIM];
    gsl_ran_dir_3d(rng, &vv[0], &vv[1], &vv[2]);
    for (int k = 0; k < NDIM; k++) {
	vel[k] = halo_vel[k] + gsl_ran_laplace(rng, vrms/SQRT3) * vv[k]; /* correct factor? */
    }
}

void
process_halos(const halo *halos, int nobj, const options_s *opt, const gsl_rng *rng, Stk *stk)
{
    for (const halo *h = halos; h < halos + nobj; h++) {
	if (h->pid != -1) continue;
	if (h->mass == 0.0f) continue;
	double halo_m = h->mass;
	double vcirc = sqrt(Gc*halo_m);

	double halo_r = 0.0;
	if (!strcmp(opt->mass_name, "m200b"))
	    halo_r = h->r200b;
	else if (!strcmp(opt->mass_name, "mvir"))
	    halo_r = h->rvir;	/* I don't understand why halo->rvir is slightly different from halo->r */
	else
	    halo_r = pow(3.0 * halo_m / (4.0 * M_PI * opt->delta_halo * opt->Omega0_m * CRITICAL_DENSITY), 1./3.);

	double ncen_expected = N_cen(halo_m, opt);
	int Ncen = (gsl_rng_uniform(rng) <= ncen_expected) ? 1 : 0;

	double nsat_expected = N_sat(halo_m, ncen_expected, opt);
	int Nsat = gsl_ran_poisson(rng, nsat_expected);
	
	if (Ncen == 0 && Nsat > 0) {
	    Ncen = 1;
	    Nsat -= 1;
	}

	if (Ncen > 0) {
	    float vel[NDIM] = {h->vel[0], h->vel[1], h->vel[2]};
	    /* central */
	    if (!strcmp(opt->velocity_distribution, "gaussian"))
		gaussian_velocity(h->vrms, h->vel, opt, rng, vel);
	    else if (!strcmp(opt->velocity_distribution, "exponential"))
		exponential_velocity(h->vrms, h->vel, opt, rng, vel);
	    StkPushData(stk, (halo *)h->pos, NDIM*sizeof(float));
	    StkPushData(stk, vel, NDIM*sizeof(float));
	}
#if 0
	/* For diagnostic plot */
	printf("%8.4f %8.4f %3d %3d %9.4g %5.3f %8.3f %8.3f %8.3f\n", 
	       ncen_expected, nsat_expected, Ncen, Nsat, 
	       halo_m, halo_r, h->pos[0], h->pos[1], h->pos[2]); 
#endif
	for (int i = 0; i < Nsat; i++) {
	    float pos[NDIM] = {}, vel[NDIM] = {};

	    if (!strcmp(opt->density_profile, "isothermal"))
		isothermal_density(halo_r, h->pos, opt, rng, pos);
	    else if (!strcmp(opt->density_profile, "NFW"))
		nfw_density(halo_r, h->r, h->rs, h->pos, opt, rng, pos);

	    if (!strcmp(opt->velocity_distribution, "constant"))
		constant_velocity(vcirc, h->vel, opt, rng, vel);
	    else if (!strcmp(opt->velocity_distribution, "gaussian"))
		gaussian_velocity(h->vrms, h->vel, opt, rng, vel);
	    else if (!strcmp(opt->velocity_distribution, "exponential"))
		exponential_velocity(h->vrms, h->vel, opt, rng, vel);

	    /* satellites */
	    StkPushData(stk, pos, NDIM*sizeof(float));
	    StkPushData(stk, vel, NDIM*sizeof(float));
	}
    }
}


int
main(int argc, char *argv[])
{
    options_s *opt, opt_models[] = {
	[0].model = "manera12",
	[0].mass_name = "m200b", 
	[0].parent_name = "m200b_pid",
	[0].delta_halo = 200.0,
	[0].M_cut = 1.193987172e+13, /* 10^13.077 */
	[0].M1 = 1.0e+14,
	[0].M_min = 1.23026916e+13, /* 10^13.09 */
	[0].sigma_logM = 0.596,
	[0].alpha = 1.0126,
	[0].cvir_fac = 1.0,
	[0].seed = 0,
	[0].density_profile = "NFW",
	[0].velocity_distribution = "gaussian",

	[1].model = "reddick13",
	[1].mass_name = "mvir", 
	[1].parent_name = "mvir_pid",
	[1].delta_halo = 360.0,	/* not used */
	[1].M_cut = 2.05e+12,
	[1].M1 = 3.11e+14,
	[1].M_min = 2.55e+13,
	[1].sigma_logM = 0.636,
	[1].alpha = 1.06,
	[1].cvir_fac = 0.606,
	[1].seed = 0,
	[1].density_profile = "NFW",
	[1].velocity_distribution = "gaussian",

	[2].model = ""		/* indicate last entry */
    };

    MPMY_Init(&argc, &argv);
    if (argc < 2) {
	singlPrintf("Required arguments: in=filename\n");
	singlPrintf("Optional arguments: hdr mass_name parent_name M_cut M1 M_min sigma_logM alpha Dgamma Dv seed\n");
	singlPrintf("Optional arguments: [xyz]_min [xyz]_max\n");
	exit(1);
    } else {
	opt = &opt_models[0];
	for (int i = 1; i < argc; i++) {
	    char s[64];
	    if (sscanf(argv[i], "model=%64s", s) == 1) {
		for (int j = 0; opt_models[j].model[0]; j++)
		    if (!strcmp(s, opt_models[j].model)) opt = &opt_models[j];
	    }
	}
	parse_opt(argc, argv, opt);
    }
    singlPrintf("%s\n\tversion %s %s %s\n", argv[0], Version, Compiled_date, Compiled_time);

    int nobj;
    int64_t gnobj;
    halo *halos;
    SDF *sdfp = ReadData(opt->in, &halos, &gnobj, &nobj, opt);
    Stk outstk;
    StkInitEz(&outstk);
    SDFgetdouble(sdfp, "Omega0_m", &opt->Omega0_m);
    SDFgetdouble(sdfp, "BOX_SIZE", &opt->box_size);

    const gsl_rng_type *T = gsl_rng_default;
    gsl_rng *rng = gsl_rng_alloc(T);
    gsl_rng_set(rng, MPMY_Nproc() * opt->seed + MPMY_Procnum());

    process_halos(halos, nobj, opt, rng, &outstk);

    char outname[256];
    if (opt->out[0]) snprintf(outname, sizeof(outname), "%s", opt->out);
    else snprintf(outname, sizeof(outname), "%s.%s_mock", opt->in, opt->model);
    galaxy *outgals = StkBase(&outstk);
    int ngals = StkSz(&outstk)/sizeof(galaxy);
    int64_t gngals = ngals;
    MPMY_Combine(&gngals, &gngals, 1, MPMY_INT64, MPMY_SUM);

    singlPrintf("\nWriting %d galaxies to %s.\n", gngals, outname);
    
    SDFwrite(outname, gngals, 
	     ngals, outgals, sizeof(galaxy),
	     GALAXYDESC,
	     "ngals", SDF_INT, gngals,
	     "Omega0_m", SDF_DOUBLE, opt->Omega0_m,
	     "BOX_SIZE", SDF_DOUBLE, opt->box_size,
	     "delta_halo", SDF_DOUBLE, opt->delta_halo,
	     "seed", SDF_INT, opt->seed,
	     "M_cut", SDF_DOUBLE, opt->M_cut,
	     "M1", SDF_DOUBLE, opt->M1,
	     "M_min", SDF_DOUBLE, opt->M_min,
	     "sigma_logM", SDF_DOUBLE, opt->sigma_logM,
	     "cvir_fac", SDF_DOUBLE, opt->cvir_fac,
	     "model", SDF_STRING, opt->model,
	     NULL);

    singlPrintf("\nOutput %d galaxies to %s.\n", gngals, outname);

    gsl_rng_free(rng);
    MPMY_Finalize();
    exit(0);
}
