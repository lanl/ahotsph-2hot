#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
#include "version_2HOT.h"

/* This is only valid at levels in the tree with less than 2G particles in each cell */
/* and less than 2G cells at that level */
typedef struct {
    int64_t base;
    int32_t len;
    int32_t index;	     /* Get rid of this?  Alignment issues? */
} idx_t;

#define OUTIDX \
"struct {\n\
    int64_t base;	/* offset of first object in cell */\n\
    int len;		/* number of objects in this cell */\n\
    int index;		/* cell morton index */\n\
}"

static void fail(idx_t *idx, int i) 
{
    printf("failed.\n");
    printf("%d %ld %d %d\n", i-2, idx[i-2].base, idx[i-2].len, idx[i-2].index);
    printf("%d %ld %d %d\n", i-1, idx[i-1].base, idx[i-1].len, idx[i-1].index);
    printf("%d %ld %d %d\n", i  , idx[i  ].base, idx[i  ].len, idx[i  ].index);
    printf("%d %ld %d %d\n", i+1, idx[i+1].base, idx[i+1].len, idx[i+1].index);
    exit(1);
}

int
main(int argc, char *argv[])
{
    MPMY_Init(&argc, &argv);

    if (argc != 2 && argc != 3) {
	singlPrintf("usage: %s infile [header]\n", argv[0]);
	exit(1);
    }
    char *infile = argv[1];
    char *header = NULL;
    if (argc == 3) header = argv[2];

    SDF *sdf = SDFopen(header, infile);
    if (!sdf) Error("SDFopen %s failed\n", infile);

    int64_t offset = SDFfileoffset("base", sdf);
    int64_t stride = SDFfilestride("base", sdf);
    int64_t len = SDFnrecs("base", sdf);

    int fd = open(infile, O_RDONLY);
    if (fd == -1) Error("open %s failed\n", infile);
    void *mm = mmap(NULL, offset+len*stride, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mm == MAP_FAILED) Error("mmap %s failed, %s\n", infile, strerror(errno));

    idx_t *idx = mm + offset;

    int64_t next_base = 0;
    for (int i = 0; i < len; i++) {
	if (idx[i].len) {
	    if (idx[i].index != i) fail(idx, i);
	    if (idx[i].base != next_base) fail(idx, i);
	    next_base = idx[i].base + idx[i].len;
	}
    }
    singlPrintf("%s ok\n", infile);
    exit(0);
}
