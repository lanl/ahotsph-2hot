#include <time.h>
#include "mpmy_time.h"
#include "Malloc.h"
#include "chn.h"

static Chn timer_chn;
static int initialized;

typedef struct {
    int type;
    struct timespec cpu_start;
    double cpu_accum;
    struct timespec wc_start;
    double wc_accum;
} MPMY_Timer;

void *MPMY_CreateTimer(int type){
    MPMY_Timer *ret;

    if( initialized == 0 ){
	ChnInit(&timer_chn, sizeof(MPMY_Timer), 40, Realloc_f);
	initialized = 1;
    }

    ret = ChnAlloc(&timer_chn);
    ret->type = type;
    MPMY_ClearTimer(ret);
    return (void *)ret;
}

int MPMY_DestroyTimer(void *p){
    ChnFree(&timer_chn, p);
    return MPMY_SUCCESS;
}

int MPMY_StartTimer(void *p){
    MPMY_Timer *t = p;

    switch(t->type){
    case MPMY_CPU_TIME:
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t->cpu_start);
	break;
    case MPMY_WC_TIME:
	clock_gettime(CLOCK_REALTIME, &t->wc_start);
	break;
    }
    return MPMY_SUCCESS;
}

int MPMY_StopTimer(void *p){
    MPMY_Timer *t = p;
    struct timespec tnow;

    switch(t->type){
    case MPMY_CPU_TIME:
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tnow);
	t->cpu_accum += (tnow.tv_sec - t->cpu_start.tv_sec) + (tnow.tv_nsec - t->cpu_start.tv_nsec) * 1e-9;
	break;
    case MPMY_WC_TIME:
	clock_gettime(CLOCK_REALTIME, &tnow);
	t->wc_accum += (tnow.tv_sec - t->wc_start.tv_sec) + (tnow.tv_nsec - t->wc_start.tv_nsec) * 1e-9;
	break;
    }
    return MPMY_SUCCESS;
}

int MPMY_ClearTimer(void *p){
    MPMY_Timer *t = p;

    t->cpu_accum = 0.0;
    t->wc_accum = 0.0;
    return MPMY_SUCCESS;
}

double MPMY_ReadTimer(void *p){
    MPMY_Timer *t = p;

    switch(t->type){
    case MPMY_CPU_TIME:
	return t->cpu_accum;
    case MPMY_WC_TIME:
	return t->wc_accum;
    }
    return -1.0;
}
