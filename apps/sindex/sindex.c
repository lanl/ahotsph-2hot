#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "assert.h"
#include "body.h"
#include "vop.h"
#include "SDF.h"
#include "mpmy.h"
#include "singlio.h"
#include "error.h"
#include "gc.h"			/* for ilog2 */
#include "stk.h"
#include "SDFwrite.h"

/* This is only valid at levels in the tree with less than 4G particles in each cell */
/* and less than 4G cells at that level */
typedef struct {
    int64_t base;
    uint32_t len;
    uint32_t index;
} idx_t;

#define OUTIDX \
"struct {\n\
    int64_t base;		/* offset of first object in cell */\n\
    unsigned int len;		/* number of objects in this cell */\n\
    unsigned int index;		/* cell morton index */\n\
}"

int
main(int argc, char *argv[])
{
    int text_output = 0;
    int level = 3;

    MPMY_Init(&argc, &argv);

    if (argc != 2 && argc != 3) {
	singlPrintf("usage: %s infile [level]\n", argv[0]);
	exit(1);
    }
    char *infile = argv[1];
    if (argc == 3) level = atoi(argv[2]);
    if (level > 10) Error("level too large for int32 index\n");

    SDF *sdf = SDFopen(NULL, infile);
    if (!sdf) Error("SDFopen %s failed\n", infile);

    int64_t gnobj;
    if (SDFgetint64(sdf, "npart", &gnobj)) {
	gnobj = SDFnrecs("x", sdf);
    }

    float a, R[NDIM], rmin[NDIM], rmax[NDIM];
    SDFgetfloatOrDie(sdf, "Rx",  &R[0]);
    SDFgetfloatOrDie(sdf, "Ry",  &R[1]);
    SDFgetfloatOrDie(sdf, "Rz",  &R[2]);
    SDFgetfloatOrDie(sdf, "a",  &a);
    
    VV(rmin, = -a*R);
    VV(rmax, = a*R);

    int ic_Nmesh = 0;
    if (!SDFgetint(sdf, "ic_Nmesh", &ic_Nmesh)) {
	/* expand root for non-power-of-two */
	double expand_root = 0.0;
	int f2 = 1<<(ilog2(ic_Nmesh-1)+1);
	if (f2 != ic_Nmesh) expand_root = (double)f2/ic_Nmesh - 1.0;
	VS(rmin, *= (1.0 + expand_root)); 
	VS(rmax, *= (1.0 + expand_root));
    }

    FixRsizeExact(rmin, rmax);

    int64_t offset = SDFfileoffset("x", sdf);
    int64_t stride = SDFfilestride("x", sdf);
    int64_t len = SDFnrecs("x", sdf);

    SDFclose(sdf);

    assert(stride == sizeof(body));
    assert(len == gnobj);

    int fd = open(infile, O_RDONLY);
    if (fd == -1) Error("open %s failed\n", infile);
    void *mm = mmap(NULL, offset+len*stride, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mm == MAP_FAILED) Error("mmap %s failed, %s\n", infile, strerror(errno));

    /* If we padded the header to be a multiple of the page size,*/
    /*  we could mmap btab directly */

    body *btab = mm + offset;

    int64_t mask = (1LL<<(level*NDIM))-1;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();
    int64_t start = procnum * gnobj / nproc;
    int64_t end = (procnum == nproc-1) ? gnobj-1 : (procnum+1) * gnobj / nproc;
    int is_partial = (procnum) ? 1 : 0;

    FILE *fp = stdout;
    if (nproc > 1 && text_output) {
	char filename[256];
	sprintf(filename, "index/%s_index.%04d", argv[1], procnum);
	fp = fopen(filename, "w");
	if (!fp) Error("fopen %s failed, %s\n", filename, strerror(errno));
    }
    Stk stk;
    StkInitEz(&stk);

    if (text_output) {
	fprintf(fp, "# base len index octal_key\n");
	fprintf(fp, "# level=%d\n", level);
    }

    int nlines = 0;
    /* Termination condition really is <=, since next proc didn't know it started at beginning */
    for (int64_t i = start; i <= end; /* NULL */) {
	int64_t i0 = i;
	Key_t this_cell = KeyRshift(GetKeyFast(&btab[i0]), NDIM*(BITS_PER_DIM-level));
	while (1) {
	    while (KeyEQ(this_cell, KeyRshift(GetKeyFast(&btab[i]), NDIM*(BITS_PER_DIM-level)))) i++;
	    /* keys are sorted in file by positions on previous timestep.  Sigh. */
	    /* If at least 8 of next 10 are not in this cell, then it's really a new cell */
	    int count = 0;
	    Key_t next_cell = KeyRshift(GetKeyFast(&btab[i]), NDIM*(BITS_PER_DIM-level));
	    for (int64_t j = i+1; j < i+11; j++) {
		if (KeyEQ(next_cell, KeyRshift(GetKeyFast(&btab[j]), NDIM*(BITS_PER_DIM-level)))) count++;
	    }
	    if (count >= 8) {
		/* Skip first one (started in the middle) the proc before us will do it */
		if (!is_partial) {
		    idx_t idx = {.base = i0, .len = i-i0, .index = this_cell.k[0] & mask};
		    if (i-i0 >= (1LL<<32)) Error("cell len too large for int32\n");
		    StkPushData(&stk, &idx, sizeof(idx_t));
		    if (text_output) {
			fprintf(fp, "%12ld %6ld %8ld %s\n", i0, i-i0, this_cell.k[0] & mask, PrintKey(GetKeyFast(&btab[i0])));
			if (++nlines % 1000) fflush(fp);
		    }
		} else is_partial = 0;
		break;
	    } else {
		i++;
	    }
	}
    }
    int64_t gnout, nout;
    nout = StkSz(&stk)/sizeof(idx_t);
    MPMY_Combine(&nout, &gnout, 1, MPMY_INT64, MPMY_SUM);

    char outname[256];
    sprintf(outname, "%s.idx", argv[1]);

    SDFwrite64(outname, gnout, 
	       nout, StkBase(&stk), sizeof(idx_t), OUTIDX,
	       "filename", SDF_STRING, argv[1],
	       "level", SDF_INT, level,
	       "x_min", SDF_DOUBLE, rmin[0],
	       "y_min", SDF_DOUBLE, rmin[1],
	       "z_min", SDF_DOUBLE, rmin[2],
	       "rsize", SDF_DOUBLE, rmax[2]-rmin[2],
	       "version", SDF_INT, 1);

    MPMY_Finalize();
    exit(0);
}
