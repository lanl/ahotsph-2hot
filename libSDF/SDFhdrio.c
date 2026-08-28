/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

/* An implementation of getc, using MPMY functions.  */

#include <errno.h>
#include <stddef.h>

#include "Msgs.h"
#include "SDF-private.h"
#include "error.h"
#include "mpmy_io.h"
#include "stdio.h"

#define MYBUFSZ 4096

/* Assumption:  we can only have one header at a time being read.
   The header is opened, read anc closed, all during the course of SDFopen
   so we don't have to screw around with multiple open files, etc. */

static MPMYFile *fp;
static char buf[MYBUFSZ];
static char *ptr;
static char *endbuf;
static int buf_offset;


int SDF_Hdropen(const char *name) {
    fp = MPMY_Fopen(name, MPMY_RDONLY | MPMY_SINGL);
    if (fp == NULL)
        return -1;
    endbuf = ptr = buf;
    buf_offset = 0;
    Msg("SDF", ("SDFhdropen:  return fp=%p\n", fp));
    return 0;
}

void SDF_Hdrclose(void) {
    Msg("SDF", ("SDFhdrclose called\n"));
    MPMY_Fclose(fp);
    endbuf = ptr = NULL;
    fp = NULL;
    buf_offset = -1;
}

int SDF_Hdroffset(void) {
    if (ptr == NULL)
        return -1;
    return buf_offset + (ptr - buf);
}

int SDF_Hdrgetc() {
    int nread;

    if (ptr == NULL) {
        Msgf(("SDFhdrgetc: Returning EOF, ptr==NULL\n"));
        return EOF;
    }

    if (ptr < endbuf) {
        if (*ptr == '\0') {
            SinglWarning("Returning NULL at char %d in SDFhdrio\n", (int)(ptr - buf));
        }
        Msgf(("SDF_hdrgetc:  %c\n", *ptr));
        return *ptr++;
    }

    nread = MPMY_Fread(buf, 1, MYBUFSZ, fp);
    {
        int i;
        int sum = 0;
        for (i = 0; i < nread; i++) { sum ^= buf[i]; }
        Msgf(("SDFhdrio Fread(%d), obtained %d, sum=%d\n", MYBUFSZ, nread, sum));
    }
    if (nread <= 0) {
        if (nread < 0) {
            SinglWarning("SDFhdrio: MPMY_Fread returns %d, errno=%d\n", nread, errno);
        }
        Msgf(("SDFhdrgetc:  Returning EOF, nread=%d, errno=%d\n", nread, errno));
        return EOF; /* don't distinguish between error and EOF? */
    }
    buf_offset += endbuf - buf;
    ptr = buf;
    endbuf = ptr + nread;
    /* This tail recursion is guaranteed to terminate on the second try
       because we have guaranteed that endbuf>ptr */
    return SDF_Hdrgetc();
}
