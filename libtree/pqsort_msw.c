#include <stdlib.h>

#include "Assert.h"
#include "Malloc.h"
#include "Msgs.h"
#include "batch.h"
#include "decomp.h"
#include "gc.h"
#include "key.h"
#include "mpmy.h"
#include "poll.h"
#include "pqsort.h"
#include "stk.h"
#include "timers.h"

Timer_t SortTm;

static int cmpkey(const void *k1, const void *k2);
static Key_t (*getkey_s)(const void *);

static Stk instk;
static char *OldPtr;
static char *NewPtr;

#define BATCH_SIZE 65536
#define STK_EXTEND_SZ (16 * 65536) /* instk grows by this much each time */
#define SORT_TAG 4326
#define NPOLL 10

/* Poll calls this function to deal with incoming messages */
static void put_on_instk(void *buf, int size) {
    /* Put it in the gap if there is room */
    if (OldPtr - NewPtr >= size) {
        memcpy(NewPtr, buf, size);
        NewPtr += size;
    } else {
        StkPushData(&instk, buf, size);
    }
}

void pqsortsetup(sortresult_t *decompp,
                 void *bp,
                 int nobj,
                 int size,
                 float median_tol,
                 void *(*realloc_like)(void *, size_t)) {
    pqsortsetup_order(decompp, bp, nobj, size, median_tol, 0, realloc_like);
}

/* This should replace pqsortsetup in the next "major release" */
void pqsortsetup_order(sortresult_t *decompp,
                       void *bp,
                       int nobj,
                       int size,
                       float median_tol,
                       int proc_order,
                       void *(*realloc_like)(void *, size_t)) {
    decompp->data = bp;
    decompp->nobj = nobj;
    decompp->size = size;
    decompp->median_tol = median_tol;
    decompp->proc_order = proc_order;
    decompp->loadbal_target = 1.0; /* default to no load balance */
    decompp->realloc_like = realloc_like;
}

void *pqsort(sortresult_t *decompp, float (*weight)(const void *), Key_t (*getkey)(const void *)) {
    int size, nobj, i, n;
    char *p, *data; /* really void*, but we do a lot of arith. */
    int procnum = MPMY_Procnum();

    Msgf(("pqsort: nobj is %d\n", decompp->nobj));
    getkey_s = getkey;

    size = decompp->size;
    nobj = decompp->nobj;
    data = decompp->data;
    if (MPMY_Nproc() == 1)
        goto sort;

    /* The qsort() in SetupDecomp() will fail mysteriously on the paragon */
    /* if nobj == 0 */
    assert(nobj > 0);

    SetupDecomp(decompp, weight, getkey);
    SetupBatch(SORT_TAG, BATCH_SIZE);
    StkInit(&instk, STK_EXTEND_SZ, Realloc_f, 0);
    PollSetup(put_on_instk, BATCH_SIZE, SORT_TAG);

    /* Strategy: put incoming msgs in the gap created by outgoing msgs */
    /* If there is no space there, save it on a Stk */
    /* Periodically check if some of Stk can be put in the gap */

    StartTimer(&DecompCommTm);
    NewPtr = data;
    for (p = data, i = 0; p < data + nobj * size; p += size, i++) {
        int dest = DestDecomp(p);
        if (i % NPOLL == 0) {
            OldPtr = p;
            Poll(SORT_TAG);
            n = OldPtr - NewPtr;
            if (n > StkSz(&instk))
                n = StkSz(&instk);
            /* Stick some of the instk in the gap */
            if (n) {
                memcpy(NewPtr, StkPop(&instk, n), n);
                NewPtr += n;
            }
        }
        if (dest != procnum)
            SendBatch(p, size, dest);
        else {
            if (NewPtr != p)
                memcpy(NewPtr, p, size);
            NewPtr += size;
        }
    }
    OldPtr = data + nobj * size;
    Msgf(("calling FinishBatch\n"));
    FinishBatch();
    StopTimer(&DecompCommTm);
    Msgf(("calling PollUntilDone\n"));
    PollUntilDone(SORT_TAG);
    Msgf(("calling FinishDecomp\n"));
    FinishDecomp();
    StkCrunch(&instk);

    nobj = (NewPtr - data) / size;
    Msgf(("nobj after send cycle is %d\n", nobj));

    assert(StkSz(&instk) % size == 0);
    data = Realloc(data, nobj * size + StkSz(&instk));
    decompp->data = data;
    memcpy(data + nobj * size, StkBase(&instk), StkSz(&instk));
    nobj += StkSz(&instk) / size;
    decompp->nobj = nobj;
    StkTerminate(&instk);
    Msgf(("Balanced nobj is %6d\n", nobj));

sort:
    assert(nobj > 0);
    StartTimer(&SortTm);
    qsort(decompp->data, decompp->nobj, decompp->size, cmpkey);
    StopTimer(&SortTm);
    Msgf(("first key is %s, ", PrintKey(getkey(decompp->data))));
    Msgf(("last key is %s\n", PrintKey(getkey((char *)decompp->data + (nobj - 1) * size))));
    return decompp->data;
}


static int cmpkey(const void *a, const void *b) {
    Key_t ka;
    Key_t kb;
    ka = getkey_s(a);
    kb = getkey_s(b);
    if (KeyGT(ka, kb))
        return 1;
    else if (KeyLT(ka, kb))
        return -1;
    else
        return 0;
}
