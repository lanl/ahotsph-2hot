/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

/* "Buffered block" cube which covers a specified region with an added spatial buffer */
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
    MPMY_Init(&argc, &argv);

    if (argc != 2) {
	singlPrintf("usage: %s file.idx\n", argv[0]);
	exit(1);
    }
    char *idxfile = argv[1];

    /* memmap index file to idx */
    SDF *sdfidx = SDFopen(NULL, idxfile);
    if (!sdfidx) Error("SDFopen %s failed\n", idxfile);

    int level;
    if (SDFgetint(sdfidx, "level", &level)) Error("No level in index\n");

    int64_t offset = SDFfileoffset("base", sdfidx);
    int64_t stride = SDFfilestride("base", sdfidx);
    int64_t len = SDFnrecs("base", sdfidx);

    char infile[256];
    if (SDFgetstring(sdfidx, "filename", infile, sizeof(infile))) Error("No filename in index\n");

    SDFclose(sdfidx);

    assert(stride == sizeof(idx_t));

    int fd = open(idxfile, O_RDONLY);
    if (fd == -1) Error("open %s failed\n", idxfile);
    void *mm = mmap(NULL, offset+len*stride, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mm == MAP_FAILED) Error("mmap %s failed, %s\n", idxfile, strerror(errno));

    idx_t *idx = mm + offset;
    int idx_len = len;

    /* memmap data file to btab */
    SDF *sdf = SDFopen(NULL, infile);
    if (!sdf) Error("SDFopen %s failed\n", infile);

    int64_t gnobj;
    if (SDFgetint64(sdf, "npart", &gnobj)) Error("SDFget npart failed\n");

    float a, R[NDIM], rmin[NDIM], rmax[NDIM], particle_mass;
    SDFgetfloatOrDie(sdf, "Rx",  &R[0]);
    SDFgetfloatOrDie(sdf, "Ry",  &R[1]);
    SDFgetfloatOrDie(sdf, "Rz",  &R[2]);
    SDFgetfloatOrDie(sdf, "a",  &a);
    SDFgetfloatOrDie(sdf, "particle_mass",  &particle_mass);
    
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

    offset = SDFfileoffset("x", sdf);
    stride = SDFfilestride("x", sdf);
    len = SDFnrecs("x", sdf);

    SDFclose(sdf);

    assert(stride == sizeof(body));
    assert(len == gnobj);

    fd = open(infile, O_RDONLY);
    if (fd == -1) Error("open %s failed\n", infile);
    void *mm2 = mmap(NULL, offset+len*stride, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mm2 == MAP_FAILED) Error("mmap %s failed, %s\n", infile, strerror(errno));

    body *btab = mm2 + offset;

    FILE *fp = fopen("ds14_a_0.bblock", "w");
    if (!fp) Error("fopen failed, %s\n", strerror(errno));

    float corner000[NDIM], corner111[NDIM], size;
    Key_t placeholder = KeyLshift(KeyInt(1), level*NDIM);
    CellCorner(placeholder, corner000, &size);
    printf("level %d cell size is %g\n", level, size);
    float b = 8.01 * size;
    int outblocks = 0;
    for (int i = 0; i < idx_len; i++) {
	Key_t key = KeyOr(placeholder, KeyInt(idx[i].index));
	CellCorner(key, corner000, &size);
	VV(corner111, = size + corner000);
	if (corner000[0] <= b && corner000[1] <= b && corner000[2] <= b &&
	    corner111[0] >= -b && corner111[1] >= -b && corner111[2] >= -b) {
	    if (fwrite(&btab[idx[i].base], sizeof(body), idx[i].len, fp) != idx[i].len)
		Error("fwrite failed, %s\n", strerror(errno));
	    printf("cell %d\n", i);
	    if (++outblocks % 1024 == 0) fflush(stdout);
	}

    }
    fclose(fp);

    MPMY_Finalize();
    exit(0);
}

#if 0
msw@titan-ext7:~/scratch/ds14_a> SDFcvt ds14_a_1.0000.idx len base index | awk '{if ($1 > 300000) print}'
307250 37577382446 11470039
300627 206944166814 30106732
354834 208675923698 30159983
396337 304966701534 43354754
330006 952498058384 117836548
310010 1062089096996 126684840
/* Densest level 9 cell is 43354754 at offset 304966701534 */
msw@titan-ext7:~/scratch/ds14_a> SDFcvt -s 304966701534 -n 1 ds*1.0000 x y z
-5.520516e+06 1.345774e+06 -2.034626e+06
#endif
