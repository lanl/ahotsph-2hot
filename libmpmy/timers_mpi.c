/* This file tries to use only MPI-approved timer constructs. */

/* time.h should define CLOCKS_PER_SECOND and prototype clock() and time()
   and it should have typedefs for time_t and clock_t. */
#include "mpmy_time.h"
#include "chn.h"

static Chn timer_chn;
static int initialized;

typedef struct {
    int type;
    double wc_start, wc_accum;
} MPMY_Timer;

void *MPMY_CreateTimer(int type){
    MPMY_Timer *ret;

    if( initialized == 0 ){
	ChnInit(&timer_chn, sizeof(MPMY_Timer), 40, Realloc_f);
	initialized = 1;
    }

    ret = ChnAlloc(&timer_chn);
    ret->type = type;
    return (void *)ret;
}

int MPMY_DestroyTimer(void *p){
    ChnFree(&timer_chn, p);
    return MPMY_SUCCESS;
}

int MPMY_StartTimer(void *p){
    MPMY_Timer *t = p;

    t->wc_start = MPI_Wtime();
    return MPMY_SUCCESS;
}

int MPMY_StopTimer(void *p){
    MPMY_Timer *t = p;

    t->wc_accum += MPI_Wtime() - t->wc_start;
    return MPMY_SUCCESS;
}

int MPMY_ClearTimer(void *p){
    MPMY_Timer *t = p;

    t->wc_accum = 0.;
    return MPMY_SUCCESS;
}

double MPMY_ReadTimer(void *p){
    MPMY_Timer *t = p;

    return (double)t->wc_accum;
}
