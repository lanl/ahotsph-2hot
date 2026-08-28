/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

/* Extract cell with no overlap region */
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
#include "version_2HOT.h"

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
    singlPrintf("compiled %s %s\n", __DATE__, __TIME__);
    singlPrintf("library %s %s %s\n", version_2HOT, compiled_date_2HOT, compiled_time_2HOT);

    if (argc != 4) {
	singlPrintf("usage: %s file.idx cell_level index\n", argv[0]);
	exit(1);
    }
    char *idxfile = argv[1];
    int cell_level = atoi(argv[2]);
    int64_t index = strtoll(argv[3], NULL, 0);

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
    if (SDFgetint64(sdf, "npart", &gnobj)) {
	gnobj = SDFnrecs("x", sdf);
    }

    float particle_mass;
    SDFgetfloatOrDie(sdf, "particle_mass",  &particle_mass);
    int morton_xyz = 0;
    SDFgetint(sdf, "morton_xyz", &morton_xyz);
    
    float rmin[NDIM], rmax[NDIM];
    if (SDFhasname("x_min", sdf)) {
	SDFgetfloatOrDie(sdf, "x_min", &rmin[0]);
	SDFgetfloatOrDie(sdf, "y_min", &rmin[1]);
	SDFgetfloatOrDie(sdf, "z_min", &rmin[2]);
	SDFgetfloatOrDie(sdf, "x_max", &rmax[0]);
	SDFgetfloatOrDie(sdf, "y_max", &rmax[1]);
	SDFgetfloatOrDie(sdf, "z_max", &rmax[2]);
	FixRsizeExact(rmin, rmax);
    } else {
	float R[NDIM];
	double a = 1.0;
	SDFgetdouble(sdf, "a",  &a);
	SDFgetfloatOrDie(sdf, "Rx",  &R[0]);
	SDFgetfloatOrDie(sdf, "Ry",  &R[1]);
	SDFgetfloatOrDie(sdf, "Rz",  &R[2]);
	VV(rmin, = -a*R);
	VV(rmax, =  a*R);

	int ic_Nmesh = 0;
	if (!morton_xyz && !SDFgetint(sdf, "ic_Nmesh", &ic_Nmesh)) {
	    /* expand root for non-power-of-two */
	    singlPrintf("Expanding root cell\n");
	    double expand_root = 0.0;
	    int f2 = 1<<(ilog2(ic_Nmesh-1)+1);
	    if (f2 != ic_Nmesh) expand_root = (double)f2/ic_Nmesh - 1.0;
	    VS(rmin, *= (1.0 + expand_root)); 
	    VS(rmax, *= (1.0 + expand_root));
	}
	FixRsizeExact(rmin, rmax);
    }

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

    if (MPMY_Nproc() > 1) index |= MPMY_Procnum();

    char outfile[256];
    sprintf(outfile, "ds14_b_%llo_cell", 1LL<<(NDIM*cell_level) | index);
    FILE *fp = fopen(outfile, "w");
    if (!fp) Error("fopen failed, %s\n", strerror(errno));

    int64_t nout = 0;
    int64_t first = -1;
    for (int i = 0; i < idx_len; i++) {
	if (idx[i].index >> NDIM*(level-cell_level) == index) {
	    if (fwrite(&btab[idx[i].base], sizeof(body), idx[i].len, fp) != idx[i].len)
		Error("fwrite failed, %s\n", strerror(errno));
	    if (first == -1) first = idx[i].base;
	    nout += idx[i].len;
	}
    }
    fprintf(stderr, "slice(%ld, %ld)\n", first, first+nout);
    fclose(fp);
    exit(0);
}
