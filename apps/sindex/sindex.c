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

int
main(int argc, char *argv[])
{
    int level = 3;

    MPMY_Init(&argc, &argv);

    if (argc != 2 && argc != 3) {
	singlPrintf("usage: %s infile [level]\n", argv[0]);
	exit(1);
    }
    char *infile = argv[1];
    if (argc == 3) level = atoi(argv[2]);

    SDF *sdf = SDFopen(NULL, infile);
    if (!sdf) Error("SDFopen %s failed\n", infile);

    int64_t gnobj;
    if (SDFgetint64(sdf, "npart", &gnobj)) Error("SDFget npart failed\n");

    float a, R[NDIM], rmin[NDIM], rmax[NDIM];
    SDFgetfloatOrDie(sdf, "Rx",  &R[0]);
    SDFgetfloatOrDie(sdf, "Ry",  &R[1]);
    SDFgetfloatOrDie(sdf, "Rz",  &R[2]);
    SDFgetfloatOrDie(sdf, "a",  &a);
    
    VV(rmin, = -a*R);
    VV(rmax, = a*R);

    /* expand root for non-power-of-two */
    VS(rmin, *= (1.0 + 0.6)); 
    VS(rmax, *= (1.0 + 0.6));

    FixRsizeExact(rmin, rmax);

    int64_t offset = SDFfileoffset("x", sdf);
    int64_t stride = SDFfilestride("x", sdf);
    int64_t len = SDFnrecs("x", sdf);

    assert(stride == sizeof(body));
    assert(len == gnobj);

    int fd = open(infile, O_RDONLY);
    if (fd == -1) Error("open %s failed\n", infile);
    void *mm = mmap(NULL, offset+len*stride, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mm == MAP_FAILED) Error("mmap %s failed, %s\n", infile, strerror(errno));

    /* If we padded the header to be a multiple of the page size,*/
    /*  we could mmap btab directly */

    body *btab = mm + offset;

    singlPrintf("# index base len octal_key\n");
    singlPrintf("# level=%d\n", level);

    int64_t mask = (1LL<<(level*NDIM))-1;
    for (int64_t i = 0; i < gnobj; /* NULL */) {
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
		singlPrintf("%8ld %12ld %12ld %s\n", this_cell.k[0] & mask, i0, i-i0, PrintKey(GetKeyFast(&btab[i0])));
		break;
	    } else {
		i++;
	    }
	}
    }

    SDFclose(sdf);
    MPMY_Finalize();
    exit(0);
}
