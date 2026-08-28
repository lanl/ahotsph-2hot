/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <utime.h>
#include "Malloc.h"
#include "SDF.h"

int
main(int argc, char *argv[])
{
    if (argc != 2) {
	fprintf(stderr, "usage %s file.sdf\n", argv[0]);
	exit(1);
    }
    
    SDF *sdfp = SDFopen(NULL, argv[1]);
    if (!sdfp) Error("SDFopen %s failed\n", argv[1]);
    
    /* Assumes data section starts with either "mass" or "x" */
    int64_t header_size = SDFfileoffset("x", sdfp);
    if (header_size <= 0) Error("Failed, expected \"x\" in file\n");
    if (SDFhasname("mass", sdfp)) {
	int64_t moffset = SDFfileoffset("mass", sdfp);
	if (moffset > 0 && moffset < header_size) header_size = moffset;
    }
    SDFclose(sdfp);

    char *buffer = Malloc(header_size);

    FILE *infp = fopen(argv[1], "r");
    if (!infp) Error("fopen %s failed\n", argv[1]);
    if (fread(buffer, 1, header_size, infp) != header_size)
	Error("fread failed\n");
    struct stat sb;
    if (fstat(fileno(infp), &sb) == -1) Error("stat failed\n");
    fclose(infp);

    char outname[256];
    snprintf(outname, sizeof(outname), "%s.head", argv[1]);
    FILE *outfp = fopen(outname, "w");
    if (!outfp) Error("fopen %s failed\n", outname);

    if (fwrite(buffer, 1, header_size, outfp) != header_size)
	Error("fread failed\n");
    fclose(infp);
    free(buffer);

    const struct utimbuf ut = {.actime = sb.st_atime, .modtime = sb.st_mtime};
    utime(outname, &ut); /* set ctime to origin file */

    fprintf(stderr, "%s success\n", outname);
    exit(0);
}
