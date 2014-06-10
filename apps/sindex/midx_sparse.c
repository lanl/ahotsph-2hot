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

static ssize_t
locked_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    int64_t lock_offset = offset;
    int64_t lock_count = count;
    int64_t pagesize = 1024*1024;
    int64_t partial_page = offset % pagesize;

    if (partial_page) {
	lock_offset -= partial_page;
	lock_count += partial_page;
    }
    lock_count += pagesize-1;
    lock_count &= ~(pagesize-1);
    
    if (lseek(fd, lock_offset, SEEK_SET) == -1) return -1;
    if (lockf(fd, F_LOCK, lock_count) == -1) return -1;
    ssize_t ret = pwrite(fd, buf, count, offset);
    if (lseek(fd, lock_offset, SEEK_SET) == -1) return -1; /* needed? */
    if (lockf(fd, F_ULOCK, lock_count) == -1) return -1;
    return ret;
}

int
main(int argc, char *argv[])
{
    int text_output = 0;
    int idx1_output = 0;
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
	float R0;
	int offset_center = 0;
	SDFgetfloatOrDie(sdf, "R0",  &R0);
	SDFgetint(sdf, "offset_center", &offset_center);
	if (offset_center) {
	    VS(rmin, = 0.0f);
	    VS(rmax, = 2.0f * R0);
	} else {
	    VS(rmin, = -R0*1.01);	/* must match lcjoin */
	    VS(rmax, =  R0*1.01);
	}
	FixRsizeExact(rmin, rmax);
	getkey = GetKeyFast;
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

    FILE *fp = stdout;
    if (nproc > 1 && text_output) {
	char filename[256];
	sprintf(filename, "index/%s_mindex.%04d", infile, procnum);
	fp = fopen(filename, "w");
	if (!fp) Error("fopen %s failed, %s\n", filename, strerror(errno));
    }
    Stk stk;
    StkInitEz(&stk);

    if (text_output) {
	fprintf(fp, "# base len index octal_key\n");
	fprintf(fp, "# level=%d\n", level);
    }

    int nlines = 0;
    /* Termination condition really is <=, since next proc didn't know it started at beginning */
    for (int64_t i = start; i <= end; /* NULL */) {
	int64_t i0 = i;
	Key_t this_cell = KeyRshift(getkey(btab+i0*stride), NDIM*(BITS_PER_DIM-level));
	while (1) {
	    while (KeyEQ(this_cell, KeyRshift(getkey(btab+i*stride), NDIM*(BITS_PER_DIM-level)))) i++;
	    if (wandering_particles) {
		/* keys are sorted in file by positions on previous timestep.  Sigh. */
		/* If at least 8 of next 10 are not in this cell, then it's really a new cell */
		int count = 0;
		Key_t next_cell = KeyRshift(getkey(btab+i*stride), NDIM*(BITS_PER_DIM-level));
		for (int64_t j = i+1; (j < i+11) && (j <= end); j++) {
		    if (KeyEQ(next_cell, KeyRshift(getkey(btab+j*stride), NDIM*(BITS_PER_DIM-level)))) count++;
		}
		if (count < 8 && i < end) {
		    i++;
		    continue;
		}
	    }
	    /* Skip first one (started in the middle) the proc before us will do it */
	    if (!is_partial) {
		idx_t idx = {.base = i0, .len = i-i0, .index = this_cell.k[0] & mask};
		if (i-i0 >= (1LL<<30)) Error("cell len too large for int32_t\n");
		StkPushData(&stk, &idx, sizeof(idx_t));
		if (text_output) {
		    fprintf(fp, "%12ld %6ld %8ld %s\n", i0, i-i0, this_cell.k[0] & mask, PrintKey(getkey(btab+i0*stride)));
		    if (++nlines % 1000) fflush(fp);
		}
	    } else is_partial = 0;
	    break;
	}
    }
    int64_t gnout, nout;
    nout = StkSz(&stk)/sizeof(idx_t);
    MPMY_Combine(&nout, &gnout, 1, MPMY_INT64, MPMY_SUM);
    int nindex = 1 << (NDIM*level);

    char outname[256];
    sprintf(outname, "%s.midx%d", infile, level);
    SDFwritehdr(outname, OUTIDX,
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
    printf("%d start %ld end %ld nout %ld\n", MPMY_Procnum(), start, end, nout);
    printf("%d index[0] %d index[-1] %d\n", MPMY_Procnum(), idxarr[0].index, idxarr[nout-1].index);
    for (int64_t i = 0; i < nout; i += n) {
	/* This can result in a sparse file */
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

    if (idx1_output) {
	sprintf(outname, "%s.midx1_%d", infile, level);

	SDFwrite64(outname, gnout, 
		   nout, StkBase(&stk), sizeof(idx_t), OUTIDX,
		   "level", SDF_INT, level,
		   "ndim", SDF_INT, NDIM,
		   "nindex", SDF_INT, nindex,
		   "midx1_version", SDF_INT, 1,
		   "x_min", SDF_DOUBLE, rmin[0],
		   "y_min", SDF_DOUBLE, rmin[1],
		   "z_min", SDF_DOUBLE, rmin[2],
		   "x_max", SDF_DOUBLE, rmax[0],
		   "y_max", SDF_DOUBLE, rmax[1],
		   "z_max", SDF_DOUBLE, rmax[2],
		   "rsize", SDF_DOUBLE, rmax[2]-rmin[2],
		   "filename", SDF_STRING, infile,
		   NULL);
    }
    MPMY_Finalize();
    exit(0);
}
