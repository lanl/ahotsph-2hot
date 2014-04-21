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

    /* Make an image slice */
    int res = 1024;
    float *image = calloc(res * res, sizeof(float));
    if (!image) Error("calloc failed\n");

    int nblocks = 0;
    float corner[NDIM], size;
    Key_t placeholder = KeyLshift(KeyInt(1), level*NDIM);
    CellCorner(placeholder, corner, &size);
    printf("level %d cell size is %g\n", level, size);
    for (int i = 0; i < idx_len; i++) {
	Key_t key = KeyOr(placeholder, KeyInt(idx[i].index));
	CellCorner(key, corner, &size);
	if (corner[0] >= 0.0 && corner[0] < size && /* slice at x == 0+ */
	    corner[1] >= 0.0 && corner[1] < 32.0 * size &&
	    corner[2] >= 0.0 && corner[2] < 32.0 * size) {
	    if (++nblocks % 1000 == 0) {
		printf(".");
		fflush(stdout);
	    }
	    for (body *p = &btab[idx[i].base]; p < &btab[idx[i].base+idx[i].len]; p++) {
		int iy = 32*(p->pos[1]-0.0)/(32.0*size);
		int iz = 32*(p->pos[2]-0.0)/(32.0*size);
		if (iy >= 0 && iy < res && iz >= 0 && iz < res)
		    image[res*iz + iy] += particle_mass;
	    }
	}
    }
    FILE *fp = fopen("slice.float32", "w");
    if (!fp) Error("fopen failed, %s\n", strerror(errno));
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
