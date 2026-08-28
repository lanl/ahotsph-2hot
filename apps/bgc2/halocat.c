/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

/* Based on example code from rockstar. */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <math.h>
#include "Malloc.h"
#include "bgc2.h"
#include "io_util.h"
#include "check_syscalls.h"
#define NDIM 3
#include "vop.h"
#include "Malloc.h"
#include "SDF.h"
#include "SDFwrite.h"
#include "stk.h"
#include "macr.h"
#include "mpmy.h"
#include "singlio.h"
#include "version.h"


static GROUP_DATA_RMPVMAX *gd = NULL;
static double BOX_SIZE = 0.0;
static double MIN_HALO_OUTPUT_SIZE = 20.0;

#define FAST3TREE_TYPE GROUP_DATA_RMPVMAX
#define FAST3TREE_PREFIX HALOCAT
#ifdef __NO_INLINE__
#define inline
#endif
#include "../Rockstar/fast3tree.c"


#define GROUP_LIST gd
#define RADIUS radius
#define RADIUS_CONVERSION 1.0
#define parent parent_id
#include "../Rockstar/parents.c"
#undef parent

void
load_bgc2(char *filename, struct bgc2_header *hdr,
	  GROUP_DATA_RMPVMAX **groups, int64_t *num_groups,
	  PARTICLE_DATA_GPV **pdata, int64_t *num_parts)
{
  FILE *input;
  int64_t new_group_size, new_part_size;
  int64_t p_start;

  assert(sizeof(struct bgc2_header) == BGC2_HEADER_SIZE);
  input = check_fopen(filename, "rb");

  fread_fortran(hdr, BGC2_HEADER_SIZE, 1, input, 0);
  assert(hdr->magic == BGC_MAGIC);
  assert(hdr->version == 2);
  assert(hdr->format_group_data == GDATA_FORMAT_RMPVMAX);

  new_group_size = sizeof(GROUP_DATA_RMPVMAX)*((*num_groups)+hdr->ngroups);
  *groups = check_realloc(*groups, new_group_size, "Allocating groups.");
  fread_fortran((*groups) + (*num_groups), sizeof(GROUP_DATA_RMPVMAX), 
		hdr->ngroups, input, 0);
  
  /* Read PV particle data, expanding to GPV */
  new_part_size = sizeof(PARTICLE_DATA_GPV)*((*num_parts)+hdr->npart);
  p_start = (*num_parts);
  *pdata = check_realloc(*pdata, new_part_size, "Allocating pdata");
  PARTICLE_DATA_PV *pvdata = NULL;
  for (int64_t i = 0; i < hdr->ngroups; i++) {
      int64_t np = groups[0][(*num_groups)+i].npart;
      pvdata = check_realloc(pvdata, np*sizeof(PARTICLE_DATA_PV), "Allocating pvdata");
      fread_fortran(pvdata, sizeof(PARTICLE_DATA_PV), np, input, 0);
      for (int64_t j = 0; j < np; j++) {
	  (*pdata+j+p_start)->part_id = pvdata[j].part_id;
	  (*pdata+j+p_start)->group_id = (*groups+*num_groups+i)->id;
	  for (int k = 0; k < 3; k++) {
	      (*pdata+j+p_start)->pos[k] = pvdata[j].pos[k];
	      (*pdata+j+p_start)->vel[k] = pvdata[j].vel[k];
	  }
      }
      p_start += np;
  }
  pvdata = check_realloc(pvdata, 0, "Free pvdata");
  *num_groups += hdr->ngroups;
  *num_parts += hdr->npart;
  fclose(input);
}

int sort_by_id(const void *a, const void *b) {
  const GROUP_DATA_RMPVMAX *c = a;
  const GROUP_DATA_RMPVMAX *d = b;
  if (c->id < d->id) return -1;
  if (c->id > d->id) return 1;
  return 0;
}

int gpvsort_by_gid(const void *a, const void *b) {
  const PARTICLE_DATA_GPV *c = a;
  const PARTICLE_DATA_GPV *d = b;
  if (c->group_id < d->group_id) return -1;
  if (c->group_id > d->group_id) return 1;
  return 0;
}

int64_t
find_so_parent(int64_t parent_id, GROUP_DATA_RMPVMAX *gd, int64_t num_groups)
{
    GROUP_DATA_RMPVMAX *p, key;
    do {
	key.id = parent_id;
	p = bsearch(&key, gd, num_groups, sizeof(GROUP_DATA_RMPVMAX), sort_by_id);
	parent_id = p->parent_id;
    } while (parent_id != -1LL);
    return p->id;
}

#define sqr(x) ((x)*(x))
#define max(A,B) ((A) > (B) ? (A) : (B))
#define min(A,B) ((A) < (B) ? (A) : (B))
#define mabs(A) ((A) < 0.0 ? -(A) : (A))
#define cnint(x) ((x-floor(x)) < 0.5 ? floor(x) : ceil(x))
#define csign(x) (x < 0.0 ? -1 : 1)
/******************/
/*   FUNCTIONS    */
/******************/
#define IM1 2147483563
#define IM2 2147483399
#define AM (1.0/IM1)
#define IMM1 (IM1-1)
#define IA1 40014
#define IA2 40692
#define IQ1 53668
#define IQ2 52774
#define IR1 12211
#define IR2 3791
#define NTAB 32
#define NDIV (1+IMM1/NTAB)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

float ran2(int *idum)
{
	int j;
	int k;
	static int idum2=123456789;
	static int iy=0;
	static int iv[NTAB];
	float temp;

	if (*idum <= 0) {
		if (-(*idum) < 1) *idum=1;
		else *idum = -(*idum);
		idum2=(*idum);
		for (j=NTAB+7;j>=0;j--) {
			k=(*idum)/IQ1;
			*idum=IA1*(*idum-k*IQ1)-k*IR1;
			if (*idum < 0) *idum += IM1;
			if (j < NTAB) iv[j] = *idum;
		}
		iy=iv[0];
	}
	k=(*idum)/IQ1;
	*idum=IA1*(*idum-k*IQ1)-k*IR1;
	if (*idum < 0) *idum += IM1;
	k=idum2/IQ2;
	idum2=IA2*(idum2-k*IQ2)-k*IR2;
	if (idum2 < 0) idum2 += IM2;
	j=iy/NDIV;
	iy=iv[j]-idum2;
	iv[j] = *idum;
	if (iy < 1) iy += IMM1;
	if ((temp=AM*iy) > RNMX) return RNMX;
	else return temp;
}
#undef IM1
#undef IM2
#undef AM
#undef IMM1
#undef IA1
#undef IA2
#undef IQ1
#undef IQ2
#undef IR1
#undef IR2
#undef NTAB
#undef NDIV
#undef EPS
#undef RNMX


/*-Average-distribution------------------------------*/
int Average(double Nexp, int *seed)
{
    int Nact ;
    float rand ;

    rand = ran2(seed);

    if (rand <= (Nexp-(int)(Nexp))) Nact = (int)(Nexp+1);
    else Nact = (int)(Nexp);
    
    return Nact;
} 

/*-Poisson-distribution------------------------------*/
int Poisson(double Nexp, int *seed)
{
    int i,Nmax,Nact;
    double x,sigma,P,*Sum,rand;

    sigma = sqrt(Nexp);
    if(Nexp>=0.6) Nmax = (int)(10*sigma+Nexp);
    else Nmax = 8;
    Sum=(double *)Calloc(Nmax,sizeof(double));
    Nact = Nmax;

    P = exp(-Nexp);
    Sum[0] = P;
    for(i=1;i<Nmax;i++)
	{
	    x = (float)i;
	    P *= Nexp/x;
	    Sum[i] = Sum[i-1]+P;
	}

    rand = ran2(seed);
    for(i=0;i<Nmax;i++) {
	if(rand<=Sum[i]) {
	    Nact = i;
	    break;
	}
    }
    Free(Sum);
    return Nact;
}

#define MAXN 32000000
static double M0, M1, Mmin, sigmalogM, alpha, Dgamma, Dv;
static int ncatalogs = 1;
static FILE **fp3;
static Stk *outstk;

void
process_halo(GROUP_DATA_RMPVMAX *halo, PARTICLE_DATA_GPV *btab, int64_t n, double pmass, int seed)
{
    PARTICLE_DATA_GPV *p;
    double Mhalo;
    double cofm[NDIM];
    double cofmv[NDIM];
    float maxr;
    float dr[NDIM];
    float *rp;
    float mass;
    float cen[NDIM];
    float vcen[NDIM];
    float pos[NDIM];
    float vel[NDIM];
    int cat;

    int part;
    int Ncen, Nsat, *hist;
    float Ncexp, Nsexp;
    double rand, Pnorm, *Prob;
    
    if (n <= 0 || n >= MAXN) Error("Bad n %ld\n", n);
    rp = Malloc(n * sizeof(float));
    Mhalo = 0.0;
    VS(cofm, = 0.0);
    VS(cofmv, = 0.0);

    maxr = 0.0;
    Pnorm = 0.0;
    for (p = btab; p < btab+n; p++) {
	Mhalo += pmass;
	VV(pos, = p->pos);
	VV(vel, = p->vel);
	VVV(dr, = pos, - halo->pos);
	VV(cofm, += pmass*pos);
	VV(cofmv, += pmass*vel);
	rp[p-btab] = sqrt(Dot(dr, dr));
	/* assert(rp[p-btab] <= halo->radius+1e-3); fails for very small halos */
	if (rp[p-btab] > maxr) maxr = rp[p-btab];
	Pnorm += pow(rp[p-btab], Dgamma);
    }
    VS(cofm, /= Mhalo);
    VS(cofmv, /= Mhalo);
    assert(fabs(Mhalo/halo->mass-1.0) < 1e-7);

    VV(cen, = halo->pos);
    VV(vcen, = halo->vel);

    double arg = (log10(Mhalo)-log10(Mmin))/sigmalogM;
    Ncexp = 0.5 * (1.0 + erf(arg));
    
    Ncen = Average(Ncexp, &seed);
    assert(Ncen >= 0);
    Prob = Calloc(n, sizeof(double));
    hist = Calloc(n, sizeof(int));
    
    for (cat = 0; cat < ncatalogs; cat++) {
	if (Mhalo < M0) Nsexp=0;
	else Nsexp = Ncexp*pow(((Mhalo-M0)/M1), alpha);
	
	Nsat = Poisson(Nsexp, &seed);
	
	if (Ncen == 0 && Nsat > 0) {
	    Ncen = 1;
	    Nsat -= 1;
	}
	
	/*---Print-out-M-vs-N-in-PNMfile------------------*/
	
	if (fp3[cat] && ((cat == 0) || (Ncen+Nsat > 0))) {
	    fprintf(fp3[cat],"%7ld %8.8e %8.4f %8.4f %6ld %3d %3d %8.3f %8.3f %8.3f\n", 
		    halo->id, n*pmass, Ncexp, Nsexp,
		    n, Ncen, Nsat, 
		    cen[0], cen[1], cen[2]); 
	}

	/*---Force-a-galaxy-at-halo-center----------------*/

	if (Ncen > 0) {
	    mass = Mhalo;
	    VV(pos, = cen);
	    VV(vel, = vcen);
	    StkPushData(&outstk[cat], &mass, sizeof(float));
	    StkPushData(&outstk[cat], pos, NDIM*sizeof(float));
	    StkPushData(&outstk[cat], vel, NDIM*sizeof(float));
	    StkPushData(&outstk[cat], &halo->id, sizeof(int64_t));
	}

	/*---Only-continue-if-at-least-one-satellite--------*/

	if (Nsat == 0) continue;

	/*---Set-up-probabilities---------------------------*/
    
	Prob[0] = pow(rp[0],Dgamma)/Pnorm;
	for (part = 1; part < n; part++) {
	    Prob[part] = Prob[part-1] + pow(rp[part],Dgamma)/Pnorm ;
	    hist[part] = 0 ;
	}
	if (mabs(Prob[part-1]-1.0) >= 1e-9) {
	    Error("Error in setting up particle probabilities for halo #%ld\n", halo->id);
	}
	Prob[part-1] = 1.0;
	
	/*---For-each-galaxy-randomly-choose-a-particle-----*/
	
	int nchosen = 0;
	for (int i = 0; i < Nsat; i++) {
	    rand = ran2(&seed);
	    for (part = 0; part < n; part++) if (rand <= Prob[part]) break;
	    while (hist[part] == 1) {
		rand = ran2(&seed);
		for (part = 0; part < n; part++) if (rand <= Prob[part]) break;
	    }
	    hist[part] = 1;
	    
	    VV(vel, = btab[part].vel); /* peculiar vel km/s */
	    VV(vel, -= vcen);
	    VS(vel, *= Dv);
	    VV(vel, += vcen);
	    
	    mass = Mhalo;
	    VV(pos, = btab[part].pos);
	    StkPushData(&outstk[cat], &mass, sizeof(float));
	    StkPushData(&outstk[cat], pos, NDIM*sizeof(float));
	    StkPushData(&outstk[cat], vel, NDIM*sizeof(float));
	    StkPushData(&outstk[cat], &halo->id, sizeof(int64_t));
	    nchosen++;
	}
    }
    Free(rp);
    Free(Prob);
    Free(hist);
}

typedef struct {
    float mass;
    float pos[NDIM];
    float vel[NDIM];
    int64_t ident;
} __attribute__ ((packed)) outbody;


#define OUTBODYDESC \
"struct {\n\
    float mass;			/* mass of halo from halo finder */\n\
    float x, y, z;		/* position of mock galaxy */\n\
    float vx, vy, vz;		/* velocity of mock galaxy */\n\
    int64_t ident;		/* halo id */\n\
}"


int
main(int argc, char *argv[])
{
    int64_t num_groups = 0;
    int64_t num_parts = 0;
    struct bgc2_header hdr = {};
    PARTICLE_DATA_GPV *parts = NULL;
    SDF  *sdfp;
    int write_pnm = 0;
    char pnmfile[256], pnmname[256];
    char outfile[256], outname[256];
    char infilespec[256];
    int nfiles;
    int seed;
    Timer_t TotalTm;

    MPMY_Init(&argc, &argv);
    singlPrintf("%s\n\tversion %s %s %s\n", argv[0], Version, Compiled_date, Compiled_time);

    EnableTimer(&TotalTm, "Total");
    StartTimer(&TotalTm);
    if ((sdfp = SDFopen(NULL, argv[1])) == NULL) {
 	SinglError("Sorry, couldn't SDFopen %s\n%s\n",
		   argv[1], SDFerrstring);
    }

    SDFgetdoubleOrDie(sdfp, "M0",  &M0);
    SDFgetdoubleOrDie(sdfp, "M1",  &M1);
    SDFgetdoubleOrDie(sdfp, "Mmin",  &Mmin);
    SDFgetdoubleOrDie(sdfp, "sigmalogM",  &sigmalogM);
    SDFgetdoubleOrDie(sdfp, "alpha",  &alpha);
    SDFgetdoubleOrDie(sdfp, "Dgamma",  &Dgamma);
    SDFgetdoubleOrDie(sdfp, "Dv",  &Dv);
    SDFgetintOrDefault(sdfp, "seed",  &seed, -1);
    SDFgetintOrDefault(sdfp, "ncatalogs",  &ncatalogs, 1);
    SDFgetintOrDefault(sdfp, "write_pnm", &write_pnm, 0);
    SDFgetstring(sdfp, "pnmfile", pnmfile, sizeof (pnmfile));
    SDFgetstring(sdfp, "outfile", outfile, sizeof(outfile));
    SDFgetstring(sdfp, "infilespec", infilespec, sizeof(infilespec));
    SDFgetintOrDefault(sdfp, "nfiles", &nfiles, 1);
    singlPrintf("Got parameters from %s\n", argv[1]);
    SDFclose(sdfp);

    fp3 = Malloc(ncatalogs * sizeof(FILE *));
    outstk = Malloc(ncatalogs * sizeof(Stk));
    for (int i = 0; i < ncatalogs; i++) {
	StkInitEz(&outstk[i]);
	if (write_pnm && (i == 0) && (MPMY_Procnum() == 0)) {
	    snprintf(pnmname, sizeof(pnmname), "%s.%d.txt", pnmfile, i);
	    Fopen(fp3[i], pnmname, "w");
	    fprintf(fp3[i],"# id mass Ncexp Nsexp Npart Ncen Nsat x y z\n");
	} else fp3[i] = NULL;
    }

    int nfiles_per_proc = nfiles/MPMY_Nproc();
    if (nfiles % MPMY_Nproc()) nfiles_per_proc++;
    int nstart = MPMY_Procnum()*nfiles_per_proc;
    int nend = (MPMY_Procnum()+1)*nfiles_per_proc;
    if (nstart > nfiles) nstart = nfiles;
    if (nend > nfiles) nend = nfiles;
    for (int filen = nstart; filen < nend; filen++) {
	char infile[256];
	sprintf(infile, infilespec, filen);
	load_bgc2(infile, &hdr, &gd, &num_groups, &parts, &num_parts);
	BOX_SIZE = hdr.box_size;

	find_parents(num_groups);

	qsort(gd, num_groups, sizeof(GROUP_DATA_RMPVMAX), sort_by_id);

	/* Remove subhalos and those below minimum */
	int64_t num_halos = 0;
	int64_t pindex = 0;
	for (int64_t i = 0; i < num_groups; i++) {
	    if ((gd[i].parent_id == -1LL) && (gd[i].mass/hdr.part_mass >= MIN_HALO_OUTPUT_SIZE-0.001)) {
		num_halos++;
		process_halo(gd+i, parts+pindex, gd[i].npart, hdr.part_mass, 1);
	    }
	    pindex += gd[i].npart;
	}
	fprintf(stderr, "%d - %s %ld halos from %ld groups, %ld particles\n", 
		MPMY_Procnum(), infile, num_halos, num_groups, num_parts);
	num_groups = num_parts = 0;
    }
    if (nend > nstart) {
	Free(parts); parts = NULL;
	Free(gd); gd = NULL;
    }
	
    for (int i = 0; i < ncatalogs; i++) {
	outbody *output_btab;
	int64_t onobj, gonobj;
	
	if (fp3[i]) Fclose(fp3[i]);

	snprintf(outname, sizeof(outname), "%s.%d.sdf", outfile, i);
	output_btab = StkBase(&outstk[i]);
	gonobj = onobj = StkSz(&outstk[i])/sizeof(outbody);
	MPMY_Combine(&gonobj, &gonobj, 1, MPMY_INT64, MPMY_SUM);

	SDFwrite64(outname, gonobj, 
		   onobj, output_btab, sizeof(outbody),
		   OUTBODYDESC,
		   "npart", SDF_INT64, gonobj,
		   "redshift", SDF_DOUBLE, hdr.redshift,
		   "R0", SDF_DOUBLE, hdr.box_size/2.0,
		   "BOX_SIZE", SDF_DOUBLE, hdr.box_size,
		   "Omega0", SDF_DOUBLE, hdr.Omega0,
		   "Omega0_Lambda", SDF_DOUBLE, hdr.OmegaLambda,
		   "h_100", SDF_DOUBLE, hdr.Hubble0,
		   "seed", SDF_INT, seed,
		   "catalog", SDF_INT, i,
		   "M0", SDF_DOUBLE, M0,
		   "M1", SDF_DOUBLE, M1,
		   "Mmin", SDF_DOUBLE, Mmin,
		   "sigmalogM", SDF_DOUBLE, sigmalogM,
		   "alpha", SDF_DOUBLE, alpha,
		   "Dgamma", SDF_DOUBLE, Dgamma,
		   "Dv", SDF_DOUBLE, Dv,
		   "rockstar_units", SDF_INT, 1,
		   "compiled_version", SDF_STRING, Version,
		   "compiled_date", SDF_STRING, Compiled_date,
		   "compiled_time", SDF_STRING, Compiled_time,
		   NULL);
	singlPrintf("\nOutput %ld halos to %s.\n", gonobj, outname);
    }
    StopTimer(&TotalTm);
    OutputTimers(singlPrintf);

    exit(0);
}
