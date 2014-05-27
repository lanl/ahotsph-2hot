#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "Malloc.h"
#include "macr.h"
#include "error.h"
#include "mpmy.h"
#include "Msgs.h"
#include "singlio.h"
#include "timers.h"

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

int
main(int argc, char *argv[])
{
    int nfiles;
    char *filelist, outname[256];

    MPMY_Init(&argc, &argv);

    if (argc != 4) {
	fprintf(stderr, "usage: %s outname filenames nfiles\n", argv[0]);
	exit(1);
    }
    singlPrintf("Welcome to the machine\n");
    strncpy(outname, argv[1], sizeof(outname));
    filelist = argv[2];
    nfiles = atoi(argv[3]);

    if (MPMY_Procnum() == 0) {
	int fd = open(outname, O_RDONLY|O_NOATIME);
	if (fd != -1) {
	    struct stat sb;
	    if (fstat(fd, &sb) == -1) Error("stat failed");
	    /* Allow zero length file so we can pre-set lustre stripe info */
	    if (sb.st_size > 0) Error("Output file exists.  Will not overwrite.\n");
	    close(fd);
	}
    }

    FILE *fp = fopen(filelist, "r");
    if (!fp) Error("fopen %s failed\n", filelist);
    char filename[nfiles][256];
    for (int i = 0; i < nfiles; i++) {
	if (!fgets(filename[i], 256, fp)) Error("fgets failed\n");
	/* strip carriage returns */
	char *cr = index(filename[i], '\n');
	if (cr) *cr = '\0';
    }

    singlPrintf("Getting sizes for %d files\n", nfiles);

    int64_t *sizes = Calloc(nfiles, sizeof(int64_t));
    for (int i = 0; i < nfiles; i++) {
	if (i % MPMY_Nproc() == MPMY_Procnum()) {
	    struct stat sb;
	    int fd = open(filename[i], O_RDONLY|O_NOATIME);
	    if (fd == -1) Error("open %s failed, %s\n", filename[i], strerror(errno));
	    if (fstat(fd, &sb) == -1) Error("stat failed");
	    sizes[i] = sb.st_size;
	    close(fd);
	}
    }

    /* logical or with zeros from other procs to concat */
    MPMY_Combine(sizes, sizes, nfiles, MPMY_INT64, MPMY_BOR);

    int64_t *offsets = Malloc((nfiles+1) * sizeof(int64_t));
    offsets[0] = 0;
    for (int i = 1; i <= nfiles; i++) {
	offsets[i] = offsets[i-1] + sizes[i-1];
    }

    singlPrintf("Reading %d files on %d procs\n", nfiles, MPMY_Nproc());

    Timer_t wtm;
    EnableTimer(&wtm, "write");
    StartTimer(&wtm);

    int outfd = -1;
    char *buffer = NULL;
    for (int i = 0; i < nfiles; i++) {
	if (i % MPMY_Nproc() == MPMY_Procnum()) {
	    printf("%d reading %s size %ld writing at %ld\n", MPMY_Procnum(), filename[i], sizes[i], offsets[i]);
	    int bufsz = 8*1024*1024;
	    int64_t nbytes;
	    if (outfd == -1) {
		/* stream I/O doesn't have a mode that does what we want */
		outfd = open(outname, O_CREAT|O_WRONLY, 0644);
		if (outfd == -1) Error("open %s failed, %s\n", outname, strerror(errno));
		buffer = Malloc(bufsz);
	    }
	    int fd = open(filename[i], O_RDONLY|O_NOATIME);
	    if (fd == -1) Error("open %s failed, %s\n", filename[i], strerror(errno));
	    int64_t off = offsets[i];
	    for (int64_t left = sizes[i]; left > 0; left -= nbytes) {
		nbytes = (left > bufsz) ? bufsz : left;
		if (read(fd, buffer, nbytes) != nbytes) 
		    Error("read failed, %s\n", strerror(errno));
		if (pwrite(outfd, buffer, nbytes, off) != nbytes) 
		    Error("pwrite failed, %s\n", strerror(errno));
		off += nbytes;
	    }
	    close(fd);
	}
    }
    if (outfd != -1) {
	close(outfd);
	Free(buffer);
    }
    MPMY_Sync();
    StopTimer(&wtm);
    singlPrintf("%.3f GB/sec\n", 1e-9*offsets[nfiles]/ReadTimer(&wtm));

    Free(sizes);
    Free(offsets);

    singlPrintf("Done.\n");
    MPMY_Finalize();
    exit(0);
}
