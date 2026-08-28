/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

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

    int sorted_rtp = 0;
    SDFgetint(sdf, "sorted_rtp", &sorted_rtp);

    if (sorted_rtp) {
	float R0;
	SDFgetfloatOrDie(sdf, "R0",  &R0);
	float rtp_min[NDIM] = {0.0, 0.0, -M_PI};
	float rtp_max[NDIM] = {R0*1.01, 2.0*M_PI, M_PI};
	FixRsizeExact(rtp_min, rtp_max);
    } else {
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
    }
    SDFclose(sdf);

    float corner[NDIM], size[NDIM];
    Key_t placeholder = KeyLshift(KeyInt(1), level*NDIM);
    Key_t key = KeyOr(placeholder, KeyInt(index));
    CellCorner(key, corner, size);
    printf("[%.8g, %.8g],\n", corner[0], corner[0]+size[0]);
    printf("[%.8g, %.8g],\n", corner[1], corner[1]+size[1]);
    printf("[%.8g, %.8g],\n", corner[2], corner[2]+size[2]);

    exit(0);
}
