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

/* 
msw@eos-ext1:~/scratch/chunk_halos> ~/2HOT/apps/sindex/bbox.i7 ds14_a_170000_1.0000 5 070000
[0, 581343.19],
[0, 581343.19],
[0, 581343.19],

msw@eos-ext1:~/scratch/chunk_halos> ~/2HOT/apps/sindex/bbox.i7 ds14_a_1077700_1.0000 6 077700
lseek on 3 returns 489881655, errno is 0
[-1162686.5, -872014.88],
[-1162686.5, -872014.88],
[-1162686.5, -872014.88],
*/

int
main(int argc, char *argv[])
{
    if (argc != 4) {
	singlPrintf("usage: %s infile.sdf level index\n", argv[0]);
	exit(1);
    }
    char *infile = argv[1];
    int level = atoi(argv[2]);
    int64_t index = strtoll(argv[3], NULL, 0);

    SDF *sdf = SDFopen(NULL, infile);
    if (!sdf) Error("SDFopen %s failed\n", infile);

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
    SDFclose(sdf);

    FixRsizeExact(rmin, rmax);

    float corner[NDIM], size;
    Key_t placeholder = KeyLshift(KeyInt(1), level*NDIM);
    Key_t key = KeyOr(placeholder, KeyInt(index));
    CellCorner(key, corner, &size);
    printf("[%.8g, %.8g],\n", corner[0], corner[0]+size);
    printf("[%.8g, %.8g],\n", corner[1], corner[1]+size);
    printf("[%.8g, %.8g],\n", corner[2], corner[2]+size);

    exit(0);
}
