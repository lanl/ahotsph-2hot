/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "Malloc.h"
#include "singlio.h"
#include "macr.h"
#include "SDF.h"
#include "mpmy.h"
#define NDIM 3
#include "vop.h"
#include "fastflpt.h"
#include "SDFread.h"
#include "error.h"
#include "gc.h"
#include "cosmo.h"

typedef struct {
    float mass;			/* mass of body */
    float pos[NDIM];		/* position of body */
    float vel[NDIM];		/* velocity of body */
    int64_t ident;		/* unique identifier */
} __attribute__ ((packed)) body;

typedef struct {
  int      npart[6];
  double   mass[6];
  double   time;
  double   redshift;
  int      flag_sfr;
  int      flag_feedback;
  int      npartTotal[6];
  int      flag_cooling;
  int      num_files;
  double   BoxSize;
  double   Omega0;
  double   OmegaLambda;
  double   HubbleParam; 
  int      flag_multiphase;
  int      flag_stellarage;
  int      flag_sfrhistogram;
  char     version[12];
  int64_t  npartTotal64[6];
  char     fill[24];  /* fills to 256 Bytes */
} __attribute__ ((packed)) gadget_header;

typedef struct options_s {
    double mass_scale;
    double pos_scale;
    int fileversion;
    int long_id;
    int write_mass;
    int verbose;
    char in[FILENAME_MAX];
    char out[FILENAME_MAX];
    char hdr[FILENAME_MAX];
} options_s;

#define scan(s, v, f) sscanf(s, #v "=" #f, &opt->v)
#define scans(s, v, f) sscanf(s, #v "=" #f, opt->v)

static void
parse_opt(int argc, char *argv[], options_s *opt)
{
    /* A non-standard arg parsing idiom */
    for (int i = 1; i < argc; i++) {
	char *p = argv[i];
	int ret;
	ret = 0;
	ret += scan(p, mass_scale, %lg);
	ret += scan(p, pos_scale, %lg);
	ret += scan(p, fileversion, %d);
	ret += scan(p, long_id, %d);
	ret += scan(p, write_mass, %d);
	ret += scan(p, verbose, %d);
	ret += scans(p, in, %256s);
	ret += scans(p, out, %256s);
	ret += scans(p, hdr, %256s);
	if (ret != 1) {
	    SinglWarning("Warning: failed to parse %s\n", p);
	}
    }
}

void 
FixId(body *btab, int64_t nobj, int64_t gnobj)
{
    int start;
    int mynobj;
    int i;

    NobjInitial(gnobj, MPMY_Nproc(), MPMY_Procnum(), &mynobj, &start);
    if (mynobj != nobj) Error("mynobj != nobj\n");

    for(i=0; i<nobj; i++){
	btab[i].ident = start+i;
    }
}

int
main(int argc, char *argv[])
{
    int i;
    char outname[256];
    SDF  *sdfp;
    int massconf, xconf, yconf, zconf;
    int vxconf, vyconf, vzconf;
    int identconf;
    body *btab;
    int64_t gnobj;
    int nobj;
    int iter;
    float eps;
    float R0;
    float particle_mass;
    double tpos;
    double redshift, H0, h_100;
    double a = 1.0;
    FILE *fp;
    int fortran_blk;
    gadget_header h;
    struct cosmology cosmo;
    Timer_t StepTot;

    options_s opt = {
	.pos_scale = 1.0e-3,	/* to Mpc */
	.mass_scale = 1e10,	/* to Msol */
	.verbose = 1};

    if (argc < 2) {
	fprintf(stderr, "Required arguments: in=filename\n");
	fprintf(stderr, "Optional arguments: out hdr mass_scale pos_scale long_id write_mass\n");
	exit(1);
    } else {
	parse_opt(argc, argv, &opt);
    }
    MPMY_Init(&argc, &argv);
    EnableTimer(&StepTot, "Step Total");
    StartTimer(&StepTot);

    if (opt.out[0]) {
	sprintf(outname, "%s.%d",  opt.out, MPMY_Procnum());
    } else {
	sprintf(outname, "%s.gadget.%d",  opt.in, MPMY_Procnum());
    }

    singlPrintf("Welcome to the machine\n");

    if ((sdfp = SDFopen(opt.hdr[0] ? opt.hdr : NULL, opt.in)) == NULL) {
 	SinglError("Sorry, couldn't SDFopen %s\n%s\n",
		   opt.in, SDFerrstring);
    }

    memset(&cosmo, 0, sizeof(cosmo));
    SDFgetintOrDefault(sdfp, "iter", &iter, 0);
    SDFgetfloatOrDefault(sdfp, "eps", &eps, 0.0);
    SDFgetfloatOrDefault(sdfp, "Rz",  &R0, 0.0);
    SDFgetfloatOrDefault(sdfp, "R0",  &R0, R0);
    SDFgetdoubleOrDefault(sdfp, "tpos", &tpos, 0.0);
    SDFgetdoubleOrDefault(sdfp, "redshift",  &redshift, 0.0);

    /* SDFget does not write third arg if key is not found */
    int fileversion = opt.fileversion;
    SDFgetint(sdfp, "version", &fileversion);
    SDFgetint(sdfp, "version_2HOT", &fileversion);
    int units_2HOT = 0;
    int units_rockstar = 0;
    SDFgetint(sdfp, "units_2HOT", &units_2HOT);
    if (fileversion == 1) {
	units_2HOT = 1;
	SDFgetdoubleOrDie(sdfp, "Omega0",  &cosmo.Omega0_m);
	SDFgetdoubleOrDie(sdfp, "Lambda_prime",  &cosmo.Omega0_lambda);
	SDFgetdoubleOrDie(sdfp, "H0",  &H0);
	h_100 = H0*10.0*(one_kpc/one_Gyr);
    } else if (fileversion == 2) {
	units_2HOT = 1;
	SDFgetdoubleOrDie(sdfp, "Omega0_m",  &cosmo.Omega0_m);
	SDFgetdoubleOrDie(sdfp, "Omega0_r",  &cosmo.Omega0_r);
	SDFgetdoubleOrDie(sdfp, "Omega0_lambda",  &cosmo.Omega0_lambda);
	SDFgetdoubleOrDie(sdfp, "Omega0_fld",  &cosmo.Omega0_fld);
	SDFgetdoubleOrDie(sdfp, "H0",  &H0);
	SDFgetdoubleOrDie(sdfp, "h_100",  &h_100);
    } else {
	SDFgetint(sdfp, "units_rockstar", &units_rockstar);
	if (!units_rockstar) {
	    SinglWarning("File version not specified, assuming rockstar units\n");
	    units_rockstar = 1;
	}
	double L0;
	SDFgetdoubleOrDie(sdfp, "BOX_SIZE",  &L0);
	R0 = L0/2.0;
	SDFgetdouble(sdfp, "SCALE_NOW",  &a);
	SDFgetdouble(sdfp, "Om",  &cosmo.Omega0_m);
	SDFgetdouble(sdfp, "Omega0_m",  &cosmo.Omega0_m);
	SDFgetdouble(sdfp, "Ol",  &cosmo.Omega0_lambda);
	SDFgetdouble(sdfp, "Omega0_lambda",  &cosmo.Omega0_lambda);
	SDFgetdoubleOrDie(sdfp, "h0",  &h_100);
    }
    SDFclose(sdfp);

    singlPrintf("mass_scale=%g pos_scale=%g\n", opt.mass_scale, opt.pos_scale);

    sdfp = SDFreadf64(opt.hdr[0] ? opt.hdr : NULL, opt.in, (void **)&btab, 
		      &gnobj, &nobj, sizeof(body),
		      "mass", offsetof(body, mass), &massconf,
		      "x", offsetof(body, pos[0]), &xconf,
		      "y", offsetof(body, pos[1]), &yconf,
		      "z", offsetof(body, pos[2]), &zconf,
		      "vx", offsetof(body, vel[0]), &vxconf,
		      "vy", offsetof(body, vel[1]), &vyconf,
		      "vz", offsetof(body, vel[2]), &vzconf,
		      "ident", offsetof(body, ident), &identconf,
		      NULL);
    
    if (xconf==0 || yconf==0 || zconf==0) {
	SinglError("Could not find %s %s %s in data file!\n",
		   (xconf==0)? "x" : "",
		   (yconf==0)? "y" : "",
		   (zconf==0)? "z" : "");
    }
    if (vxconf != vyconf || vxconf != vzconf) {
	SinglError("Missing velocity components!\n");
    }
    if (identconf == 0) {
	SinglWarning("No \"ident\" in file, numbering sequentially\n");
	FixId(btab, nobj, gnobj);
    }
    if (massconf == 0 && opt.write_mass) Error("No mass in file, and used opt.write_mass\n");
    if (massconf == 0) {
	SDFgetfloatOrDie(sdfp, "particle_mass", &particle_mass);
    } else {
	particle_mass = btab[0].mass;
    }
    SDFclose(sdfp);
    
    a = 1.0/(1.0+redshift);

    if (sizeof(gadget_header) != 256) SinglError("Bad header size\n");
    memset(&h, 0, sizeof(gadget_header));
    h.npart[1] = nobj;
    h.mass[1] = h_100 * opt.mass_scale * particle_mass; /* what if multiple masses? */
    h.time = a;
    h.redshift = redshift;
    h.npartTotal64[1] = gnobj;
    if (gnobj > (1L<<31)) {
	SinglWarning("gnobj larger than int can hold\n");
	h.npartTotal[1] = 0;
    } else {
	h.npartTotal[1] = gnobj;
    }
    h.num_files = MPMY_Nproc();
    h.BoxSize = 2.0 * R0 * h_100 * opt.pos_scale;
    h.Omega0 = cosmo.Omega0_m + cosmo.Omega0_r; /* not strictly correct */
    h.OmegaLambda = cosmo.Omega0_lambda + cosmo.Omega0_fld;
    h.HubbleParam = h_100;
    strncpy(h.version, "ds13-1.0", 9);

    singlPrintf("Omega0=%g HubbleParam=%g BoxSize=%g\n",
		h.Omega0, h.HubbleParam, h.BoxSize);
    
    Fopen(fp, outname, "w");
    
    fortran_blk = sizeof(h);
    if (fortran_blk != 256) Error("Bad header size\n");
    Fwrite(&fortran_blk, sizeof(int), 1, fp);
    Fwrite(&h, sizeof(gadget_header), 1, fp);
    Fwrite(&fortran_blk, sizeof(int), 1, fp);
    
    fortran_blk = nobj * NDIM * sizeof(float);
    Fwrite(&fortran_blk, sizeof(int), 1, fp);
    for (i = 0; i < nobj; i++) {
	float pos[NDIM];
	VV(pos, = R0 + (1.0 / a) * btab[i].pos);
	VS(pos, *= h_100 * opt.pos_scale);
	Fwrite(pos, sizeof(float), NDIM, fp);
    }
    Fwrite(&fortran_blk, sizeof(int), 1, fp);
    
    fortran_blk = nobj * NDIM * sizeof(float);
    Fwrite(&fortran_blk, sizeof(int), 1, fp);
    for (i = 0; i < nobj; i++) {
	float vel[NDIM];
	/* tree19 stores peculiar vels */
	VV(vel, = btab[i].vel);
	VS(vel, *= (one_kpc/one_Gyr) / sqrt(a));
	Fwrite(vel, sizeof(float), NDIM, fp);
    }
    Fwrite(&fortran_blk, sizeof(int), 1, fp);

    if (opt.long_id) {
	fortran_blk = nobj * sizeof(int64_t);
	Fwrite(&fortran_blk, sizeof(int), 1, fp);
	for (i = 0; i < nobj; i++) {
	    int64_t id = btab[i].ident;
	    Fwrite(&id, sizeof(int64_t), 1, fp);
	}
	Fwrite(&fortran_blk, sizeof(int), 1, fp);
    } else {
	fortran_blk = nobj * sizeof(int);
	Fwrite(&fortran_blk, sizeof(int), 1, fp);
	for (i = 0; i < nobj; i++) {
	    int id = btab[i].ident;
	    Fwrite(&id, sizeof(int), 1, fp);
	}
	Fwrite(&fortran_blk, sizeof(int), 1, fp);
    }

    if (opt.write_mass) {
	fortran_blk = nobj * sizeof(float);
	Fwrite(&fortran_blk, sizeof(int), 1, fp);
	for (i = 0; i < nobj; i++) {
	    float mass = h_100 * opt.mass_scale * btab[i].mass;
	    Fwrite(&mass, sizeof(float), 1, fp);
	}
	Fwrite(&fortran_blk, sizeof(int), 1, fp);
    }
    
    Fclose(fp);
    singlPrintf("\nOutput to %s done.\n", outname);
    StopTimer(&StepTot);
    OutputTimer(&StepTot, singlPrintf);
    MPMY_Finalize();
}
