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

int
main(int argc, char *argv[])
{
    int level = 3;

    MPMY_Init(&argc, &argv);
    singlPrintf("compiled %s %s\n", __DATE__, __TIME__);
    singlPrintf("library %s %s %s\n", version_2HOT, compiled_date_2HOT, compiled_time_2HOT);

    if (argc != 2 && argc != 3) {
	singlPrintf("usage: %s infile [level]\n", argv[0]);
	exit(1);
    }
    char *infile = argv[1];
    if (argc == 3) level = atoi(argv[2]);
    if (level > 10) Error("level too large for int32_t index\n");
    singlPrintf("infile is %s\n", infile);

    SDF *sdf = SDFopen(NULL, infile);
    if (!sdf) Error("SDFopen %s failed\n", infile);

    int64_t gnobj;
    if (SDFgetint64(sdf, "npart", &gnobj)) {
	gnobj = SDFnrecs("x", sdf);
    }

    int sorted_rtp = 0;
    SDFgetint(sdf, "sorted_rtp", &sorted_rtp);
    int sorted_xyz = 0;
    SDFgetint(sdf, "sorted_xyz", &sorted_xyz);
    int morton_xyz = 0;
    SDFgetint(sdf, "morton_xyz", &morton_xyz);
    int wandering_particles = !morton_xyz;

    float rmin[NDIM], rmax[NDIM];
    Key_t (*getkey)(const void *p);
    if (sorted_rtp) {
	float R0;
	SDFgetfloatOrDie(sdf, "R0",  &R0);
	float rtp_min[NDIM] = {0.0, 0.0, -M_PI};
	float rtp_max[NDIM] = {R0*1.01, 2.0*M_PI, M_PI};
	FixRsizeExact(rtp_min, rtp_max);
	getkey = GetKeySphericalFast;
	VV(rmin, = rtp_min);
	VV(rmax, = rtp_max);
    } else if (sorted_xyz) {
	SDFgetfloatOrDie(sdf, "x_min", &rmin[0]);
	SDFgetfloatOrDie(sdf, "y_min", &rmin[1]);
	SDFgetfloatOrDie(sdf, "z_min", &rmin[2]);
	SDFgetfloatOrDie(sdf, "x_max", &rmax[0]);
	SDFgetfloatOrDie(sdf, "y_max", &rmax[1]);
	SDFgetfloatOrDie(sdf, "z_max", &rmax[2]);
	FixRsizeExact(rmin, rmax);
	getkey = GetKeyFast;
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
	    double expand_root = 0.0;
	    int f2 = 1<<(ilog2(ic_Nmesh-1)+1);
	    if (f2 != ic_Nmesh) expand_root = (double)f2/ic_Nmesh - 1.0;
	    VS(rmin, *= (1.0 + expand_root)); 
	    VS(rmax, *= (1.0 + expand_root));
	}
	FixRsizeExact(rmin, rmax);
	getkey = GetKeyFast;
    }

    int64_t offset = SDFfileoffset("x", sdf);
    int64_t stride = SDFfilestride("x", sdf);
    int64_t len = SDFnrecs("x", sdf);

    SDFclose(sdf);

    assert(len == gnobj);

    int fd = open(infile, O_RDONLY);
    if (fd == -1) Error("open %s failed\n", infile);
    void *mm = mmap(NULL, offset+len*stride, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mm == MAP_FAILED) Error("mmap %s failed, %s\n", infile, strerror(errno));

    /* If we padded the header to be a multiple of the page size,*/
    /*  we could mmap btab directly */

    char *btab = mm + offset;

    int64_t mask = (1LL<<(level*NDIM))-1;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();
    int64_t start = procnum * gnobj / nproc;
    int64_t end = (procnum == nproc-1) ? gnobj-1 : (procnum+1) * gnobj / nproc;
    int is_partial = (procnum) ? 1 : 0;

    Stk stk;
    StkInitEz(&stk);
    int need_sparse = 0;
    int64_t nwander = 0;
    int next_index = 0;
    Key_t previous_cell = {};
    if (is_partial) {
	Key_t k = KeyRshift(getkey(btab+start*stride), NDIM*(BITS_PER_DIM-level));
	next_index = (k.k[0] & mask) + 1;
    }

    /* Termination condition really is <=, since next proc didn't know it started at beginning */
    for (int64_t i = start; i <= end; /* NULL */) {
	int64_t i0 = i;
	Key_t this_cell = KeyRshift(getkey(btab+i0*stride), NDIM*(BITS_PER_DIM-level));
	if (KeyLE(this_cell, previous_cell) && !is_partial && !wandering_particles) /* catch unsorted files or bad bounds */
	    Error("input file not sorted by key\n");
	int wandered = 0;
    again:
	while (KeyEQ(this_cell, KeyRshift(getkey(btab+i*stride), NDIM*(BITS_PER_DIM-level)))) i++;
	if (wandering_particles) {
	    /* keys can be sorted in file by positions on previous timestep.  Sigh. */
	    /* Roundoff error could also move particles out of their cell? */
	    /* Decide if i belongs with this cell or next cell */
	    int this_count = 0;
	    int next_count = 0;
	    Key_t next_cell = KeyRshift(getkey(btab+i*stride), NDIM*(BITS_PER_DIM-level));
	    /* Need to look at at least 30 for ds14_a_1.0000 */
	    for (int64_t j = i+1; (j < i+51) && (j <= gnobj); j++) {
		Key_t k = KeyRshift(getkey(btab+j*stride), NDIM*(BITS_PER_DIM-level));
		if (KeyEQ(this_cell, k)) this_count++;
		if (KeyEQ(next_cell, k)) next_count++;
	    }
	    if ((this_count > 1 || next_count < 20) && i < gnobj) {
		i++;
		wandered = 1;
		goto again;
	    }
	}
	nwander += wandered;
	idx_t idx = {.base = i0, .len = i-i0, .index = this_cell.k[0] & mask};
	/* Skip first one (started in the middle) the proc before us will do it */
	if (!is_partial) {
	    if (i-i0 >= (1LL<<30)) Error("cell len too large for int32_t\n");
	    if (idx.index - next_index >= 1024*1024) {
		need_sparse = 1;
	    } else {
		for (int j = next_index; j < idx.index; j++) {
		    idx_t empty = {.base = i0, .len = 0, .index = j}; /* could also leave empty for sparse file */
		    StkPushData(&stk, &empty, sizeof(idx_t));
		}
	    }
	    StkPushData(&stk, &idx, sizeof(idx_t));
	    next_index = idx.index + 1;
	    previous_cell = this_cell;
	} else is_partial = 0;
    }
    close(fd);
    
    int64_t gnout, nout;
    nout = StkSz(&stk)/sizeof(idx_t);
    MPMY_Combine(&nout, &gnout, 1, MPMY_INT64, MPMY_SUM);
    MPMY_Combine(&need_sparse, &need_sparse, 1, MPMY_INT, MPMY_SUM);
    if (wandering_particles) MPMY_Combine(&nwander, &nwander, 1, MPMY_INT64, MPMY_SUM);
    int nindex = 1 << (NDIM*level);
    char outname[256];
    sprintf(outname, "%s.midx%d", infile, level);

    if (need_sparse) {
	SDFwritehdr(outname, OUTIDX,
		    "level", SDF_INT, level,
		    "ndim", SDF_INT, NDIM,
		    "nindex", SDF_INT, nindex,
		    "midx_version", SDF_INT, 1,
		    "sparse_file", SDF_INT, 1,
		    "x_min", SDF_FLOAT, rmin[0],
		    "y_min", SDF_FLOAT, rmin[1],
		    "z_min", SDF_FLOAT, rmin[2],
		    "x_max", SDF_FLOAT, rmax[0],
		    "y_max", SDF_FLOAT, rmax[1],
		    "z_max", SDF_FLOAT, rmax[2],
		    "length_unit", SDF_STRING, "kpc", 
		    "compiled_date_idx", SDF_STRING, __DATE__,
		    "compiled_time_idx", SDF_STRING, __TIME__,
		    "compiled_version_2HOT", SDF_STRING, version_2HOT,
		    "compiled_date_2HOT", SDF_STRING, compiled_date_2HOT,
		    "compiled_time_2HOT", SDF_STRING, compiled_time_2HOT,
		    "filename", SDF_STRING, infile,
		    NULL);

	MPMY_Sync();
	/* stream I/O doesn't have a mode that does what we want */
	int fd2 = open(outname, O_WRONLY, 0644);
	if (fd2 == -1) Error("open %s failed, %s\n", outname, strerror(errno));

	struct stat sb;
	if (fstat(fd2, &sb) == -1) Error("stat failed\n");
	int64_t header_len = sb.st_size;

	singlPrintf("header_len is %ld\n", header_len);
	MPMY_Sync();	       /* get size before other procs write */

	idx_t *idxarr = StkBase(&stk);
	int64_t off = idxarr[0].index * sizeof(idx_t);
	int64_t n = 0;
	for (int64_t i = 0; i < nout; i += n) {
	    /* This will result in a sparse file */
	    off = idxarr[i].index * sizeof(idx_t);
	    n = i;
	    while (n < nout-1 && (idxarr[n].index + 1 == idxarr[n+1].index)) n++; /* find contiguous group */
	    n++;
	    n -= i;
	    printf("%d pwrite %ld at %ld\n", MPMY_Procnum(), n * sizeof(idx_t), header_len+off);
	    if (pwrite(fd2, &idxarr[i].base, n * sizeof(idx_t), header_len+off) != n * sizeof(idx_t))
		Error("pwrite failed, %s\n", strerror(errno));
	    off += n * sizeof(idx_t);
	}
	/* Account for empty cells at the end */
	if (MPMY_Procnum() == MPMY_Nproc()-1 && off < nindex*sizeof(idx_t)) {
	    int empty = 0;
	    printf("%d final pwrite %ld at %ld\n", MPMY_Procnum(), sizeof(int), header_len+nindex*sizeof(idx_t)-sizeof(int));
	    if (pwrite(fd2, &empty, sizeof(int), header_len+nindex*sizeof(idx_t)-sizeof(int)) != sizeof(int))
		Error("pwrite failed, %s\n", strerror(errno));
	}
	close(fd2);
    } else {
	if (gnout != nindex) 
	    Error("Expected %d indices, got %ld\n", nindex, gnout);
	
	SDFwrite64(outname, gnout, 
		   nout, StkBase(&stk), sizeof(idx_t), OUTIDX,
		   "level", SDF_INT, level,
		   "ndim", SDF_INT, NDIM,
		   "nindex", SDF_INT, nindex,
		   "midx_version", SDF_INT, 1,
		   "x_min", SDF_FLOAT, rmin[0],
		   "y_min", SDF_FLOAT, rmin[1],
		   "z_min", SDF_FLOAT, rmin[2],
		   "x_max", SDF_FLOAT, rmax[0],
		   "y_max", SDF_FLOAT, rmax[1],
		   "z_max", SDF_FLOAT, rmax[2],
		   "length_unit", SDF_STRING, "kpc", 
		   "compiled_date_idx", SDF_STRING, __DATE__,
		   "compiled_time_idx", SDF_STRING, __TIME__,
		   "compiled_version_2HOT", SDF_STRING, version_2HOT,
		   "compiled_date_2HOT", SDF_STRING, compiled_date_2HOT,
		   "compiled_time_2HOT", SDF_STRING, compiled_time_2HOT,
		   "filename", SDF_STRING, infile,
		   NULL);
    }
    
    if (wandering_particles) singlPrintf("Saw %ld wandering particles.\n", nwander);
    MPMY_Finalize();
    exit(0);
}
