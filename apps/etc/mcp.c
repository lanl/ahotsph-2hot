/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
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
    char *filelist, outdir[256];

    MPMY_Init(&argc, &argv);

    if (argc != 4) {
	fprintf(stderr, "usage: %s outdir filenames nfiles\n", argv[0]);
	exit(1);
    }
    singlPrintf("Welcome to the machine\n");
    strncpy(outdir, argv[1], sizeof(outdir));
    filelist = argv[2];
    nfiles = atoi(argv[3]);

    FILE *fp = fopen(filelist, "r");
    if (!fp) Error("fopen %s failed\n", filelist);
    char filename[nfiles][256];
    for (int i = 0; i < nfiles; i++) {
	if (!fgets(filename[i], 256, fp)) Error("fgets failed\n");
	/* strip carriage returns */
	char *cr = index(filename[i], '\n');
	if (cr) *cr = '\0';
    }

    singlPrintf("Copying %d files on %d procs\n", nfiles, MPMY_Nproc());

    Timer_t wtm;
    EnableTimer(&wtm, "write");
    StartTimer(&wtm);

    char *buffer = NULL;
    int64_t copied = 0;
    int bufsz = 8*1024*1024;
    buffer = Malloc(bufsz);
    for (int i = 0; i < nfiles; i++) {
	if (i % MPMY_Nproc() == MPMY_Procnum()) {
	    struct stat sb, outsb;
	    char outname[256];
	    snprintf(outname, sizeof(outname), "%s/%s", outdir, filename[i]);

	    int outfd = open(outname, O_RDONLY|O_NOATIME);
	    if (outfd != -1) {
		if (fstat(outfd, &outsb) == -1) Error("stat failed\n");
		close(outfd);
	    } else outsb.st_size = 0;

	    int fd = open(filename[i], O_RDONLY|O_NOATIME);
	    if (fd == -1) Error("open %s failed, %s\n", filename[i], strerror(errno));
	    if (fstat(fd, &sb) == -1) Error("stat failed");

	    if (outsb.st_size == sb.st_size) {
		printf("%d %s sizes %ld are same, skipping\n", MPMY_Procnum(), filename[i], sb.st_size);
		close(fd);
		continue;
	    }

	    outfd = open(outname, O_CREAT|O_WRONLY, 0644);
	    if (outfd == -1) Error("open %s failed, %s\n", outname, strerror(errno));

	    printf("%d reading %s size %ld\n", MPMY_Procnum(), filename[i], sb.st_size);

	    int64_t nbytes;
	    for (int64_t left = sb.st_size; left > 0; left -= nbytes) {
		nbytes = (left > bufsz) ? bufsz : left;
		if (read(fd, buffer, nbytes) != nbytes) 
		    Error("read failed, %s\n", strerror(errno));
		if (write(outfd, buffer, nbytes) != nbytes) 
		    Error("write failed, %s\n", strerror(errno));
	    }
	    copied += sb.st_size;
	    close(fd);
	    close(outfd);
	    const struct utimbuf ut = {.actime = sb.st_atime, .modtime = sb.st_mtime};
	    utime(outname, &ut); /* set ctime to origin file */
	}
    }
    Free(buffer);
    MPMY_Combine(&copied, &copied, 1, MPMY_INT64, MPMY_SUM);
    StopTimer(&wtm);
    singlPrintf("%.3f GB/sec\n", 1e-9*copied/ReadTimer(&wtm));

    singlPrintf("Done.\n");
    MPMY_Finalize();
    exit(0);
}
