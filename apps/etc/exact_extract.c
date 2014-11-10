#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#define NDIM 3
#include "vop.h"
#include "mpmy.h"
#include "SDFread.h"
#include "image.h"
#include "singlio.h"
#include "error.h"
#include "Malloc.h"
#include "timers.h"
#include "Msgs.h"
#include "macr.h"
#include "stk.h"

#define Max(a,b) \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

typedef struct {
    float acc[NDIM];		/* acceleration of body */
    int64_t ident;
} __attribute__ ((packed)) body;

typedef struct {
    float diff[NDIM];
    int64_t ident;
} __attribute__ ((packed)) adiff;


int
main(int argc, char **argv)
{
    SDF *sdfp0, *sdfp1;
    int64_t gnobj0, gnobj1;
    int nobj0, nobj1;
    double a0, a1;
    double R0;
    body *btab0, *btab1;
    int axconf, ayconf, azconf, identconf;

    MPMY_Init(&argc, &argv);
    if (argc < 3) {
	singlPrintf("usage: %s file0.sdf file1.sdf [file2.sdf file3.sdf ...]\n", argv[0]);
	exit(1);
    }
    singlPrintf("Differencing %d files\n", argc-2);
    singlPrintf("Reading \"%s\"\n", argv[1]);
    sdfp0 = SDFreadf64(NULL, argv[1], (void **)&btab0, &gnobj0, &nobj0, sizeof(body),
		       "ax", offsetof(body, acc[0]), &axconf,
		       "ay", offsetof(body, acc[1]), &ayconf,
		       "az", offsetof(body, acc[2]), &azconf,
		       "ident", offsetof(body, ident), &identconf,
		       NULL);
    if( axconf==0 || ayconf==0 || azconf==0){
	SinglWarning("Could not find %s %s %s %s in data file!\n",
		     (axconf==0)? "ax" : "",
		     (ayconf==0)? "ay" : "",
		     (azconf==0)? "az" : "",
		     (identconf==0)? "ident" : "");
    }
    SDFgetdouble(sdfp0, "R0", &R0);
    SDFgetdouble(sdfp0, "a", &a0);
    SDFclose(sdfp0);

    for (int ii = 2; ii < argc; ii++) {
	singlPrintf("Reading \"%s\"\n", argv[ii]);
	sdfp1 = SDFreadf64(NULL, argv[ii], (void **)&btab1, &gnobj1, &nobj1, sizeof(body),
			   "ax", offsetof(body, acc[0]), &axconf,
			   "ay", offsetof(body, acc[1]), &ayconf,
			   "az", offsetof(body, acc[2]), &azconf,
			   "ident", offsetof(body, ident), &identconf,
			   NULL);
	if( axconf==0 || ayconf==0 || azconf==0){
	    SinglWarning("Could not find %s %s %s %s in data file!\n",
			 (axconf==0)? "ax" : "",
			 (ayconf==0)? "ay" : "",
			 (azconf==0)? "az" : "",
			 (identconf==0)? "ident" : "");
	    continue;
	}
	SDFgetdouble(sdfp1, "a", &a1);

	SDFclose(sdfp1);

	if (gnobj0 != gnobj1) {
	    SinglWarning("gnobj differs in %s\n", argv[ii]);
	    Free(btab1);
	    continue;
	}
	if (nobj0 != nobj1) {
	    SinglWarning("nobj differs in %s\n", argv[ii]);
	    Free(btab1);
	    continue;
	}
	if (fabs(1.0-a0/a1) > 1e-7) {
	    SinglWarning("a differs in %s\n", argv[ii]);
	    Free(btab1);
	    continue;
	}
	
	float diff[NDIM];
	VS(diff, = 0.0);
	Stk outstk;
	StkInitEz(&outstk);
	for (int i = 0; i < nobj0; i++) {
	    /* exact accs are usually a subsample */
	    if (Dot(btab0[i].acc, btab0[i].acc) != 0.0 && btab0[i].ident < 1000000) {
		if (btab0[i].ident != btab1[i].ident) Error("idents don't match\n");
		VVV(diff, = btab1[i].acc, - btab0[i].acc);
		StkPushData(&outstk, diff, NDIM*sizeof(float));
		StkPushData(&outstk, &btab0[i].ident, sizeof(int64_t));
	    }
	}
	Free(btab1);

	adiff *atab;
	unsigned int nin;
	nin = MPMY_NGather(StkBase(&outstk), StkSz(&outstk), MPMY_CHAR, 
			   (void **)&atab, 0);
	nin /= sizeof(*atab);

	if (MPMY_Procnum() == 0) {
	    char outname[256];
	    FILE *fp;
	    snprintf(outname, sizeof(outname), "%s.adiff", argv[ii]);
	    Fopen(fp, outname, "w");
	    fprintf(fp, "# difference %s %s\n", argv[1], argv[ii]);
	    for (int i = 0; i < nin; i++) {
		fprintf(fp, "%10ld %10g %10g %10g\n", 
			atab[i].ident,
			atab[i].diff[0], atab[i].diff[1], atab[i].diff[2]);
	    }
	    Free(atab);
	    Fclose(fp);
	}
	StkTerminate(&outstk);
    }
    singlPrintf("Done.\n");
    exit(0);
}


