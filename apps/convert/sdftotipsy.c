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

#define FORMAT_MAJOR 3
#define FORMAT_MINOR 0

#define GNEWT 4.49865897e4 /* units of kpc^3 / 10^10Msolar-Gyr^2 */
#define one_kpc (3.08567802e16) /* km */
#define one_Gyr (3.1558149984e16) /* sec */
#ifndef M_PI
#define	M_PI	3.14159265358979323846
#endif

typedef struct {
    float mass;			/* mass of body */
    float pos[NDIM];		/* position of body */
    float vel[NDIM];		/* velocity of body */
    int64_t ident;		/* unique identifier */
} __attribute__ ((packed)) body;

struct dump {
    double time;
    int nbodies;
    int ndim;
    int nsph;
    int ndark;
    int nstar;
    int pad;
};

struct tipsy {
    float mass;
    float pos[NDIM];
    float vel[NDIM];
    float eps;
    float phi;
};

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
    float tpos;
    double redshift, R0, H0, Omega0, Lambda_prime, hubble;
    double a;
    FILE *fp;
    struct dump h;
 
    MPMY_Init(&argc, &argv);
    if (argc != 2) {
	singlPrintf("usage: %s file.sdf\n", argv[0]);
	exit(1);
    }
    singlPrintf("Welcome to the machine\n");

    if ((sdfp = SDFopen(NULL, argv[1])) == NULL) {
 	SinglError("Sorry, couldn't SDFopen %s\n%s\n",
		   argv[1], SDFerrstring);
    }

    SDFgetintOrDefault(sdfp, "iter", &iter, 0);
    SDFgetfloatOrDefault(sdfp, "epsilon_scaled", &eps, 0.0);
    SDFgetfloatOrDefault(sdfp, "tpos", &tpos, 0.0);
    SDFgetdoubleOrDefault(sdfp, "redshift",  &redshift, 0.0);
    SDFgetdoubleOrDefault(sdfp, "R0",  &R0, 0.0);
    SDFgetdoubleOrDefault(sdfp, "Omega0",  &Omega0, 1.0);
    SDFgetdoubleOrDefault(sdfp, "H0",  &H0, 0.0511365);
    SDFgetdoubleOrDefault(sdfp, "hubble",  &hubble, 0.0511365);
    SDFgetdoubleOrDefault(sdfp, "Lambda_prime",  &Lambda_prime, 0.0);

    sdfp = SDFreadf64(NULL, argv[1], (void **)&btab, &gnobj, &nobj, sizeof(body),
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
      SinglError("Could not find %s %s %s %s in data file!\n",
		 (massconf==0)? "mass" : "",
		 (xconf==0)? "x" : "",
		 (yconf==0)? "y" : "",
		 (zconf==0)? "z" : "");
    }
    if (vxconf != vyconf || vxconf != vzconf) {
      SinglError("Missing velocity components!\n");
    }
    if (massconf == 0) {
	float particle_mass;
	SDFgetfloatOrDie(sdfp, "particle_mass", &particle_mass);
	for (i = 0; i < nobj; i++) {
	    btab[i].mass = particle_mass;
	}
    }

    a = 1.0/(1.0+redshift);

    sprintf(outname, "%s.tipsy", argv[1]);

    if (fopen(outname, "r")) {
      SinglError("%s exists!  Will not overwrite.\n", outname);
    }
    Fopen(fp, outname, "w");

    h.time = tpos;
    h.nbodies = gnobj;
    h.ndim = NDIM;
    h.nsph = 0;
    h.ndark = gnobj;
    h.nstar = 0;

    Fwrite(&h, sizeof(struct dump), 1, fp);
    
    for (i = 0; i < nobj; i++) {
      struct tipsy p;

      p.mass = btab[i].mass;
      VV(p.pos, = btab[i].pos);
      VV(p.vel, = btab[i].vel);
      p.eps = eps;
      p.phi = 0.0;
      
      Fwrite(&p, sizeof(p), 1, fp);
    }
    
    Fclose(fp);
    singlPrintf("\nOutput to %s done.\n", outname);
    
    exit(0);
}
