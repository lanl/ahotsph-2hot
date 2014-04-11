#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <complex.h>
#include <fftw3-mpi.h>
#define NDIM 3
#include "vop.h"
#include "mpmy.h"
#include "SDFread.h"
#include "SDFwrite.h"
#include "image.h"
#include "singlio.h"
#include "error.h"
#include "Malloc.h"
#include "timers.h"
#include "mpmy_io.h"
#include "pqsort.h"
#include "decomp.h"
#include "macr.h"
#include "Msgs.h"
#include "memfile.h"
#include "cosmo.h"

#define idx(i,j,k) (((i-SlabBegin)*MaxMeshIndex+j)*MaxMeshIndex+k)
#define idx2(i,j,k) ((i*MaxMeshIndex+j)*MaxMeshIndex+k)
#define BIN_MULTIPLIER 1
#define MAXCOUNTINDEX (0.9*Nmesh*BIN_MULTIPLIER) /* sqrt(3)/2 + delta */
#define EPS 3.0e-6

typedef enum { NGP, CIC, TSC } Interp_method;

typedef struct {
    float mass;			/* mass of body */
    float pos[NDIM];		/* position of body */
} body;

static double Rmin[NDIM];
static double Rsize;
static double MeshFactor;
static int MaxMeshIndex;
static int SlabBegin;
static int SlabSize;
static int DoNGP;

Key_t GetKeySlab(const body *p);
float UnityCost(const void *p);

#define MAXTFTABLE 8192
#define BIG 1e30

#define SAMPLING_N 52

static float *sampling_corr;

/* These were calculcated numerically with sampling.c */
static float sampling_x[SAMPLING_N] = {
0.0000, 0.0100, 0.0200, 0.0300, 0.0400, 0.0500, 0.0600, 0.0700,
0.0800, 0.0900, 0.1000, 0.1100, 0.1200, 0.1300, 0.1400, 0.1500,
0.1600, 0.1700, 0.1800, 0.1900, 0.2000, 0.2100, 0.2200, 0.2300,
0.2400, 0.2500, 0.2600, 0.2700, 0.2800, 0.2900, 0.3000, 0.3100,
0.3200, 0.3300, 0.3400, 0.3500, 0.3600, 0.3700, 0.3800, 0.3900,
0.4000, 0.4100, 0.4200, 0.4300, 0.4400, 0.4500, 0.4600, 0.4700,
0.4800, 0.4900, 0.5000};

static float sampling_ngp3[SAMPLING_N] = {
1.0000, 0.9997, 0.9987, 0.9970, 0.9947, 0.9918, 0.9882, 0.9840,
0.9791, 0.9737, 0.9676, 0.9609, 0.9536, 0.9458, 0.9374, 0.9285,
0.9190, 0.9091, 0.8987, 0.8878, 0.8765, 0.8648, 0.8527, 0.8403,
0.8276, 0.8146, 0.8014, 0.7880, 0.7745, 0.7608, 0.7471, 0.7335,
0.7199, 0.7064, 0.6932, 0.6802, 0.6676, 0.6555, 0.6439, 0.6329,
0.6228, 0.6135, 0.6052, 0.5981, 0.5923, 0.5881, 0.5854, 0.5847,
0.5861, 0.5898, 0.5961};

static float sampling_cic3[SAMPLING_N] = {
1.0000, 0.9993, 0.9974, 0.9941, 0.9895, 0.9837, 0.9766, 0.9682,
0.9587, 0.9480, 0.9362, 0.9233, 0.9093, 0.8944, 0.8785, 0.8618,
0.8442, 0.8258, 0.8068, 0.7871, 0.7668, 0.7461, 0.7249, 0.7033,
0.6814, 0.6592, 0.6369, 0.6145, 0.5920, 0.5695, 0.5472, 0.5250,
0.5030, 0.4813, 0.4600, 0.4390, 0.4186, 0.3986, 0.3793, 0.3607,
0.3428, 0.3257, 0.3095, 0.2943, 0.2801, 0.2671, 0.2554, 0.2450,
0.2361, 0.2289, 0.2234};

static float sampling_tsc3[SAMPLING_N] = {
1.0000, 0.9990, 0.9961, 0.9912, 0.9843, 0.9756, 0.9651, 0.9527,
0.9387, 0.9230, 0.9058, 0.8872, 0.8671, 0.8459, 0.8234, 0.8000,
0.7756, 0.7505, 0.7247, 0.6983, 0.6715, 0.6444, 0.6171, 0.5897,
0.5623, 0.5350, 0.5080, 0.4813, 0.4551, 0.4293, 0.4041, 0.3795,
0.3557, 0.3326, 0.3103, 0.2889, 0.2683, 0.2487, 0.2301, 0.2124,
0.1957, 0.1801, 0.1654, 0.1519, 0.1393, 0.1279, 0.1176, 0.1084,
0.1003, 0.0935, 0.0879};

static float sampling_D2[SAMPLING_N];


/* from NR, modified for zero-offset */
void spline(float x[], float y[], int n, float yp1, float ypn, float y2[])
{
	int i,k;
	float p,qn,sig,un,*u;

	u=Malloc(n*sizeof(float));
	if (yp1 > 0.99e30)
		y2[0]=u[0]=0.0;
	else {
		y2[0] = -0.5;
		u[0]=(3.0/(x[1]-x[0]))*((y[1]-y[0])/(x[1]-x[0])-yp1);
	}
	for (i=1;i<n-1;i++) {
		sig=(x[i]-x[i-1])/(x[i+1]-x[i-1]);
		p=sig*y2[i-1]+2.0;
		y2[i]=(sig-1.0)/p;
		u[i]=(y[i+1]-y[i])/(x[i+1]-x[i]) - (y[i]-y[i-1])/(x[i]-x[i-1]);
		u[i]=(6.0*u[i]/(x[i+1]-x[i-1])-sig*u[i-1])/p;
	}
	if (ypn > 0.99e30)
		qn=un=0.0;
	else {
		qn=0.5;
		un=(3.0/(x[n-1]-x[n-2]))*(ypn-(y[n-1]-y[n-2])/(x[n-1]-x[n-2]));
	}
	y2[n-1]=(un-qn*u[n-2])/(qn*y2[n-2]+1.0);
	for (k=n-2;k>=0;k--)
		y2[k]=y2[k]*y2[k+1]+u[k];
	Free(u);
}

/* from NR, modified for zero-offset */
void splint(float xa[], float ya[], float y2a[], int n, float x, float *y)
{
	int klo,khi,k;
	float h,b,a;

	klo=0;
	khi=n-1;
	while (khi-klo > 1) {
		k=(khi+klo) >> 1;
		if (xa[k] > x) khi=k;
		else klo=k;
	}
	h=xa[khi]-xa[klo];
	if (h == 0.0) Error("Bad xa input to routine splint");
	a=(xa[khi]-x)/h;
	b=(x-xa[klo])/h;
	*y=a*ya[klo]+b*ya[khi]+((a*a*a-a)*y2a[klo]+(b*b*b-b)*y2a[khi])*(h*h)/6.0;
}

void
do_slab_decomp(body **btab, int *nobj, ptrdiff_t local_nx)
{
    sortresult_t sortedbtab;
    Key_t *decomptab;
    int i;
    Key_t tmp;

    decomptab = Malloc(MPMY_Nproc() * sizeof(Key_t));
    for (i = 0; i < MPMY_Nproc(); i++) {
	tmp.k[0] = local_nx*(i+1)-1;
#if NK==2
	tmp.k[1] = 0;
#endif
	/* mask_decomp_keys */
	decomptab[i] = KeyLshift(tmp,24);
    }
    SetDecomp19(decomptab);

    pqsortsetup_order(&sortedbtab, *btab, *nobj,
		      sizeof(body), 0.1, 1, Realloc_f);
    pqsort(&sortedbtab,(pq_wgtproto)UnityCost, (pq_keyproto)GetKeySlab);
    *btab = sortedbtab.data;
    *nobj = sortedbtab.nobj;
}

void
SetMesh(int nmesh, double *rmin, double rsize, int fold_factor)
{
    singlPrintf("SetMesh: %d (%g, %g, %g) %g\n", nmesh, rmin[0], rmin[1], rmin[2], rsize);
    Rsize = rsize;
    VV(Rmin, = rmin);
    MaxMeshIndex = nmesh;
    MeshFactor = fold_factor*nmesh/Rsize;
    SlabSize = nmesh/MPMY_Nproc();
    SlabBegin = MPMY_Procnum()*SlabSize;
}

void
exchange_boundary(fftwf_complex *bnd, int nmesh)
{
    fftwf_complex *inbuf;
    int i, up, down;
    MPMY_Comm_request sreq, rreq;
    MPMY_Status stat;

    inbuf = Malloc(sizeof(fftwf_complex)*nmesh);
    up = (MPMY_Procnum()+1)%MPMY_Nproc();
    down = (MPMY_Procnum()+MPMY_Nproc()-1)%MPMY_Nproc();
    for (i = 0; i < nmesh; i++) {
      MPMY_Irecv(inbuf, nmesh*sizeof(fftwf_complex), down, 121, &rreq);
      MPMY_Isend(bnd+i*nmesh, nmesh*sizeof(fftwf_complex), up, 121, &sreq);
      MPMY_Wait2(rreq, &stat, sreq, 0);
      memcpy(bnd+i*nmesh, inbuf, nmesh*sizeof(fftwf_complex));
    }
    Free(inbuf);
}


void
tsc_density(float *pos, fftwf_complex *rho, float wgt)
{
    int iposs[NDIM], iposm[NDIM], iposp[NDIM];
    float fpos[NDIM];
    float h[NDIM], hs[NDIM], hm[NDIM], hp[NDIM];

    VVVS(fpos, = LPAREN MeshFactor*LPAREN pos, - Rmin, RPAREN RPAREN);
    VV(iposs, = (int)fpos);
    VS(fpos, -= (float)0.5);
    VV(iposp, = 1 + iposs);
    VV(iposm, = -1 + iposs);
    if (iposs[0] >= MaxMeshIndex || iposs[0] < 0 ||
	iposs[1] >= MaxMeshIndex || iposs[1] < 0 ||
	iposs[2] >= MaxMeshIndex || iposs[2] < 0) {
      Error("Bad index (%d, %d, %d) for (%g,%g,%g)\n", 
	    iposs[0], iposs[1], iposs[2],
	    pos[0], pos[1], pos[2]);
    }
    if (iposp[0] == MaxMeshIndex) iposp[0] = 0;
    if (iposp[1] == MaxMeshIndex) iposp[1] = 0;
    if (iposp[2] == MaxMeshIndex) iposp[2] = 0;
    if (iposm[0] == -1) iposm[0] = MaxMeshIndex-1;
    if (iposm[1] == -1) iposm[1] = MaxMeshIndex-1;
    if (iposm[2] == -1) iposm[2] = MaxMeshIndex-1;
	
    VVV(h, = fpos, - (float)iposs);
    hs[0] = 0.75F - h[0] * h[0];
    hs[1] = 0.75F - h[1] * h[1];
    hs[2] = 0.75F - h[2] * h[2];
    hp[0] = 0.5F * (0.5F + h[0]) * (0.5F + h[0]);
    hp[1] = 0.5F * (0.5F + h[1]) * (0.5F + h[1]);
    hp[2] = 0.5F * (0.5F + h[2]) * (0.5F + h[2]);
    hm[0] = 0.5F * (0.5F - h[0]) * (0.5F - h[0]);
    hm[1] = 0.5F * (0.5F - h[1]) * (0.5F - h[1]);
    hm[2] = 0.5F * (0.5F - h[2]) * (0.5F - h[2]);
    

    /* TSC density interpolation */
    rho[idx(iposm[0], iposm[1], iposm[2])] += wgt*hm[0]*hm[1]*hm[2];
    rho[idx(iposs[0], iposm[1], iposm[2])] += wgt*hs[0]*hm[1]*hm[2];
    rho[idx(iposp[0], iposm[1], iposm[2])] += wgt*hp[0]*hm[1]*hm[2];
    rho[idx(iposm[0], iposs[1], iposm[2])] += wgt*hm[0]*hs[1]*hm[2];
    rho[idx(iposs[0], iposs[1], iposm[2])] += wgt*hs[0]*hs[1]*hm[2];
    rho[idx(iposp[0], iposs[1], iposm[2])] += wgt*hp[0]*hs[1]*hm[2];
    rho[idx(iposm[0], iposp[1], iposm[2])] += wgt*hm[0]*hp[1]*hm[2];
    rho[idx(iposs[0], iposp[1], iposm[2])] += wgt*hs[0]*hp[1]*hm[2];
    rho[idx(iposp[0], iposp[1], iposm[2])] += wgt*hp[0]*hp[1]*hm[2];
    rho[idx(iposm[0], iposm[1], iposs[2])] += wgt*hm[0]*hm[1]*hs[2];
    rho[idx(iposs[0], iposm[1], iposs[2])] += wgt*hs[0]*hm[1]*hs[2];
    rho[idx(iposp[0], iposm[1], iposs[2])] += wgt*hp[0]*hm[1]*hs[2];
    rho[idx(iposm[0], iposs[1], iposs[2])] += wgt*hm[0]*hs[1]*hs[2];
    rho[idx(iposs[0], iposs[1], iposs[2])] += wgt*hs[0]*hs[1]*hs[2];
    rho[idx(iposp[0], iposs[1], iposs[2])] += wgt*hp[0]*hs[1]*hs[2];
    rho[idx(iposm[0], iposp[1], iposs[2])] += wgt*hm[0]*hp[1]*hs[2];
    rho[idx(iposs[0], iposp[1], iposs[2])] += wgt*hs[0]*hp[1]*hs[2];
    rho[idx(iposp[0], iposp[1], iposs[2])] += wgt*hp[0]*hp[1]*hs[2];
    rho[idx(iposm[0], iposm[1], iposp[2])] += wgt*hm[0]*hm[1]*hp[2];
    rho[idx(iposs[0], iposm[1], iposp[2])] += wgt*hs[0]*hm[1]*hp[2];
    rho[idx(iposp[0], iposm[1], iposp[2])] += wgt*hp[0]*hm[1]*hp[2];
    rho[idx(iposm[0], iposs[1], iposp[2])] += wgt*hm[0]*hs[1]*hp[2];
    rho[idx(iposs[0], iposs[1], iposp[2])] += wgt*hs[0]*hs[1]*hp[2];
    rho[idx(iposp[0], iposs[1], iposp[2])] += wgt*hp[0]*hs[1]*hp[2];
    rho[idx(iposm[0], iposp[1], iposp[2])] += wgt*hm[0]*hp[1]*hp[2];
    rho[idx(iposs[0], iposp[1], iposp[2])] += wgt*hs[0]*hp[1]*hp[2];
    rho[idx(iposm[0], iposp[1], iposp[2])] += wgt*hp[0]*hp[1]*hp[2];
}

void
cic_density(float *pos, fftwf_complex *rho, float wgt, fftwf_complex *bnd)
{
    int ipos[NDIM], ipos1[NDIM];
    double fpos[NDIM];
    double d[NDIM];
    double t[NDIM];

    VVVS(fpos, = LPAREN MeshFactor*LPAREN pos, - Rmin, RPAREN RPAREN);
    VS(fpos, += 0.5);
    while (fpos[0] >= MaxMeshIndex) fpos[0] -= MaxMeshIndex;
    while (fpos[1] >= MaxMeshIndex) fpos[1] -= MaxMeshIndex;
    while (fpos[2] >= MaxMeshIndex) fpos[2] -= MaxMeshIndex;
    VV(ipos, = (int)fpos);
    VVV(d, = fpos, - (double)ipos);
    VV(t, = 1.0 - d);
    VV(ipos1, = 1 + ipos);

    
    if (ipos1[1] == MaxMeshIndex) ipos1[1] = 0;
    if (ipos1[2] == MaxMeshIndex) ipos1[2] = 0;
    if (ipos[0] < SlabBegin || ipos[0] >= SlabBegin+SlabSize) {
      Error("Node %d: Bad index (%d) %14.8g\n", 
	    MPMY_Procnum(), ipos[0], pos[0]);
    }
    if (ipos1[0] <= SlabBegin || ipos1[0] > SlabBegin+SlabSize) {
      Error("Node %d: Bad index (%d) %14.8g\n", 
	    MPMY_Procnum(), ipos[0], pos[0]);
    } 
    if (ipos[0] >= MaxMeshIndex || ipos[0] < 0 ||
	ipos[1] >= MaxMeshIndex || ipos[1] < 0 ||
	ipos[2] >= MaxMeshIndex || ipos[2] < 0) {
      fprintf(stderr, "Bad index (%d, %d, %d) for (%g,%g,%g)\n", 
	      ipos[0], ipos[1], ipos[2], pos[0], pos[1], pos[2]);
      return;
    }
    /* CIC density interpolation */
    if (ipos1[0] == SlabBegin+SlabSize) {
      bnd[idx2(0, ipos[1], ipos[2])] += wgt*d[0]*t[1]*t[2];
      bnd[idx2(0, ipos1[1], ipos[2])] += wgt*d[0]*d[1]*t[2];
      bnd[idx2(0, ipos[1], ipos1[2])] += wgt*d[0]*t[1]*d[2];
      bnd[idx2(0, ipos1[1], ipos1[2])] += wgt*d[0]*d[1]*d[2];
    } else {
      rho[idx(ipos1[0], ipos[1], ipos[2])] += wgt*d[0]*t[1]*t[2];
      rho[idx(ipos1[0], ipos1[1], ipos[2])] += wgt*d[0]*d[1]*t[2];
      rho[idx(ipos1[0], ipos[1], ipos1[2])] += wgt*d[0]*t[1]*d[2];
      rho[idx(ipos1[0], ipos1[1], ipos1[2])] += wgt*d[0]*d[1]*d[2];
    }
    rho[idx(ipos[0], ipos[1], ipos[2])] += wgt*t[0]*t[1]*t[2];
    rho[idx(ipos[0], ipos1[1], ipos[2])] += wgt*t[0]*d[1]*t[2];
    rho[idx(ipos[0], ipos[1], ipos1[2])] += wgt*t[0]*t[1]*d[2];
    rho[idx(ipos[0], ipos1[1], ipos1[2])] += wgt*t[0]*d[1]*d[2];
}

void
ngp_density(float *pos, fftwf_complex *rho, float wgt)
{
    int ipos[NDIM];
  
    VVVS(ipos, = (int)LPAREN MeshFactor*LPAREN pos, - Rmin, RPAREN RPAREN);
    while (ipos[0] >= MaxMeshIndex) ipos[0] -= MaxMeshIndex;
    while (ipos[1] >= MaxMeshIndex) ipos[1] -= MaxMeshIndex;
    while (ipos[2] >= MaxMeshIndex) ipos[2] -= MaxMeshIndex;
    if (ipos[0] < SlabBegin || ipos[0] >= SlabBegin+SlabSize) {
	Error("Node %d: Bad index (%d) %14.8g\n", 
	      MPMY_Procnum(), ipos[0], pos[0]);
    }
    rho[idx(ipos[0], ipos[1], ipos[2])] += wgt;
}

void
do_pspec(int Nmesh, int ic_Nmesh, float L0mpc, int fold_factor, float fft_kmax, fftwf_complex *d, int *counts, double *kbin, double *sum, double *linear_sum, double *scorrect_sum, double *cic_sum, float *Ktab, float *Tftab, float *D2, int Ntable, float CICAlpha, int write_modes)
{
    int i, j, k;
    int ii, jj, kk;
    float kindex;
    float real, imag;
    float mag;
    float logwavenum;
    int index;
    float scorrect;

    for (i = SlabBegin; i < SlabBegin+SlabSize; i++) {
      for (j = 0; j < Nmesh; j++) {
	for (k = 0; k < Nmesh; k++) {

	  if (i <= Nmesh/2) ii = i; else ii = Nmesh-i;
	  if (j <= Nmesh/2) jj = j; else jj = Nmesh-j;
	  if (k <= Nmesh/2) kk = k; else kk = Nmesh-k;

	  kindex = sqrt(ii*ii+jj*jj+kk*kk);
	  logwavenum = log(fold_factor*kindex/L0mpc);
	  splint(sampling_x, sampling_corr, sampling_D2, SAMPLING_N, 
		 kindex/Nmesh, &scorrect);
	  real = creal(d[idx(i,j,k)]);
	  imag = cimag(d[idx(i,j,k)]);
	  mag = real*real+imag*imag;
	  index = (int)(BIN_MULTIPLIER*kindex+0.5);
	  if (write_modes) {
	      printf("%2d %2d %2d %12.8f %12.8f %12.8f %12.8f\n", 
		     i, j, k, creal(d[idx(i,j,k)]), cimag(d[idx(i,j,k)]), cabs(d[idx(i,j,k)]), carg(d[idx(i,j,k)]));
	  }
	  if (index >= MAXCOUNTINDEX) Error("index is %d\n", (int)index);
	  if (kindex < fft_kmax*BIN_MULTIPLIER) {
	    counts[index]++;
	    kbin[index] += logwavenum;
	    sum[index] += mag;
	    scorrect_sum[index] += 1.0/scorrect;
	  }
	}
      }
    }
    MPMY_Combine(counts, counts, MAXCOUNTINDEX, MPMY_INT, MPMY_SUM);
    MPMY_Combine(kbin, kbin, MAXCOUNTINDEX, MPMY_DOUBLE, MPMY_SUM);
    MPMY_Combine(sum, sum, MAXCOUNTINDEX, MPMY_DOUBLE, MPMY_SUM);
    MPMY_Combine(linear_sum, linear_sum, MAXCOUNTINDEX, MPMY_DOUBLE, MPMY_SUM);
    MPMY_Combine(scorrect_sum, scorrect_sum, MAXCOUNTINDEX, MPMY_DOUBLE, MPMY_SUM);
    MPMY_Combine(cic_sum, cic_sum, MAXCOUNTINDEX, MPMY_DOUBLE, MPMY_SUM);
}


int
main(int argc, char **argv)
{
    SDF *csdfp, *sdfp;
    char name[256], outname[256];
    int64_t gnobj;
    int nobj;
    body *btab;
    int massconf, xconf, yconf, zconf;
    Timer_t StepTot;
    int read_nfiles;
    double M0;
    float min_mass, max_mass;
    float max_mode;
    int64_t nsamples;
    int mass_weighted;
    int interp_method;
    body *p;
    int write_modes, write_modes_complex, write_rho;
    double rmin[NDIM], rmax[NDIM], rsize;
    float xmin, xmax;
    int Nmesh = 32;
    ptrdiff_t Nw;
    Timer_t FFTTm;
    fftwf_complex *d;
    fftwf_plan plan;
    ptrdiff_t total_local_size, local_nx, local_x_start;
    FILE *outfile = NULL;
    int *counts;
    double *kbin;
    double *sum;
    double *linear_sum, *scorrect_sum, *cic_sum;
    float Ktab[MAXTFTABLE];	/* h/Mpc */
    float Tftab[MAXTFTABLE];
    float D2[MAXTFTABLE];
    int fold_factor;
    double redshift;
    float R0;
    double h_100, growthfac, ic_growthfac;
    char powspec_version[] = "powspec-0.8";

    MPMY_Init(&argc, &argv);
    fftwf_mpi_init();

    char msgfile[256];
    sprintf(msgfile, "msgs/msg.%d", MPMY_Procnum());
    MsgdirInit(msgfile);
    Msg_turnon("mpmy_mpi.c,mpmy_mpiio.c");

    EnableWCTimer(&FFTTm, "FFT Time");
    if (argc != 3) {
	singlPrintf("usage: %s powspec.ctl file.sdf\n", argv[0]);
	exit(1);
    }
    singlPrintf("Welcome to the machine\n");
    
    csdfp = SDFopen(0, argv[1]);
    if (csdfp == 0)
	SinglError("Sorry, couldn't SDFopen %s\n%s\n", argv[1], SDFerrstring);

    SDFgetintOrDefault(csdfp, "read_nfiles", &read_nfiles, 0);
    SDFgetintOrDefault(csdfp, "Nmesh", &Nmesh, 1024);
    SDFgetintOrDefault(csdfp, "write_rho", &write_rho, 0);
    SDFgetintOrDefault(csdfp, "write_modes", &write_modes, 0);
    SDFgetintOrDefault(csdfp, "write_modes_complex", &write_modes_complex, 1);
    SDFgetintOrDefault(csdfp, "mass_weighted", &mass_weighted, 0);
    SDFgetintOrDefault(csdfp, "interp_method", (int *)&interp_method, CIC);
    SDFgetintOrDefault(csdfp, "fold_factor", &fold_factor, 1);
    SDFgetfloatOrDefault(csdfp, "min_mass",  &min_mass, 0.0);
    SDFgetfloatOrDefault(csdfp, "max_mass",  &max_mass, 1e20);
    SDFgetfloatOrDefault(csdfp, "max_mode",  &max_mode, 1.0);
    SDFgetstring(csdfp, "outfile", outname, sizeof(outname));

    singlPrintf("Reading \"%s\"\n", argv[2]);
    if (read_nfiles) MPMY_Nfileio(1);
    sdfp = SDFreadf64(NULL, argv[2], (void **)&btab, &gnobj, &nobj, sizeof(body),
		   "mass", offsetof(body, mass), &massconf,
		   "x", offsetof(body, pos[0]), &xconf,
		   "y", offsetof(body, pos[1]), &yconf,
		   "z", offsetof(body, pos[2]), &zconf,
		   NULL);
    if (read_nfiles) MPMY_Nfileio(0);
    if (xconf==0 || yconf==0 || zconf==0) {
	SinglError("Could not find %s %s %s in data file!\n",
		   (xconf==0)? "x" : "",
		   (yconf==0)? "y" : "",
		   (zconf==0)? "z" : "");
    }
    if (massconf == 0) {
	float particle_mass;
	SinglWarning("No \"mass\" in file, assigning from particle_mass in header\n");
	SDFgetfloatOrDie(sdfp, "particle_mass", &particle_mass);
	for (int i = 0; i < nobj; i++) {
	    btab[i].mass = particle_mass;
	}
    }
    SDFgetfloatOrDefault(sdfp, "R0",  &R0, 0.5f);
    SDFgetfloatOrDefault(sdfp, "Rz",  &R0, R0);
    SDFgetdoubleOrDefault(sdfp, "redshift",  &redshift, 0.0);
    SDFgetdoubleOrDefault(sdfp, "h_100",  &h_100, 1.0);
    SDFgetdoubleOrDefault(sdfp, "growthfac",  &growthfac, 1.0);
    SDFgetdoubleOrDefault(sdfp, "ic_growthfac",  &ic_growthfac, 1.0);

    SDFclose(csdfp);
    SDFclose(sdfp);

    EnableTimer(&StepTot, "Step Total");
    StartTimer(&StepTot);

    if (Nmesh % MPMY_Nproc()) {
      Error("Nmesh (%d) must be divisible by Nproc (%d)\n", 
	    Nmesh, MPMY_Nproc());
    }
    Nw = Nmesh;
    total_local_size = fftwf_mpi_local_size_3d(Nw, Nw, Nw, MPI_COMM_WORLD,
					      &local_nx, &local_x_start);
    singlPrintf("%.0f Mbytes per proc\n", 
		sizeof(fftwf_complex)*total_local_size/(1024.0*1024.0));
    d = Malloc(sizeof(fftwf_complex) * total_local_size);
    for (int i = 0; i < total_local_size; i++) {
      d[i] = 0.0 + 0.0I;
    }

    plan = fftwf_mpi_plan_dft_3d(Nw, Nw, Nw, d, d, MPI_COMM_WORLD, 
				FFTW_FORWARD, FFTW_ESTIMATE);

    VS(rmin, = -R0/(1.0+redshift));
    VS(rmax, = R0/(1.0+redshift));
    for (p = btab; p < btab+nobj; p++) {
	if (p->pos[0] > 2.0*rmax[0] || p->pos[0] < 2.0*rmin[0] ||
	    p->pos[1] > 2.0*rmax[1] || p->pos[1] < 2.0*rmin[1] ||
	    p->pos[2] > 2.0*rmax[2] || p->pos[2] < 2.0*rmin[2]) Error("Bad pos");
    }

    /* WrapPeriodic */
    for (p = btab; p < btab+nobj; p++) {
	VVVS(if LPAREN p->pos, > rmax, RPAREN p->pos, -= 2.0*R0/(1.0+redshift));
	VVVS(if LPAREN p->pos, < rmin, RPAREN p->pos, += 2.0*R0/(1.0+redshift));
    }

    VS(rmin, = -R0*(1.0+EPS)/(1.0+redshift));
    rsize = 2.0*R0*(1.0+EPS)/(1.0+redshift);
    SetMesh(Nmesh, rmin, rsize, fold_factor);
    M0 = 1.0;

    /* Interpolate sampling error correction data */
    if (interp_method == NGP) sampling_corr = sampling_ngp3;
    else if (interp_method == CIC) sampling_corr = sampling_cic3;
    else if (interp_method == TSC) sampling_corr = sampling_tsc3;
    else Error("Bad interp_method\n");
    spline(sampling_x, sampling_corr, SAMPLING_N, BIG, BIG, sampling_D2);


    DoNGP = (interp_method == NGP);		/* used in GetKeySlab */
    do_slab_decomp(&btab, &nobj, local_nx);

    M0 = 0.0;
    nsamples = 0;
    xmax = -1e30;
    xmin = 1e30;
    for (p = btab; p < btab+nobj; p++) {
      nsamples++;
      M0 += p->mass;
      if (p->pos[0] > xmax) xmax = p->pos[0];
      if (p->pos[0] < xmin) xmin = p->pos[0];
    }
    MPMY_Combine(&nsamples, &nsamples, 1, MPMY_INT64, MPMY_SUM);
    MPMY_Combine(&M0, &M0, 1, MPMY_DOUBLE, MPMY_SUM);

    singlPrintf("nsamples = %ld, total mass is %14.8g\n", nsamples, M0);

    if (interp_method == NGP) {
      for (p = btab; p < btab+nobj; p++) {
	ngp_density(p->pos, d, p->mass/M0);
      } 
    } else {
      int ii, jj, bsize;
      fftwf_complex *bnd;
      /* Temporary array to communicate slice to next processor */
      bsize = MaxMeshIndex * MaxMeshIndex;
      bnd = Malloc(sizeof(fftwf_complex)*bsize);
      for (ii = 0; ii < bsize; ii++) {
	bnd[ii] = 0.0 + 0.0I;
      }
      for (p = btab; p < btab+nobj; p++) {
	cic_density(p->pos, d, p->mass/M0, bnd);
      }
      exchange_boundary(bnd, MaxMeshIndex);
      for (ii = 0; ii < MaxMeshIndex; ii++) {
	for (jj = 0; jj < MaxMeshIndex; jj++) {
	  d[idx(SlabBegin,ii,jj)] += bnd[idx2(0, ii, jj)];
	}
      }
      Free(bnd);
    }
    Free(btab);

    if (write_rho) {
	float *rho = Malloc(total_local_size * sizeof(float));
	for (int i = 0; i < total_local_size; i += 2) {
	    rho[i/2] = creal(d[i]);
	}
	/* Split header from data */
	/* Perhaps viz pacakges will understand cube by itself */
	sprintf(name, "%s.%drho.hdr", argv[2], Nmesh);
	SDFwritehdr(name,
		    "struct {\n    float rho;\n}",
		    "Nmesh", SDF_INT, Nmesh,
		    NULL);
	sprintf(name, "%s.%drho", argv[2], Nmesh);
	SDFwrite64(name, (int64_t)Nmesh*Nmesh*Nmesh, 
		   (int64_t)total_local_size, rho, sizeof(float), 
		   "struct {\n    float rho;\n}",
		   "Nmesh", SDF_INT, Nmesh,
		   NULL);
	Free(rho);
    }

    StartTimer(&FFTTm);
    fftwf_execute(plan);
    StopTimer(&FFTTm);
    singlPrintf("FFT %.2f sec\n", ReadTimer(&FFTTm));

    fftwf_destroy_plan(plan);

    counts = Calloc(MAXCOUNTINDEX, sizeof(int));
    kbin = Calloc(MAXCOUNTINDEX, sizeof(double));
    sum = Calloc(MAXCOUNTINDEX, sizeof(double));
    linear_sum = Calloc(MAXCOUNTINDEX, sizeof(double));
    scorrect_sum = Calloc(MAXCOUNTINDEX, sizeof(double));
    cic_sum = Calloc(MAXCOUNTINDEX, sizeof(double));

    do_pspec(Nmesh, Nmesh, 1.0, fold_factor, Nmesh/2.0, d, counts, kbin, sum, linear_sum, 
	     scorrect_sum, cic_sum, Ktab, Tftab, D2, 0, 0.0, write_modes);

    if (MPMY_Procnum() == 0) {
	sprintf(name, "%s_ps%df%d", argv[2], Nmesh, fold_factor);
	Fopen(outfile, name, "w");

	fprintf(outfile, "# k k_mean p scorrect count\n");
	fprintf(outfile, "# infile=%s Nmesh=%d fold_factor=%d nsamples=%ld shot_noise=%.8g powspec_version=%s\n", 
		argv[2], Nmesh, fold_factor, nsamples, 1.0/nsamples, powspec_version);
	fprintf(outfile, "# L0Mpch=%g redshift=%g h_100=%g growthfac=%g ic_growthfac=%g\n", 
		R0*2.0*h_100/1000.0, redshift, h_100, growthfac, ic_growthfac);
	for (int i = 0; i < (Nmesh/2)*BIN_MULTIPLIER; i++) {
	    if (counts[i] >= 1) {
		fprintf(outfile, "%5d %14.08g %14.08g %14.08g %9d\n", 
			i * fold_factor, 
			exp(kbin[i]/counts[i]), 
			sum[i]/counts[i], 
			scorrect_sum[i]/counts[i],
			counts[i]/2);
	    }
	}
	Fclose(outfile);
    }

    StopTimer(&StepTot);
    OutputTimer(&StepTot, singlPrintf);
    singlPrintf("Done.\n");
    MPI_Finalize();
    exit(0);
}

Key_t GetKeySlab(const body *p)
{
    Key_t tmp;
    unsigned int i;

    if (DoNGP) {
      i = MeshFactor*(p->pos[0]-Rmin[0]);
    } else {
      i = 0.5+MeshFactor*(p->pos[0]-Rmin[0]);
    }
    while (i >= MaxMeshIndex) i -= MaxMeshIndex;
    tmp.k[0] = i;
#if NK==2
    tmp.k[1] = 0;
#endif
    /* mask_decomp_keys */
    return KeyLshift(tmp, 24);
}

float UnityCost(const void *ptr)
{
    return 1.0;
}
