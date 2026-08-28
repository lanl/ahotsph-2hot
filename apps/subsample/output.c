/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#undef NO_MSGS
#include <stdio.h>		/* only use sprintf */
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include "SDF.h"
#include "sw_malloc.h"
#include "Malloc.h"
#include "stk.h"
#include "pqsort.h"
#include "SDFwrite.h"
#include "physics.h"
#include "vop.h"
#include "Msgs.h"
#include "singlio.h"
#include "mpmy.h"
#include "mpmy_io.h"
#include "cosmo.h"
#include "integrate.h"
#include "output.h"
#include "version_2HOT.h"
#define R123_USE_U01_DOUBLE 1
#include <Random123/threefry.h>
#include <Random123/u01fixedpt.h>

int maxmem(void);
int maxheap(void);

static const body *
subsample(int64_t *gnobj, int *nobj, body *btab, double subsample_fraction, int subsample_random_seed)
{
    Stk outstk;
    StkInitEz(&outstk);
    threefry2x64_ctr_t ctr = {{}};
    threefry2x64_key_t key = {{0, subsample_random_seed}};

    for (body *b = btab; b < btab + *nobj; b++) {
        ctr.v[0] = b->ident & IDENTMASK;
	threefry2x64_ctr_t rand = threefry2x64(ctr, key);
	double p = u01fixedpt_closed_open_64_53(rand.v[0]);
	if (p < subsample_fraction) {
	    StkPushData(&outstk, b, sizeof(body));
	}
    }
    *nobj = StkSz(&outstk)/sizeof(body);
    *gnobj = *nobj;
    MPMY_Combine(gnobj, gnobj, 1, MPMY_INT64, MPMY_SUM);
    return StkBase(&outstk);
}

void
output(const char *outname, int64_t gnobj, int nobj, body *btab, int iter, 
       double dt, double dtv, cosmology *cosmo, 
       double tpos, double tvel, int do_cosmology, int do_periodic, 
       float eps, float this_eps_scaled, int force_smoothing_type,
       float this_tol, float frac_tol, float frac_tol0, 
       const float *R, const int *N, 
       int write_nfiles, double *ke, double *pe, 
       int do_output, int identsort_output, int ic_Nmesh, double ic_growthfac,
       double subsample_fraction, int subsample_random_seed,
       char *version_nln, char *compiled_date_nln, char *compiled_time_nln
       )
{
    int i;
    sortresult_t outputsort = {};
    outbodyptr output_btab;
    double z, a, H, tacc;
    outbodyptr b;
    float rmin[NDIM], rmax[NDIM];
    float sysradius[NDIM];
    MPMY_Comm_request req;
    int checkpoint;
    int64_t gnobj0 = gnobj;
    const body *btab2;
    int morton_xyz = 0;
    float particle_mass = btab[0].mass;

    if (do_output) checkpoint = 0;
    else checkpoint = 1;

    if( Msg_test("memleak") ){
	Msg_do("Memory map before output\n");
	malloc_print();
    }

    if (subsample_fraction != 0.0) {
	Msgf(("Doing subsample\n"));
	btab2 = subsample(&gnobj, &nobj, (body *)btab, subsample_fraction, subsample_random_seed);
    } else {
	btab2 = btab;
    }

    Msgf(("Doing output\n"));
    output_btab = Malloc(nobj * sizeof(outbody));
    Msgf(("output_btab Malloc done\n"));
    for (i=0; i<nobj; i++) {
#ifdef SAVE_MASS
	output_btab[i].mass = btab2[i].mass;
#endif
	/* pos and vel set in Integrate() */
#ifdef SAVE_ACC
	VV(output_btab[i].acc, = btab2[i].acc);
	output_btab[i].phi = btab2[i].phi;
#endif
	output_btab[i].ident = btab2[i].ident;
    }
    /* Don't sort before Integrate_out or btab and output_btab */
    /* will not be in the same order */
    tacc = tpos;	   /* tpos and tvel advanced in Integrate()  */
    if (do_cosmology && do_periodic) {
	CosmoIntegrate(&btab2[0].mass, &btab2[0].pos[0], &btab2[0].vel[0],
		       &btab2[0].acc[0], &btab2[0].phi, sizeof(body),
		       &output_btab[0].pos[0], &output_btab[0].vel[0], sizeof(outbody),
		       nobj, dt, dtv, cosmo, &tpos, &tvel, ke, pe);
    } else {
	Integrate(&btab2[0].mass, &btab2[0].pos[0], &btab2[0].vel[0],
		  &btab2[0].acc[0], &btab2[0].phi, sizeof(body),
		  &output_btab[0].pos[0], &output_btab[0].vel[0], sizeof(outbody),
		  nobj, dt, dtv, &tpos, &tvel, ke, pe);
    }
    Free(btab);
    if (!isfinite(*pe)) *pe = 1e30;
    if (do_cosmology) {
	a = cosmo->a_at_t(cosmo, tpos);
	z = cosmo->z_at_t(cosmo, tpos);
	H = cosmo->H_at_t(cosmo, tpos);
	VV(sysradius, = a*R);
	VV(rmin, = -sysradius);
	VV(rmax, = sysradius);
        /* Do NOT convert velocities when subsampling.  Fixed 12/13/2014. ds14_a resort files are affected. */
        /* Adding subsample_velocities_ok flag to designate known good files */
	/* ConvertV(&output_btab[0].vel[0], sizeof(outbody), nobj, cosmo->a_at_t(cosmo, tvel), 1); */
	/* WrapPeriodic */
	for(b=output_btab; b<&output_btab[nobj]; b++) {
	    VVVV(if LPAREN b->pos, > rmax, RPAREN b->pos, -= 2.0*sysradius);
	    VVVV(if LPAREN b->pos, < rmin, RPAREN b->pos, += 2.0*sysradius);
	}
    } else {
	a = 0.0;
	z = 0.0;
	H = 0.0;
    }
    if (do_output && identsort_output) {
	pqsortsetup_order(&outputsort, output_btab, nobj,
			  sizeof(outbody), 0.1, 1, Realloc_f);
 	outputsort.method = 0;
 	outputsort.decomp_cycles = 1;
	output_btab = pqsort(&outputsort,
			     (pq_wgtproto)UnityCost, 
			     (pq_keyproto)OutIdentKey);
	nobj = outputsort.nobj;
	Msgf(("After output pqsort, %d outbodies\n", nobj));
    } else if (do_output) {	/* Need to re-sort, since positions were updated in Integrate */
	morton_xyz = 1;
	FixRsizeExact(rmin, rmax);
	pqsortsetup_order(&outputsort, output_btab, nobj,
			  sizeof(outbody), 0.1, 1, Realloc_f);
 	outputsort.method = 0;
 	outputsort.decomp_cycles = 1;
	output_btab = pqsort(&outputsort,
			     (pq_wgtproto)UnityCost, 
			     (pq_keyproto)GetKeyFastOut);
	nobj = outputsort.nobj;
	Msgf(("After output pqsort, %d outbodies\n", nobj));
    }
    MPMY_ICombine_Init(&req);
    MPMY_ICombine(ke, ke, 1, MPMY_DOUBLE, MPMY_SUM, req);
    MPMY_ICombine(pe, pe, 1, MPMY_DOUBLE, MPMY_SUM, req);
    MPMY_ICombine_Wait(req);

    if (write_nfiles) MPMY_Nfileio(1);
    singlPrintf("Writing %s\n", outname);
    SDFwrite64(outname, gnobj, 
	       nobj, output_btab, sizeof(outbody), OUTBODYDESC,
	       "version", SDF_INT, 2,
	       "version_2HOT", SDF_INT, 2,
	       "units_2HOT", SDF_INT, 2,
	       "npart", SDF_INT64, gnobj,
#ifndef SAVE_MASS
	       "particle_mass", SDF_FLOAT, particle_mass,
#endif
	       "iter", SDF_INT, iter,
	       "do_periodic", SDF_INT, do_periodic,
	       "tpos", SDF_DOUBLE, tpos,
	       "tvel", SDF_DOUBLE, tvel,
	       "tacc", SDF_DOUBLE, tacc,
	       "R0", SDF_FLOAT, R[2],
	       "L0", SDF_FLOAT, 2.0*R[2],
	       "redshift", SDF_DOUBLE, z,
	       "a", SDF_DOUBLE, a,
	       "a_tvel", SDF_DOUBLE, cosmo->a_at_t(cosmo, tvel),
	       "a_tacc", SDF_DOUBLE, cosmo->a_at_t(cosmo, tacc),
	       "Omega0", SDF_DOUBLE, cosmo->Omega0,
	       "Omega0_m", SDF_DOUBLE, cosmo->Omega0_m,
	       "Omega0_r", SDF_DOUBLE, cosmo->Omega0_r,
	       "Omega0_lambda", SDF_DOUBLE, cosmo->Omega0_lambda,
	       "Omega0_cdm", SDF_DOUBLE, cosmo->Omega0_cdm,
	       "Omega0_ncdm_tot", SDF_DOUBLE, cosmo->Omega0_ncdm_tot,
	       "Omega0_b", SDF_DOUBLE, cosmo->Omega0_b,
	       "Omega0_g", SDF_DOUBLE, cosmo->Omega0_g,
	       "Omega0_ur", SDF_DOUBLE, cosmo->Omega0_ur,
	       "Omega0_fld", SDF_DOUBLE, cosmo->Omega0_fld,
	       "w0_fld", SDF_DOUBLE, cosmo->w0_fld,
	       "wa_fld", SDF_DOUBLE, cosmo->wa_fld,
	       "h_100", SDF_DOUBLE, cosmo->h_100,
	       "H0", SDF_DOUBLE, cosmo->H0,
	       "hubble", SDF_DOUBLE, H,
	       "H", SDF_DOUBLE, H,
	       "Gnewt", SDF_DOUBLE, cosmo->Gnewt,
	       "growthfac", SDF_DOUBLE, cosmo->growthfac_at_z(cosmo, z),
	       "growthfac_tvel", SDF_DOUBLE, cosmo->growthfac_at_t(cosmo, tvel),
	       "growthfac_tacc", SDF_DOUBLE, cosmo->growthfac_at_t(cosmo, tacc),
	       "velfac", SDF_DOUBLE, cosmo->velfac_at_z(cosmo, z),
	       "tolerance", SDF_FLOAT, this_tol,
	       "frac_tolerance", SDF_FLOAT, frac_tol,
	       "frac_tolerance0", SDF_FLOAT, frac_tol0,
	       "Rx", SDF_FLOAT, R[0],
	       "Ry", SDF_FLOAT, R[1],
	       "Rz", SDF_FLOAT, R[2],
	       "Nx", SDF_INT, N[0],
	       "Ny", SDF_INT, N[1],
	       "Nz", SDF_INT, N[2],
	       "epsilon_mscale", SDF_FLOAT, eps,
	       "epsilon_scaled", SDF_FLOAT, this_eps_scaled,
	       "epsilon0", SDF_FLOAT, this_eps_scaled/a,
	       "force_smoothing_type", SDF_INT, force_smoothing_type,
	       "ic_Nmesh", SDF_INT, ic_Nmesh,
	       "ic_growthfac", SDF_DOUBLE, ic_growthfac,
	       "checkpoint", SDF_INT, checkpoint,
	       "morton_order", SDF_INT, morton_xyz,
	       "morton_xyz", SDF_INT, morton_xyz,
	       "npart_orig", SDF_INT64, gnobj0,
	       "subsample_fraction", SDF_DOUBLE, subsample_fraction,
	       "subsample_random_seed", SDF_INT, subsample_random_seed,
	       "subsample_velocities_ok", SDF_INT, 1,
	       "ke", SDF_DOUBLE, *ke,
	       "pe", SDF_DOUBLE, *pe,
	       "length_unit", SDF_STRING, "kpc", 
	       "mass_unit", SDF_STRING, "1e10 Msun", 
	       "time_unit", SDF_STRING, "Gyr", 
	       "velocity_unit", SDF_STRING, "kpc/Gyr",
	       "length_unit_cgs", SDF_DOUBLE, cm_kpc,
	       "mass_unit_cgs", SDF_DOUBLE, g_Msol10,
	       "time_unit_cgs", SDF_DOUBLE, one_Gyr,
	       "compiled_version_nln", SDF_STRING, version_nln,
	       "compiled_date_nln", SDF_STRING, compiled_date_nln,
	       "compiled_time_nln", SDF_STRING, compiled_time_nln,
	       "compiled_version_2HOT", SDF_STRING, version_2HOT,
	       "compiled_date_2HOT", SDF_STRING, compiled_date_2HOT,
	       "compiled_time_2HOT", SDF_STRING, compiled_time_2HOT,
	       NULL);
    if (write_nfiles) MPMY_Nfileio(0);
;
    Msgf(("Output to %s done\n", outname));
    Free(output_btab);
    if (subsample_fraction != 0.0) {
	Free((body *)btab2);
    }
}
