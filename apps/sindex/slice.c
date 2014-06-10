#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
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
    MPMY_Init(&argc, &argv);

    if (argc != 4) {
	singlPrintf("usage: %s file.idx res nlayers\n", argv[0]);
	exit(1);
    }
    char *idxfile = argv[1];
    int64_t res = atoi(argv[2]);
    int nlayers = atoi(argv[3]);

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

    int sorted_rtp = 0;
    SDFgetint(sdf, "sorted_rtp", &sorted_rtp);
    int sorted_xyz = 0;
    SDFgetint(sdf, "sorted_xyz", &sorted_xyz);
    float particle_mass = 1.0f;
    SDFgetfloat(sdf, "particle_mass",  &particle_mass);

    float rmin[NDIM], rmax[NDIM];
    if (sorted_rtp) {
	float R0;
	SDFgetfloatOrDie(sdf, "R0",  &R0);
	float rtp_min[NDIM] = {0.0, 0.0, -M_PI};
	float rtp_max[NDIM] = {R0*1.01, 2.0*M_PI, M_PI};
	FixRsizeExact(rtp_min, rtp_max);
	VV(rmin, = rtp_min);
	VV(rmax, = rtp_max);
    } else if (sorted_xyz) {
	float R0;
	SDFgetfloatOrDie(sdf, "R0",  &R0);
	VS(rmin, = -R0*1.01);	/* must match lcjoin */
	VS(rmax, =  R0*1.01);
	FixRsizeExact(rmin, rmax);
    } else {
	float R[NDIM];
	float a = 1.0;
	SDFgetfloat(sdf, "a",  &a);
	SDFgetfloatOrDie(sdf, "Rx",  &R[0]);
	SDFgetfloatOrDie(sdf, "Ry",  &R[1]);
	SDFgetfloatOrDie(sdf, "Rz",  &R[2]);
	VV(rmin, = -a*R);
	VV(rmax, =  a*R);

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

    /* Make an image slice */
    float *image = calloc(res * res, sizeof(float));
    if (!image) Error("calloc failed\n");

    int nblocks = 0;
    float corner[NDIM], center[NDIM], size;
    Key_t placeholder = KeyLshift(KeyInt(1), level*NDIM);
    CellCorner(placeholder, corner, &size);
    printf("level %d cell size is %g\n", level, size);
#if 0
    float min[NDIM] = {0.0, 0.0, 0.0};
    float max[NDIM] = {4.0*size, 64.0*size, 64.0*size};
#else
    float min[NDIM] = {-(nlayers/2)*size, rmin[1], rmin[2]};
    float max[NDIM] = { (nlayers/2)*size, rmax[1], rmax[2]};
#endif
    printf("Expect %.0f**2 blocks per layer, %d layers\n", (max[2]-min[2])/size, nlayers);
    for (int i = 0; i < idx_len; i++) {
	Key_t key = KeyOr(placeholder, KeyInt(idx[i].index));
	CellCorner(key, corner, &size);
	VV(center, = 0.5f*size + corner);
	if (center[0] >= min[0] && center[0] < max[0] &&
	    center[1] >= min[1] && center[1] < max[1] &&
	    center[2] >= min[2] && center[2] < max[2]) {
	    if (++nblocks % 1000 == 0) {
		printf(".%d", nblocks/1000);
		fflush(stdout);
	    }
	    for (body *p = &btab[idx[i].base]; p < &btab[idx[i].base+idx[i].len]; p++) {
		int64_t iy = res*(p->pos[1]-min[1])/(max[1]-min[1]);
		int64_t iz = res*(p->pos[2]-min[2])/(max[2]-min[2]);
		if (iy >= 0 && iy < res && iz >= 0 && iz < res)
		    image[res*iz + iy] += particle_mass;
	    }
	}
    }
    char outname[256];
    sprintf(outname, "%s_slice0_%ld.float32", infile, res);
    FILE *fp = fopen(outname, "w");
    if (!fp) Error("fopen %s failed, %s\n", outname, strerror(errno));
    fwrite(image, sizeof(float), res * res, fp);
    fclose(fp);
    printf("\nDone.\n");

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
