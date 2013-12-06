#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "SDF.h"
#include "SDFread.h"
#include "Malloc.h"
#include "error.h"
#include "singlio.h"
#include "mpmy.h"
#include "timers.h"
#include "randoms.h"
#include "SDFwrite.h"
#define NDIM 3
#include "vop.h"


typedef struct  {
    float mass;			/* mass of body */
    float pos[NDIM];		/* position of body */
    float vel[NDIM];		/* velocity of body */
    int64_t ident;		/* unique identifier */
} __attribute__ ((packed)) body;

#define OUTBODYDESC \
"struct {\n\
    float mass;			/* mass of body */\n\
    float x, y, z;		/* position of body */\n\
    float vx, vy, vz;		/* velocity of body */\n\
    int64_t ident;		/* unique identifier */\n\
}"


SDF *
ReadData(char *name, body **btab, int64_t *gnobj, int *nobj)
{
    int massconf, xconf, yconf, zconf;
    int vxconf, vyconf, vzconf;
    int identconf;
    SDF *sdfp;

    sdfp = SDFreadf64(NULL, name, (void *)btab, gnobj, nobj, sizeof(body),
		     "mass", offsetof(body, mass), &massconf,
		     "x", offsetof(body, pos[0]), &xconf,
		     "y", offsetof(body, pos[1]), &yconf,
		     "z", offsetof(body, pos[2]), &zconf,
		     "vx", offsetof(body, vel[0]), &vxconf,
		     "vy", offsetof(body, vel[1]), &vyconf,
		     "vz", offsetof(body, vel[2]), &vzconf,
		     "ident", offsetof(body, ident), &identconf,
		     NULL);
    if (massconf==0 || xconf==0 || yconf==0 || zconf==0) {
	SinglError("Could not find %s %s %s %s in data file!\n",
		   (massconf==0)? "mass" : "",
		   (xconf==0)? "x" : "",
		   (yconf==0)? "y" : "",
		   (zconf==0)? "z" : "");
    }
    if (vxconf != vyconf || vxconf != vzconf) {
	SinglError("Missing velocity components!\n");
    }
    if (identconf == 0) {
	SinglError("No \"ident\" in file.\n");
    }
    singlPrintf("Data read, gnobj=%ld\n", *gnobj);
    
    return sdfp;
}

#define SDFHDR_INT(a) a, SDF_INT, *(int *)SDFptr(a, sdfp)
#define SDFHDR_INT64(a) a, SDF_INT64, *(int64_t *)SDFptr(a, sdfp)
#define SDFHDR_FLOAT(a) a, SDF_FLOAT, *(float *)SDFptr(a, sdfp)
#define SDFHDR_DOUBLE(a) a, SDF_DOUBLE, *(double *)SDFptr(a, sdfp)
#define SDFHDR_STRING(a) a, SDF_STRING, (char *)SDFptr(a, sdfp)

void *
SDFptr(char *name, SDF *sdfp)
{
    int n = SDFarrcnt(name, sdfp); /* number of elements */
    int sz = SDFtype_sizes[SDFtype(name, sdfp)]; /* size of each element */
    if (n*sz >= 1024) Error("%s too large for SDFptr\n", name);
    char *buf = Malloc(n*sz);
    if (SDFseekrdvecs(sdfp, name, 0, 1, buf, 0, NULL)) {
	Error("SDF read %s failed\n", name);
    }
    return buf;
}

int
main(int argc, char **argv)
{
    MPMY_Init(&argc, &argv);
    if (argc != 7) {
	singlPrintf("usage: %s cut_sphere x y z r infile.sdf outfile.sdf\n", argv[0]);
	exit(1);
    }
    singlPrintf("Welcome to the machine\n");

    float center[NDIM] = { atof(argv[1]), atof(argv[2]), atof(argv[3]) };
    float rmax = atof(argv[4]);
    int nobj;
    int64_t gnobj;
    body *btab;
    SDF *sdfp = ReadData(argv[5], &btab, &gnobj, &nobj);
    float R0, h_100;
    SDFgetfloatOrDie(sdfp, "R0",  &R0);
    SDFgetfloatOrDie(sdfp, "h_100",  &h_100);
    float L0 = 2.0f*R0;

    /* input in Mpc/h */
    rmax *= 1000.0/h_100;
    VS(center, *= 1000.0/h_100);
    VS(center, -= R0);

    int nout = 0;
    for (int k = 0; k < nobj; k++) {
	float dr[NDIM];
	VVV(dr, = btab[k].pos, - center);
	VVS(if LPAREN dr, > R0 RPAREN dr, -= L0);
	VVS(if LPAREN dr, < -R0 RPAREN dr, += L0);
	if (Dot(dr, dr) <= rmax*rmax) {
	    btab[nout++] = btab[k];
	}
    }
    nobj = nout;
    btab = Realloc(btab, nout*sizeof(body));
    int64_t gnobj0 = gnobj;
    gnobj = nobj;
    MPMY_Combine(&gnobj, &gnobj, 1, MPMY_INT64, MPMY_SUM);
    singlPrintf("sample reduced to %ld particles (%0.4lf)\n", gnobj, (double)gnobj/gnobj0);

    SDFwrite64(argv[6], gnobj, 
	       nobj, btab, sizeof(body), OUTBODYDESC,
	       "npart", SDF_INT64, gnobj,
	       "npart_orig", SDF_INT64, gnobj0,
	       "center_x", SDF_FLOAT, center[0],
	       "center_y", SDF_FLOAT, center[1],
	       "center_z", SDF_FLOAT, center[2],
	       "rmax", SDF_FLOAT, rmax,
	       "do_periodic", SDF_INT, 0,
	       SDFHDR_INT("version"),
	       SDFHDR_INT("iter"),
	       SDFHDR_DOUBLE("tpos"),
	       SDFHDR_DOUBLE("tvel"),
	       SDFHDR_DOUBLE("tacc"),
	       SDFHDR_FLOAT("R0"),
	       SDFHDR_DOUBLE("redshift"),
	       SDFHDR_DOUBLE("a"),
	       SDFHDR_DOUBLE("a_tvel"),
	       SDFHDR_DOUBLE("a_tacc"),
	       SDFHDR_DOUBLE("Omega0"),
	       SDFHDR_DOUBLE("Omega0_m"),
	       SDFHDR_DOUBLE("Omega0_r"),
	       SDFHDR_DOUBLE("Omega0_lambda"),
	       SDFHDR_DOUBLE("Omega0_cdm"),
	       SDFHDR_DOUBLE("Omega0_ncdm_tot"),
	       SDFHDR_DOUBLE("Omega0_b"),
	       SDFHDR_DOUBLE("Omega0_g"),
	       SDFHDR_DOUBLE("Omega0_ur"),
	       SDFHDR_DOUBLE("Omega0_fld"),
	       SDFHDR_DOUBLE("w0_fld"),
	       SDFHDR_DOUBLE("wa_fld"),
	       SDFHDR_DOUBLE("h_100"),
	       SDFHDR_DOUBLE("H0"),
	       SDFHDR_DOUBLE("hubble"),
	       SDFHDR_DOUBLE("H"),
	       SDFHDR_DOUBLE("Gnewt"),
	       SDFHDR_DOUBLE("growthfac"),
	       SDFHDR_DOUBLE("growthfac_tvel"),
	       SDFHDR_DOUBLE("growthfac_tacc"),
	       SDFHDR_DOUBLE("velfac"),
	       SDFHDR_FLOAT("tolerance"),
	       SDFHDR_FLOAT("frac_tolerance"),
	       SDFHDR_FLOAT("frac_tolerance0"),
	       SDFHDR_FLOAT("Rx"),
	       SDFHDR_FLOAT("Ry"),
	       SDFHDR_FLOAT("Rz"),
	       SDFHDR_INT("Nx"),
	       SDFHDR_INT("Ny"),
	       SDFHDR_INT("Nz"),
	       SDFHDR_FLOAT("epsilon_mscale"),
	       SDFHDR_FLOAT("epsilon_scaled"),
	       SDFHDR_FLOAT("epsilon0"),
	       SDFHDR_INT("force_smoothing_type"),
	       SDFHDR_INT("ic_Nmesh"),
	       SDFHDR_DOUBLE("ic_growthfac"),
	       SDFHDR_INT("checkpoint"),
	       SDFHDR_DOUBLE("ke"),
	       SDFHDR_DOUBLE("pe"),
	       SDFHDR_STRING("compiled_version"),
	       SDFHDR_STRING("compiled_date"),
	       SDFHDR_STRING("compiled_time"),
	       NULL);
    SDFclose(sdfp);

    Free(btab);
    exit(0);
}

