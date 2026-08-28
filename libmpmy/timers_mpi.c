/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

/* This file tries to use only MPI-approved timer constructs. */

/* time.h should define CLOCKS_PER_SECOND and prototype clock() and time()
   and it should have typedefs for time_t and clock_t. */
#include "chn.h"
#include "mpmy_time.h"

static Chn timer_chn;
static int initialized;

typedef struct {
    int type;
    double wc_start, wc_accum;
} MPMY_Timer;

void *MPMY_CreateTimer(int type) {
    MPMY_Timer *ret;

    if (initialized == 0) {
        ChnInit(&timer_chn, sizeof(MPMY_Timer), 40, Realloc_f);
        initialized = 1;
    }

    ret = ChnAlloc(&timer_chn);
    ret->type = type;
    return (void *)ret;
}

int MPMY_DestroyTimer(void *p) {
    ChnFree(&timer_chn, p);
    return MPMY_SUCCESS;
}

int MPMY_CopyTimer(void *p, void *q) {
    MPMY_Timer *t = p;
    MPMY_Timer *u = q;

    *u = *t;
    return MPMY_SUCCESS;
}

int MPMY_StartTimer(void *p) {
    MPMY_Timer *t = p;

    t->wc_start = MPI_Wtime();
    return MPMY_SUCCESS;
}

int MPMY_StopTimer(void *p) {
    MPMY_Timer *t = p;

    t->wc_accum += MPI_Wtime() - t->wc_start;
    return MPMY_SUCCESS;
}

int MPMY_AccumTimer(void *p, double dt) {
    MPMY_Timer *t = p;

    t->wc_accum += dt;
    return MPMY_SUCCESS;
}

int MPMY_ClearTimer(void *p) {
    MPMY_Timer *t = p;

    t->wc_accum = 0.;
    return MPMY_SUCCESS;
}

double MPMY_ReadTimer(void *p) {
    MPMY_Timer *t = p;

    return (double)t->wc_accum;
}
