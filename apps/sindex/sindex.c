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
    MPMY_Init(&argc, &argv);

    if (argc != 3) {
	singlPrintf("usage: %s infile index\n", argv[0]);
	exit(1);
    }
    char *infile = argv[1];

    SDF *sdf = SDFopen(NULL, infile);
    if (!sdf) Error("SDFopen %s failed\n", infile);

    int64_t gnobj;
    if (SDFgetint64(sdf, "npart", &gnobj)) Error("SDFget npart failed\n");
    singlPrintf("gnobj is %ld\n", gnobj);

    float a, R[NDIM], rmin[NDIM], rmax[NDIM];
    SDFgetfloatOrDie(sdf, "Rx",  &R[0]);
    SDFgetfloatOrDie(sdf, "Ry",  &R[1]);
    SDFgetfloatOrDie(sdf, "Rz",  &R[2]);
    SDFgetfloatOrDie(sdf, "a",  &a);
    
    VV(rmin, = -a*R);
    VV(rmax, = a*R);
    FixRsizeExact(rmin, rmax);

    int64_t offset = SDFfileoffset("x", sdf);
    int64_t stride = SDFfilestride("x", sdf);
    int64_t len = SDFnrecs("x", sdf);

    singlPrintf("offset is %ld nrecs is %ld stride is %ld\n", offset, len, stride);

    assert(stride == sizeof(body));
    assert(len == gnobj);

    int fd = open(infile, O_RDONLY);
    if (fd == -1) Error("open %s failed\n", infile);
    void *mm = mmap(NULL, offset+len*stride, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mm == MAP_FAILED) Error("mmap %s failed, %s\n", infile, strerror(errno));

    /* If we padded the header to be a multiple of the page size,*/
    /*  we could mmap btab directly */

    body *btab = mm + offset;

    int64_t i = atoll(argv[2]);
    Key_t key = GetKeyFast(&btab[i]);
    singlPrintf("%12.8g %12.8g %12.8g %s\n", 
		btab[i].pos[0], btab[i].pos[1], btab[i].pos[2], PrintKey(key));

    SDFclose(sdf);
    MPMY_Finalize();
    exit(0);
}
