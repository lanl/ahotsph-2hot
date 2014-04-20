#include <stdio.h>
#include <stdlib.h>
#include "Malloc.h"
#include "SDF.h"
#include "SDFread.h"
#define NDIM 3
#include "vop.h"
#include "mpmy.h"
#include "singlio.h"
#include "chealpix.h"

typedef struct {
    float mass;			/* mass of body */
    float pos[NDIM];		/* position of body */
} body;


int
main(int argc, char *argv[])
{
    MPMY_Init(&argc, &argv);

    if (argc != 6) {
	singlPrintf("usage: %s nside rin rout infile.sdf outfile.fits\n", argv[0]);
	exit(1);
    }

    long nside = atoi(argv[1]);
    float r0 = atof(argv[2]);
    float r1 = atof(argv[3]);
    long npix = nside2npix(nside);

    singlPrintf("nside %ld npix %ld\n", nside, npix);

    double *count = Calloc(npix, sizeof(double));

    SDF *sdfp;
    body *btab;
    int64_t gnobj;
    int nobj;
    int massconf, xconf, yconf, zconf;
    singlPrintf("Reading \"%s\"\n", argv[4]);
    sdfp = SDFreadf64(NULL, argv[4], (void **)&btab, &gnobj, &nobj, sizeof(body),
		      "mass", offsetof(body, mass), &massconf,
		      "x", offsetof(body, pos[0]), &xconf,
		      "y", offsetof(body, pos[1]), &yconf,
		      "z", offsetof(body, pos[2]), &zconf,
		      NULL);
    if (xconf==0 || yconf==0 || zconf==0)
	SinglError("Could not find %s %s %s in data file!\n",
		   (xconf==0)? "x" : "",
		   (yconf==0)? "y" : "",
		   (zconf==0)? "z" : "");

    if (massconf == 0) {
	float particle_mass;
	SDFgetfloatOrDie(sdfp, "particle_mass", &particle_mass);
	for (int i = 0; i < nobj; i++) {
	    btab[i].mass = particle_mass;
	}
    }

    float redshift;

    if (r1 == 0.0) {
	SDFgetfloatOrDefault(sdfp, "R0", &r1, 4000.0);
	SDFgetfloatOrDefault(sdfp, "Rz", &r1, r0);
	SDFgetfloatOrDie(sdfp, "redshift",  &redshift);
	r1 /= (1.0+redshift);
	if (r0 < 1.0) r0 = r0*r1;
    }

    SDFclose(sdfp);

    for (body *p = btab; p < btab + nobj; p++) {
	double vec[NDIM];
	double theta, phi;
	long ipring;
	
	float r2 = Dot(p->pos, p->pos);
	if (r2 < r1*r1 && r2 >= r0*r0) {
	    VV(vec, = p->pos);
	    vec2ang(vec, &theta, &phi);
	    ang2pix_ring(nside, theta, phi, &ipring);
	    count[ipring] += 1.0;
	}
    }
    Free(btab);

    MPMY_Combine(count, count, npix, MPMY_DOUBLE, MPMY_SUM);

    if (MPMY_Procnum() == 0) {
	float *signal = Malloc(npix*sizeof(float));
	double sum = 0.0;
	double n = 0.0;		
	for (int i = 0; i < npix; i++) {
	    sum += count[i];
	    if (count[i] > 0.0) n += 1.0;
	}
	printf("sum is %g\n", sum);
	double mean = sum/n;	/* mean of nonzero pixels */
	for (int i = 0; i < npix; i++) signal[i] = count[i];
	Free(count);
	write_healpix_map(signal, nside, argv[5], 0, "G");
	Free(signal);
    } else {
	Free(count);
    }
    
    MPMY_Finalize();
    exit(0);
}
