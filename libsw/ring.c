/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include "ring.h"

#include <string.h>

#include "Malloc.h"
#include "Msgs.h"
#include "gc.h"
#include "mpmy.h"
#include "singlio.h"

#define MSGTYPE 142

void Ring(void *bptr,
          int bsize,
          int bnobj,
          void *optr,
          int osize,
          int onobj,
          int oused,
          void initf(void *, void *),
          void interactf(void *, void *, int, int)) {
    char *p;
    void *travel_btab;
    void *tmpbuf;
    int travel_size;
    int n, i;
    int from_proc, to_proc;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();
    int max_nobj = onobj;

    MPMY_Combine(&onobj, &max_nobj, 1, MPMY_INT, MPMY_MAX);

    travel_size = max_nobj * oused;
    travel_btab = Malloc(travel_size);
    tmpbuf = Malloc(travel_size);
    for (i = 0; i < onobj; i++) {
        p = (char *)optr + i * osize;
        initf((char *)travel_btab + i * oused, p);
    }

#if GRAYDECOMP
    /* There should be functions like Gcup() which are periodic */
    to_proc = Gcup(procnum, nproc);
    from_proc = Gcdown(procnum, nproc);
    if (to_proc == -1)
        to_proc = bin2gray(0);
    if (from_proc == -1)
        from_proc = bin2gray(nproc - 1);
#else
    to_proc = (procnum + 1) % nproc;
    from_proc = (procnum + nproc - 1) % nproc;
#endif

    /* local part */
    for (p = bptr; p < (char *)bptr + bnobj * bsize; p += bsize) {
        interactf(p, travel_btab, oused, onobj);
    }

    for (n = 1; n < nproc; n++) {
        MPMY_Comm_request req, req2;
        MPMY_Status stat;

        singlPrintf("cycle %d starting\n", n);
        Msgf(("communicate, cycle %d\n", n));
        /* This uses a lot more memory than packets would */
        memcpy(tmpbuf, travel_btab, travel_size);
        MPMY_Irecv(travel_btab, travel_size, from_proc, MSGTYPE, &req);
        MPMY_Isend(tmpbuf, onobj * oused, to_proc, MSGTYPE, &req2);
        MPMY_Wait2(req, &stat, req2, 0);
        onobj = MPMY_Count(&stat) / oused;

        Msgf(("compute, cycle %d\n", n));
        for (p = bptr; p < (char *)bptr + bnobj * bsize; p += bsize) {
            interactf(p, travel_btab, oused, onobj);
        }
    }
    Free(tmpbuf);
    Free(travel_btab);
}

/* Swap source and sink, and provide finishf() */
void Ring2(void *bptr,
           int bsize,
           int bnobj,
           void *optr,
           int osize,
           int onobj,
           int tsize,
           void initf(void *, void *),
           void interactf(void *, void *, int, int),
           void finishf(void *, void *)) {
    char *p;
    void *travel_btab;
    void *tmpbuf;
    int travel_size;
    int n, i;
    int from_proc, to_proc;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();
    int max_nobj = onobj;
    int initial_onobj = onobj;
    MPMY_Comm_request req, req2;
    MPMY_Status stat;


    MPMY_Combine(&onobj, &max_nobj, 1, MPMY_INT, MPMY_MAX);

    travel_size = max_nobj * tsize;
    travel_btab = Malloc(travel_size);
    tmpbuf = Malloc(travel_size);
    for (i = 0; i < onobj; i++) {
        p = (char *)optr + i * osize;
        initf((char *)travel_btab + i * tsize, p);
    }

#if GRAYDECOMP
    /* There should be functions like Gcup() which are periodic */
    to_proc = Gcup(procnum, nproc);
    from_proc = Gcdown(procnum, nproc);
    if (to_proc == -1)
        to_proc = bin2gray(0);
    if (from_proc == -1)
        from_proc = bin2gray(nproc - 1);
#else
    to_proc = (procnum + 1) % nproc;
    from_proc = (procnum + nproc - 1) % nproc;
#endif

    /* local part */
    for (p = travel_btab; p < (char *)travel_btab + onobj * tsize; p += tsize) {
        interactf(p, bptr, bsize, bnobj);
    }

    for (n = 1; n < nproc; n++) {
        singlPrintf("ring2, cycle %d starting\n", n);
        Msgf(("communicate, cycle %d\n", n));
        /* This uses a lot more memory than packets would */
        memcpy(tmpbuf, travel_btab, onobj * tsize);
        MPMY_Irecv(travel_btab, travel_size, from_proc, MSGTYPE, &req);
        MPMY_Isend(tmpbuf, onobj * tsize, to_proc, MSGTYPE, &req2);
        MPMY_Wait2(req, &stat, req2, 0);
        onobj = MPMY_Count(&stat) / tsize;

        Msgf(("compute, cycle %d\n", n));
        for (p = travel_btab; p < (char *)travel_btab + onobj * tsize; p += tsize) {
            interactf(p, bptr, bsize, bnobj);
        }
    }

    memcpy(tmpbuf, travel_btab, onobj * tsize);
    MPMY_Irecv(travel_btab, travel_size, from_proc, MSGTYPE, &req);
    MPMY_Isend(tmpbuf, onobj * tsize, to_proc, MSGTYPE, &req2);
    MPMY_Wait2(req, &stat, req2, 0);
    onobj = MPMY_Count(&stat) / tsize;
    Free(tmpbuf);

    if (onobj != initial_onobj)
        Error("onobj doesn't match after trip around ring\n");

    Msgf(("finish ring\n"));
    if (finishf) {
        for (i = 0; i < onobj; i++) {
            p = (char *)optr + i * osize;
            finishf((char *)travel_btab + i * tsize, p);
        }
    }
    Free(travel_btab);
}
