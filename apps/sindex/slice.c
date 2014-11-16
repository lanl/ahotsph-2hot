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

int
main(int argc, char *argv[])
{
    MPMY_Init(&argc, &argv);

    if (argc != 5 && argc != 6) {
	singlPrintf("usage: %s file.idx res x_min x_max [min_mass]\n", argv[0]);
	exit(1);
    }
    char *idxfile = argv[1];
    int64_t res = atoi(argv[2]);
    float x_min = atoi(argv[3]);
    float x_max = atoi(argv[4]);
    float min_mass = 0.0;
    if (argc == 6) min_mass = atof(argv[5]);

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

    float particle_mass = 1.0f;
    SDFgetfloat(sdf, "particle_mass",  &particle_mass);

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
	float a = 1.0;
	SDFgetfloat(sdf, "a",  &a);
	SDFgetfloatOrDie(sdf, "Rx",  &R[0]);
	SDFgetfloatOrDie(sdf, "Ry",  &R[1]);
	SDFgetfloatOrDie(sdf, "Rz",  &R[2]);
	VV(rmin, = -a*R);
	VV(rmax, =  a*R);

#if 0
	int ic_Nmesh = 0;
	if (!SDFgetint(sdf, "ic_Nmesh", &ic_Nmesh)) {
	    /* expand root for non-power-of-two */
	    double expand_root = 0.0;
	    int f2 = 1<<(ilog2(ic_Nmesh-1)+1);
	    if (f2 != ic_Nmesh) expand_root = (double)f2/ic_Nmesh - 1.0;
	    VS(rmin, *= (1.0 + expand_root)); 
	    VS(rmax, *= (1.0 + expand_root));
	}
#endif
	FixRsizeExact(rmin, rmax);
    }

    offset = SDFfileoffset("x", sdf);
    stride = SDFfilestride("x", sdf);
    len = SDFnrecs("x", sdf);

    int64_t moffset = 0;
    /* moffset = SDFfileoffset("m200b", sdf); */

    SDFclose(sdf);

    assert(len == gnobj);

    fd = open(infile, O_RDONLY);
    if (fd == -1) Error("open %s failed\n", infile);
    void *mm2 = mmap(NULL, offset+len*stride, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mm2 == MAP_FAILED) Error("mmap %s failed, %s\n", infile, strerror(errno));

    /* Make an image slice */
    float *image = calloc(res * res, sizeof(float));
    if (!image) Error("calloc failed\n");

    int nblocks = 0;
    float corner[NDIM], center[NDIM], size;
    Key_t placeholder = KeyLshift(KeyInt(1), level*NDIM);
    CellCorner(placeholder, corner, &size);
    printf("level %d cell size is %g\n", level, size);
    float min[NDIM] = {x_min, rmin[1], rmin[2]};
    float max[NDIM] = {x_max, rmax[1], rmax[2]};
    float blocks_per_x = (x_max-x_min)/size;
    float blocks_per_dim = (max[2]-min[2])/size;
    printf("Expect %.0fk blocks\n", blocks_per_dim*blocks_per_dim*blocks_per_x/1024.0);
    for (int i = 0; i < idx_len; i++) {
	if (idx[i].len == 0) continue;
	Key_t key = KeyOr(placeholder, KeyInt(idx[i].index));
	CellCorner(key, corner, &size);
	VV(center, = 0.5f*size + corner);
	if (center[0] >= min[0] && center[0] < max[0] &&
	    center[1] >= min[1] && center[1] < max[1] &&
	    center[2] >= min[2] && center[2] < max[2]) {
	    if (++nblocks % 1024 == 0) {
		printf(".%d", nblocks/1000);
		fflush(stdout);
	    }
	    void *pos0 = mm2 + stride*idx[i].base + offset;
	    void *mass0 = mm2 + stride*idx[i].base + moffset;
	    assert(stride % sizeof(float) == 0);
	    for (int j = 0; j < idx[i].len; j++) {
		float *pos = pos0 + j * stride;
		float *mass = mass0 + j * stride;
		if (pos[0] < x_min || pos[0] >= x_max) continue;
		int64_t iy = res*(pos[1]-min[1])/(max[1]-min[1]);
		int64_t iz = res*(pos[2]-min[2])/(max[2]-min[2]);
		if (iy >= 0 && iy < res && iz >= 0 && iz < res) {
		    if (moffset && mass[0] > min_mass) {
			image[res*iz + iy] += mass[0];
		    } else {
			image[res*iz + iy] += particle_mass;
		    }
		}
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
