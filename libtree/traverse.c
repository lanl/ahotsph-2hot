/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

/* These are some alternative traversals I've tried.  They are */
/* in various states of disrepair.  It might still be easier to repair */
/* them than to start afresh.  Or maybe not. */
#include "Assert.h"
#include "Malloc.h"
#include "Msgs.h"
#include "stk.h"
#include "tree.h"

void Traverse0(tree_t *tp, Key_t key, int (*cellf)(hcellptr), int (*bodyf)(hcellptr)) {
    hcellptr pp;
    unsigned int sub_flags;
    unsigned int n;
    Key_t start;
    int nsub = 1 << tp->ndim;
    int sub_mask = (1 << nsub) - 1;
    int type;

    if (Find(tp, key) == NULL)
        return;

    start = key;
    n = 0;
    while (1) {
        pp = Find(tp, key);
        assert(pp);
        type = pp->type;
        if (Sub_Flags(pp)) {
            if (n == 0 && cellf)
                cellf(pp); /* pre-order function cellf */
            if (type & KIDSHERE) {
                /* Set the highest bit to avoid a test in the while loop */
                sub_flags = Sub_Flags(pp) | (1 << (1 << tp->ndim));
                while ((sub_flags & 1 << n) == 0) n++;
            } else {
                n = nsub;
            }
        } else {
            if (bodyf)
                bodyf(pp);
            n = nsub;
        }

        if (n == nsub) {
            do {
                n = KeyAndInt(key, sub_mask) + 1;
                key = KeyRshift(key, tp->ndim); /* go up */
                                                /* A post-order function call could go here? */
            } while (n == nsub);
            if (KeyLT(key, start))
                break;
        } else {
            key = KeyLshift(key, tp->ndim); /* go down */
            key = KeyOrInt(key, n);
            n = 0;
        }
    }
}

/* This is the simplest possible recursive version.  It doesn't */
/* even use sub_flags.  */
void *Tr0(tree_t *tp, Key_t key, int (*pref)(hcellptr), void *(*postf)(hcellptr, void **)) {
    hcellptr pp;
    void *daughters[MAXNSUB];

    pp = Find(tp, key);
    if (pp == NULL)
        return NULL;

    if ((pref && pref(pp)) || (!pref)) {
        unsigned int childnum;
        unsigned int nsub;

        key = KeyLshift(key, tp->ndim);
        nsub = 1 << tp->ndim;
        for (childnum = 0; childnum < nsub; childnum++) {
            daughters[childnum] = Tr0(tp, KeyOrInt(key, childnum), pref, postf);
        }
    }
    if (postf)
        return postf(pp, daughters);
    else
        return NULL;
}

/* A Breadth-first pre-order traversal.  Vectorizable???  No post-func. */
void TrBF(tree_t *tp, hcellptr pp, int (*pref)(hcellptr)) {
    Stk stk1, stk2;
    Stk *instk, *outstk, *tmpstk;
    unsigned int sub_flags, childnum;
    unsigned int ndim = tp->ndim;
    unsigned int nsub = 1 << ndim;
    Key_t key;
    hcellptr *stkp;
    hcellptr *stkend;

    StkInit(&stk1, 4096, Realloc_f, 0);
    StkInit(&stk2, 4096, Realloc_f, 0);
    instk = &stk1;
    outstk = &stk2;
    StkPushType(instk, pp, hcellptr);

    while (StkSz(instk)) {
        stkp = StkBase(instk);
        stkend = StkTop(instk);
        while (stkp < stkend) {
            pp = *stkp;
            if (pref(pp)) {
                key = KeyLshift(pp->key, ndim);
                for (sub_flags = Sub_Flags(pp), childnum = 0; childnum < nsub;
                     childnum++, sub_flags >>= 1) {
                    if ((sub_flags & 1) == 0)
                        continue;
                    StkPushType(outstk, Find(tp, KeyOrInt(key, childnum)), hcellptr);
                }
            }
            stkp = (hcellptr *)((char *)stkp + StkAlign(instk, sizeof(*stkp)));
            ;
        }
        StkClear(instk);
        tmpstk = instk;
        instk = outstk;
        outstk = tmpstk;
    }
    StkTerminate(&stk1);
    StkTerminate(&stk2);
}
