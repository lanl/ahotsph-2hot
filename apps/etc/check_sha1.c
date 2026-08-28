/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include "mpmy.h"
#include "singlio.h"
#include "error.h"
#include "Malloc.h"
#include "SDF.h"
#include "SDFread.h"

struct md_s {
    unsigned int sha1_len;
    unsigned char sha1[SHA_DIGEST_LENGTH];
} __attribute__ ((packed));

int
main(int argc, char *argv[])
{
    MPMY_Init(&argc, &argv);

    if (argc != 4) {
	fprintf(stderr, "usage: %s filename start end\n", argv[0]);
	exit(1);
    }

    singlPrintf("Welcome to the machine\n");
    char filename[256];
    strncpy(filename, argv[1], sizeof(filename));
    int start = atoi(argv[2]);
    int end = atoi(argv[3]);

    SDF *sdfp;
    if (!(sdfp = SDFopen(NULL, filename))) Error("SDFopen failed, %s\n", SDFerrstring);
    int sha1_chunks = 0;
    SDFgetintOrDie(sdfp, "sha1_chunks", &sha1_chunks);
    singlPrintf("sha1_chunks = %d\n", sha1_chunks);
    int64_t offset = SDFfileoffset("x", sdfp);
    singlPrintf("offset = %ld\n", offset);
        
    struct md_s *mdtab = Calloc(sha1_chunks, sizeof(struct md_s));
    void *addrs[2] = {&mdtab[0].sha1_len, &mdtab[0].sha1};
    char *names[2] = {"sha1_len", "sha1"};
    int strides[2] = {sizeof(struct md_s), sizeof(struct md_s)};
    int nobjs[2] = {sha1_chunks, sha1_chunks};
    int64_t starts[2] = {0L, 0L};
    if (SDFseekrdvecsarr(sdfp, 2, names, starts, nobjs, addrs, strides))
	Error("SDFseekrdvecsarr failed, %s\n", SDFerrstring);

    int64_t sha1_offsets[sha1_chunks];

    sha1_offsets[0] = offset;
    for (int i = 1; i < sha1_chunks; i++)
	sha1_offsets[i] = sha1_offsets[i-1] + mdtab[i-1].sha1_len;

    struct md_s md;
    unsigned char *buf;
    FILE *fp = fopen(filename, "r");
    if (!fp) Error("fopen failed, %s\n", strerror(errno));

    for (int i = start; i < end; i++) {
	md.sha1_len = mdtab[i].sha1_len;
	buf = Malloc(md.sha1_len);
	if (fseek(fp, sha1_offsets[i], SEEK_SET)) Error("fseek failed, %s\n", strerror(errno));
	if (fread(buf, 1, md.sha1_len, fp) != md.sha1_len) Error("fread failed, %s\n", strerror(errno));
	SHA1(buf, md.sha1_len, md.sha1);
	free(buf);

	for (int j = 0; j < SHA_DIGEST_LENGTH; j++) {
	    if (md.sha1[j] != mdtab[i].sha1[j]) 
		Error("sha1 does not match, chunk %d\n", i);
	    printf("%02x", mdtab[i].sha1[j]);
	}
	printf(" %d\n", i);
    }
    singlPrintf("Done.\n");
    MPMY_Finalize();
    exit(0);
}
