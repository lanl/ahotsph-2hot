#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <float.h>
#include "Malloc.h"
#include "macr.h"
#define NDIM 3
#include "error.h"
#include "mpmy.h"
#include "Msgs.h"
#include "singlio.h"
#include "timers.h"

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
	FILE *fp = fopen(outname, "r");
	if (fp) {
	    struct stat sb;
	    if (fstat(fileno(fp), &sb) == -1) Error("stat failed");
	    /* Allow zero length file so we can pre-set lustre stripe info */
	    if (sb.st_size > 0) Error("Output file exists.  Will not overwrite.\n");
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
	    Fopen(fp, filename[i], "r");
	    if (fstat(fileno(fp), &sb) == -1) Error("stat failed");
	    sizes[i] = sb.st_size;
	    Fclose(fp);
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

    FILE *outfp = NULL;
    char *buffer = NULL;
    for (int i = 0; i < nfiles; i++) {
	if (i % MPMY_Nproc() == MPMY_Procnum()) {
	    printf("%d reading %s size %ld writing at %ld\n", MPMY_Procnum(), filename[i], sizes[i], offsets[i]);
	    int bufsz = 8*1024*1024;
	    int64_t nbytes;
	    if (!outfp) {
		Fopen(outfp, outname, "w");
		buffer = Malloc(bufsz);
	    }
	    Fseek(outfp, offsets[i], SEEK_SET);
	    Fopen(fp, filename[i], "r");
	    for (int64_t left = sizes[i]; left > 0; left -= nbytes) {
		nbytes = (left > bufsz) ? bufsz : left;
		Fread(buffer, 1, nbytes, fp);
		Fwrite(buffer, 1, nbytes, outfp);
	    }
	    Fclose(fp);
	}
    }
    if (!outfp) {
	Fclose(outfp);
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
