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

typedef struct  {
    float mass;			/* mass of halo */
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
    int seed;
    double delta_halo;
    double Omega0_m;
    double box_size;
    double x_min, y_min, z_min;
    double x_max, y_max, z_max;
    char mass_name[64];
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
	ret += scans(p, mass_name, %64s);
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
    int vxconf, vyconf, vzconf;
    int pidconf;
    SDF *sdfp;

    singlPrintf("Reading %s\n", name);
    sdfp = SDFreadf64(opt->hdr, name, (void *)htab, gnobj, nobj, sizeof(halo),
		      opt->mass_name, offsetof(halo, mass), &massconf,
		      "x", offsetof(halo, pos[0]), &xconf,
		      "y", offsetof(halo, pos[1]), &yconf,
		      "z", offsetof(halo, pos[2]), &zconf,
		      "vx", offsetof(halo, vel[0]), &vxconf,
		      "vy", offsetof(halo, vel[1]), &vyconf,
		      "vz", offsetof(halo, vel[2]), &vzconf,
		      opt->parent_name, offsetof(halo, pid), &pidconf,
		      NULL);

    if (massconf==0 || xconf==0 || yconf==0 || zconf==0) {
	SinglError("Could not find %s %s %s %s in data file!\n",
		   (massconf==0)? opt->mass_name : "",
		   (xconf==0)? "x" : "",
		   (yconf==0)? "y" : "",
		   (zconf==0)? "z" : "");
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
    /* HOD.x case 2 */
    return (halo_m >= opt->M_cut)
	? ncen_expected * pow(((halo_m-opt->M_cut)/opt->M1), opt->alpha) 
	: 0.0;
}


void
process_halos(const halo *halos, int nobj, const options_s *opt, const gsl_rng *rng, Stk *stk)
{
    for (const halo *h = halos; h < halos + nobj; h++) {
	if (h->pid != -1) continue;
	if (h->mass == 0.0f) continue;
	double halo_m = h->mass;
	double halo_r = pow(3.0 * halo_m / (4.0 * M_PI * opt->delta_halo * opt->Omega0_m * CRITICAL_DENSITY), 1./3.);
	double vcirc = sqrt(Gc*halo_m);

	double ncen_expected = N_cen(halo_m, opt);
	int Ncen = (gsl_rng_uniform(rng) > ncen_expected) ? 0 : 1;

	double nsat_expected = N_sat(halo_m, ncen_expected, opt);
	int Nsat = gsl_ran_poisson(rng, nsat_expected);
	
	if (Ncen == 0 && Nsat > 0) {
	    Ncen = 1;
	    Nsat -= 1;
	}

	if (Ncen > 0) {
	    /* central */
	    StkPushData(stk, (halo *)h->pos, NDIM*sizeof(float));
	    StkPushData(stk, (halo *)h->vel, NDIM*sizeof(float));
	}
#if 0
	/* For diagnostic plot */
	printf("%8.4f %8.4f %3d %3d %9.4g %5.3f %8.3f %8.3f %8.3f\n", 
	       ncen_expected, nsat_expected, Ncen, Nsat, 
	       halo_m, halo_r, h->pos[0], h->pos[1], h->pos[2]); 
#endif
	for (int i = 0; i < Nsat; i++) {
	    float pos[NDIM], vel[NDIM];
	    /* Simple isotropic isothermal distribution */
	    float rsq = spherical_rand(pos, rng);
	    spherical_rand(vel, rng);
	    float scale = halo_r * gsl_rng_uniform(rng) / sqrt(rsq);
	    float vscale = vcirc;
	    for (int k = 0; k < NDIM; k++) {
		pos[k] = h->pos[k] + pos[k] * scale;
		vel[k] = h->vel[k] + vel[k] * vscale;
		if (pos[k] >= opt->box_size) pos[k] -= opt->box_size;
		if (pos[k] < 0.0) pos[k] += opt->box_size;
	    }
	    /* satellites */
	    StkPushData(stk, pos, NDIM*sizeof(float));
	    StkPushData(stk, vel, NDIM*sizeof(float));
	}
    }
}


int
main(int argc, char *argv[])
{
    options_s opt = {.mass_name = "m200b", .parent_name = "m200b_pid",
		     /* Manera12 parameters */
		     .M_cut = 1.193987172e+13, /* 10^13.077 */
		     .M1 = 1.0e+14,
		     .M_min = 1.23026916e+13, /* 10^13.09 */
		     .sigma_logM = 0.596,
		     .alpha = 1.0126,
		     .Dgamma = 0.0,
		     .Dv = 1.0,
		     .seed = 0,
    };
    MPMY_Init(&argc, &argv);
    if (argc < 2) {
	singlPrintf("Required arguments: in=filename\n");
	singlPrintf("Optional arguments: hdr mass_name parent_name M_cut M1 M_min sigma_logM alpha Dgamma Dv seed\n");
	singlPrintf("Optional arguments: [xyz]_min [xyz]_max\n");
	exit(1);
    } else {
	parse_opt(argc, argv, &opt);
    }
    singlPrintf("%s\n\tversion %s %s %s\n", argv[0], Version, Compiled_date, Compiled_time);

    int nobj;
    int64_t gnobj;
    halo *halos;
    SDF *sdfp = ReadData(opt.in, &halos, &gnobj, &nobj, &opt);
    Stk outstk;
    StkInitEz(&outstk);
    SDFgetdouble(sdfp, "Omega0_m", &opt.Omega0_m);
    SDFgetdouble(sdfp, "overdensity", &opt.delta_halo);
    SDFgetdouble(sdfp, "BOX_SIZE", &opt.box_size);

    const gsl_rng_type *T = gsl_rng_default;
    gsl_rng *rng = gsl_rng_alloc(T);
    gsl_rng_set(rng, MPMY_Nproc() * opt.seed + MPMY_Procnum());

    process_halos(halos, nobj, &opt, rng, &outstk);

    char outname[256];
    snprintf(outname, sizeof(outname), "%s.gals", opt.in);
    galaxy *outgals = StkBase(&outstk);
    int ngals = StkSz(&outstk)/sizeof(galaxy);
    int64_t gngals = ngals;
    MPMY_Combine(&gngals, &gngals, 1, MPMY_INT64, MPMY_SUM);
    
    SDFwrite(outname, gngals, 
	     ngals, outgals, sizeof(galaxy),
	     GALAXYDESC,
	     "ngals", SDF_INT, gngals,
	     "Omega0_m", SDF_DOUBLE, opt.Omega0_m,
	     "BOX_SIZE", SDF_DOUBLE, opt.box_size,
	     "delta_halo", SDF_DOUBLE, opt.delta_halo,
	     "seed", SDF_INT, opt.seed,
	     "M_cut", SDF_DOUBLE, opt.M_cut,
	     "M1", SDF_DOUBLE, opt.M1,
	     "M_min", SDF_DOUBLE, opt.M_min,
	     "sigma_logM", SDF_DOUBLE, opt.sigma_logM,
	     "Dgamma", SDF_DOUBLE, opt.Dgamma,
	     "Dv", SDF_DOUBLE, opt.Dv,
	     NULL);

    singlPrintf("\nOutput %d galaxies to %s.\n", gngals, outname);

    gsl_rng_free(rng);
    MPMY_Finalize();
    exit(0);
}
