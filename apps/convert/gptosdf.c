/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Malloc.h"
#include "macr.h"
#define NDIM 3
#include "vop.h"
#include "error.h"
#include "mpmy.h"
#include "Msgs.h"
#include "SDF.h"
#include "SDFwrite.h"
#include "pqsort.h"
#include "singlio.h"
#include "cosmo.h"

#ifndef M_PI
#define	M_PI	3.14159265358979323846
#endif

typedef struct {
    float pos[NDIM];		/* position of body */
    float vel[NDIM];		/* velocity of body */
#ifdef SAVE_ACC
    float acc[NDIM];
    float phi;
#endif
    int64_t ident;		/* unique identifier */
} __attribute__ ((packed)) body;

typedef struct {
    float val[NDIM];
} vec;

#ifdef SAVE_ACC
#define OUTBODYDESC \
"struct {\n\
    float x, y, z;		/* position of body */\n\
    float vx, vy, vz;		/* velocity of body */\n\
    float ax, ay, az;\n\
    float phi;\n\
    int64_t ident;		/* unique identifier */\n\
}"
#else
#define OUTBODYDESC \
"struct {\n\
    float x, y, z;		/* position of body */\n\
    float vx, vy, vz;		/* velocity of body */\n\
    int64_t ident;		/* unique identifier */\n\
}"
#endif

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
  char     fill[84];  /* fills to 256 Bytes */
} gadget_header;

/* Use this to sort by "ident" for output */
float UnityCost(const void *ptr){
    return 1.0;
}

Key_t OutIdentKey(const body *bp)
{
    Key_t tmp;

    /* Using KeyInt will truncate int64_t idents */
    tmp.k[0] = bp->ident;
#if NK == 2
    tmp.k[1] = 0;
#endif
    /* Decomp ignores the last 30 bits of the Key */
    return KeyLshift(tmp,30);
}

/* Convert gadget multiple file format */

int
main(int argc, char *argv[])
{
    int i, nfiles;
    FILE *fp;
    int fortran_blocksize;
    body *btab;
    int64_t gnobj;
    int64_t nobj;
    int Nhalo;
    double R0;
    struct cosmology cosmo;
    double mtot = 0.0, total_mass;
    float mass;
    int iter;
    char filenamep[256],  outname[256];
    float *m = NULL;
    vec *pos, *vel, *acc = NULL;
    float *pot = NULL;
    unsigned int *id = NULL;
    int64_t *lid = NULL;
    gadget_header header;
    double pos_scale = 1.0;
    double mass_scale = 1.0;
    sortresult_t outputsort;
    struct stat stat;
    off_t filesize, fileleft;
    int has_m = 0;
    int has_acc = 0;
    int has_phi = 0;
    int longid = 1;
    double nx;

    MPMY_Init(&argc, &argv);
    singlPrintf("Running %s\n", argv[0]);

#if 0
    char msgfile[256];
    sprintf(msgfile, "msgs/msg.%d", MPMY_Procnum());
    MsgdirInit(msgfile);
    Msg_turnon("SDFwrite.c,decomp.c,pqsort.c,mpmy_mpiio.c");
#endif

    if (argc < 3 || argc > 4) {
	fprintf(stderr, "usage: %s filename_base_pos [nfiles] [longid]\n", argv[0]);
	exit(1);
    }
    if (argc == 3) {
	nfiles = atoi(argv[2]);
    } else {
	nfiles = 1;
    }
    if (argc == 4) {
	longid = atoi(argv[3]);
    } else {
	longid = 1;
    }

    total_mass = 0.0;
    gnobj = 0;

    if (nfiles != MPMY_Nproc()) Error("Nproc must equal nfiles\n");

    SDF *sdfp;
    int version_cosmo = 2;
    double z_initial, CICAlpha, CICAlpha_v;
    if ((sdfp = SDFopen(NULL, "params.ctl"))) {
	SDFgetintOrDefault(sdfp, "version_cosmo",  &version_cosmo, 2);
	SDFgetdoubleOrDefault(sdfp, "z_initial",  &z_initial, 0.0);
	SDFgetdoubleOrDefault(sdfp, "CICAlpha",  &CICAlpha, 0.0);
	SDFgetdoubleOrDefault(sdfp, "CICAlpha_v",  &CICAlpha_v, 0.0);
	SDFclose(sdfp);
    }

    if (nfiles == 1) {
	sprintf(filenamep, "%s", argv[1]);
    } else {
	sprintf(filenamep, "%s.%d", argv[1], MPMY_Procnum());
    }
    Fopen(fp, filenamep, "r");
    fstat(fileno(fp), &stat);
    filesize = stat.st_size;
	
    singlPrintf("Reading %s in %d files\n", argv[1], nfiles);
    Fread(&fortran_blocksize, sizeof(int), 1, fp);
    Fread(&header, sizeof(gadget_header), 1, fp);
    Fread(&fortran_blocksize, sizeof(int), 1, fp);

    memset(&cosmo, 0, sizeof(cosmo));
    if (version_cosmo == 2) {
	if (MPMY_Procnum() == 0) class_params(&cosmo, "class.ini");
	MPMY_Bcast(&cosmo, sizeof(cosmo), MPMY_CHAR, 0);
	tbl_init(&cosmo, "cosmology.tbl");
	cosmo.background_at_z(&cosmo, header.redshift);
    } else {
	cosmo.Omega0 = header.Omega0+header.OmegaLambda;
	cosmo.Omega0_m = header.Omega0;
	cosmo.h_100 = header.HubbleParam;
	cosmo.a = 1.0/(1.0+header.redshift);
	cosmo.Omega0_lambda = header.OmegaLambda;
	cosmo1_init(&cosmo);
    }
	
    for (i = 0; i < 6; i++) {
	if (i != 1 && header.npart[i] != 0) 
	    Error("This code does not currently support type %d\n", i);
    }
    Nhalo = header.npart[1];
    Msgf(("%d particles in this block\n", Nhalo));
    if (longid)
	singlPrintf("Using int64_t ids\n");
    if (header.mass[1] == 0.0) {
	has_m = 1;
    } else if (header.mass[1] < 1e-7) {
	singlPrintf("Looks like masses are fractions of mtot.\n");
    }
    fileleft = filesize-sizeof(gadget_header);
    fileleft -= Nhalo * sizeof(float) * 6; /* pos, vel */
    if (longid) {
	fileleft -= Nhalo * sizeof(int64_t); /* id */
    } else {
	fileleft -= Nhalo * sizeof(int); /* id */
    }
    if (has_m) fileleft -= Nhalo * sizeof(float); /* mass */
    if (has_m) Error("This version does not support variable particle mass\n");
    if (fileleft/Nhalo < 4) {
	singlPrintf("No pot or acc in file\n");	
    } else {
	singlPrintf("acc in file\n");
	has_acc = 1;
	fileleft -= Nhalo * sizeof(float) * 3; /* acc */
	if (fileleft/Nhalo > 4) {
	    singlPrintf("pot in file\n");
	    has_phi = 1;
	}
    }
    if (header.BoxSize < 2000.0) {
	singlPrintf("Looks like positions are in Mpc.\n");
	pos_scale = 1000.0;
    }
    Msgf(("header.npart %d %d %d %d %d %d\n", 
	  header.npart[0], header.npart[1], header.npart[2], 
	  header.npart[3], header.npart[4], header.npart[5]));
    Msgf(("header.mass %g %g %g %g %g %g\n",
	  header.mass[0], header.mass[1], header.mass[2], 
	  header.mass[3], header.mass[4], header.mass[5]));
    
    pos = Malloc(Nhalo*sizeof(vec));
    Fread(&fortran_blocksize, sizeof(int), 1, fp);
    Fread(pos, sizeof(vec), Nhalo, fp);
    Fread(&fortran_blocksize, sizeof(int), 1, fp);

    vel = Malloc(Nhalo*sizeof(vec));
    Fread(&fortran_blocksize, sizeof(int), 1, fp);
    Fread(vel, sizeof(vec), Nhalo, fp);
    Fread(&fortran_blocksize, sizeof(int), 1, fp);
	
    if (longid) {
	lid = Malloc(Nhalo*sizeof(int64_t));
	Fread(&fortran_blocksize, sizeof(int), 1, fp);
	Fread(lid, sizeof(int64_t), Nhalo, fp);
	Fread(&fortran_blocksize, sizeof(int), 1, fp);
    } else {
	id = Malloc(Nhalo*sizeof(unsigned int));
	Fread(&fortran_blocksize, sizeof(int), 1, fp);
	Fread(id, sizeof(unsigned int), Nhalo, fp);
	Fread(&fortran_blocksize, sizeof(int), 1, fp);
    }
    
    if (has_m) {
	m = Malloc(Nhalo*sizeof(float));
	Fread(&fortran_blocksize, sizeof(int), 1, fp);
	Fread(m, sizeof(float), Nhalo, fp);
	Fread(&fortran_blocksize, sizeof(int), 1, fp);
    }
    
    if (has_phi) {
	pot = Malloc(Nhalo*sizeof(float));
	Fread(&fortran_blocksize, sizeof(int), 1, fp);
	Fread(pot, sizeof(float), Nhalo, fp);
	Fread(&fortran_blocksize, sizeof(int), 1, fp);
    }
    if (has_acc) {
	acc = Malloc(Nhalo*sizeof(vec));
	Fread(&fortran_blocksize, sizeof(int), 1, fp);
	Fread(acc, sizeof(vec), Nhalo, fp);
	Fread(&fortran_blocksize, sizeof(int), 1, fp);
    }
    Fclose(fp);

    R0 = 0.5 * header.BoxSize * pos_scale / cosmo.h_100;
    nobj = Nhalo;
    gnobj += nobj;
    
    MPMY_Combine(&gnobj, &gnobj, 1, MPMY_INT64, MPMY_SUM);
    nx = cbrt(gnobj);
    singlPrintf("gnobj = %ld (%.0f^3), Omega0_m = %g, Lambda = %g, h_100 = %g, R0 = %g, z = %g\n", 
		gnobj, nx, cosmo.Omega0_m, cosmo.Omega0_lambda, cosmo.h_100, R0, 
		1.0/cosmo.a-1.0);

    
    iter = 0;
    mtot = cosmo.Omega0_m*(3./(8.*M_PI*cosmo.Gnewt))*cosmo.H0*cosmo.H0*8.0*R0*R0*R0;
    if (header.mass[1] < 1e-7) mass_scale = mtot;
    mass = header.mass[1] * mass_scale / cosmo.h_100;
    
    btab = Calloc(nobj, sizeof(body));
	
    for (i = 0; i < nobj; i++) {
	if (pos[i].val[0] == 0.0 && pos[i].val[1] == 0.0 && pos[i].val[2] == 0.0) {
	    Warning("Proc %d particle %d has zero position\n", MPMY_Procnum(), i);
	}
	VV(btab[i].pos, = -R0 + (pos_scale / cosmo.h_100) * pos[i].val);
	VS(btab[i].pos, += R0/nx); /* offset by half a grid cell */
	/* Wrap Periodic */
	VVS(if LPAREN btab[i].pos, >= R0 RPAREN btab[i].pos, -= 2.0*R0);
	VVS(if LPAREN btab[i].pos, < -R0 RPAREN btab[i].pos, += 2.0*R0);
	VS(btab[i].pos, *= cosmo.a);
	/* comov kpc/Gyr */
	VV(btab[i].vel, = vel[i].val);
	VS(btab[i].vel, /= sqrt(1.0/cosmo.a));
	VS(btab[i].vel, *= (one_Gyr/one_kpc));
	if (has_m) {
	    mass = m[i] * mass_scale / cosmo.h_100;
	}
	/* btab[i].mass = mass; */
	total_mass += mass;
	if (longid) {
	    btab[i].ident = lid[i];
	} else {
	    btab[i].ident = id[i];
	}
	/* non power-of-two Morton IDs have offset */
	if (btab[i].ident <= 0 /* || btab[i].ident >= gnobj */) {
	    Warning("Proc %d particle %d has ident %ld\n", MPMY_Procnum(), i, btab[i].ident);
	}
#ifdef SAVE_ACC
	if (has_acc) {
	    VV(btab[i].acc, = (one_Gyr/one_kpc) * (one_Gyr/one_kpc) * cosmo.h_100 * acc[i].val);
	}
	if (has_phi) {
	    btab[i].phi = pot[i];
	}
#endif
    }
    if (has_acc) {
	Free(acc);
    }
    if (has_phi) {
	Free(pot);
    }
    if (has_m) {
	Free(m);
    }
    if (longid) Free(lid);
    else Free(id);
    Free(vel);
    Free(pos);

    MPMY_Combine(&total_mass, &total_mass, 1, MPMY_DOUBLE, MPMY_SUM);

    if (fabs(mtot-total_mass)/mtot > 1e-4) {
	Error("total_mass is %lg, mtot from Omega0_m is %lg\n", total_mass, mtot);
    }

#if 0
    pqsortsetup_order(&outputsort, btab, nobj,
		      sizeof(body), 0.1, 1, Realloc_f);
    btab = pqsort(&outputsort,
		  (pq_wgtproto)UnityCost, 
		  (pq_keyproto)OutIdentKey);
    nobj = outputsort.nobj;
#endif

    snprintf(outname, sizeof(outname), "%s.sdf", argv[1]);
    singlPrintf("Writing \"%s\"\n", outname);

    double t = cosmo.t_at_a(&cosmo, cosmo.a);

    SDFwrite64(outname, gnobj,
	       nobj, btab, sizeof(body), OUTBODYDESC,
	       "version", SDF_INT, 2,
	       "version_2HOT", SDF_INT, 2,
	       "version_cosmo", SDF_INT, version_cosmo,
	       "units_2HOT", SDF_INT, 2,
	       "npart", SDF_INT64, gnobj,
	       "particle_mass", SDF_FLOAT, mass,
	       "do_periodic", SDF_INT, 1,
	       "redshift", SDF_DOUBLE, 1.0/cosmo.a-1.0,
	       "tpos", SDF_DOUBLE, t,
	       "tvel", SDF_DOUBLE, t,
	       "H", SDF_DOUBLE, cosmo.H_at_t(&cosmo, t),
	       "conf_distance", SDF_DOUBLE, cosmo.conformal_distance_at_t(&cosmo, t),
	       "growthfac", SDF_DOUBLE, cosmo.growthfac_at_t(&cosmo, t),
	       "ic_growthfac", SDF_DOUBLE, cosmo.growthfac_at_t(&cosmo, t),
	       "ic_Nmesh", SDF_INT, (int)nx,
	       "CICAlpha", SDF_DOUBLE, CICAlpha,
	       "CICAlpha_v", SDF_DOUBLE, CICAlpha_v,
	       "z_initial", SDF_DOUBLE, z_initial,
	       "iter", SDF_INT, iter,
	       "L0", SDF_FLOAT, 2.0*R0,
	       "R0", SDF_FLOAT, R0,
	       "Rx", SDF_FLOAT, R0,
	       "Ry", SDF_FLOAT, R0,
	       "Rz", SDF_FLOAT, R0,
	       "H0", SDF_DOUBLE, cosmo.H0,
	       "Omega0", SDF_DOUBLE, cosmo.Omega0,
	       "Omega0_m", SDF_DOUBLE, cosmo.Omega0_m,
	       "Omega0_r", SDF_DOUBLE, cosmo.Omega0_r,
	       "Omega0_lambda", SDF_DOUBLE, cosmo.Omega0_lambda,
	       "Omega0_cdm", SDF_DOUBLE, cosmo.Omega0_cdm,
	       "Omega0_ncdm_tot", SDF_DOUBLE, cosmo.Omega0_ncdm_tot,
	       "Omega0_b", SDF_DOUBLE, cosmo.Omega0_b,
	       "Omega0_g", SDF_DOUBLE, cosmo.Omega0_g,
	       "Omega0_ur", SDF_DOUBLE, cosmo.Omega0_ur,
	       "Omega0_fld", SDF_DOUBLE, cosmo.Omega0_fld,
	       "w0_fld", SDF_DOUBLE, cosmo.w0_fld,
	       "wa_fld", SDF_DOUBLE, cosmo.wa_fld,
	       "h_100", SDF_DOUBLE, cosmo.h_100,
	       "hubble", SDF_DOUBLE, cosmo.H_at_t(&cosmo, t),
	       "mtot", SDF_DOUBLE, mtot,
	       "Gnewt", SDF_DOUBLE, cosmo.Gnewt,
	       NULL);
    Free(btab);
    singlPrintf("Done.\n", gnobj);
    MPMY_Finalize();
    exit(0);
}
