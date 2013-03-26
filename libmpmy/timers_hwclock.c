#include <time.h>
#include "mpmy_time.h"
#include "Malloc.h"
#include "chn.h"

static Chn timer_chn;
static int initialized;

typedef struct {
    unsigned long long start;
    unsigned long long accum;
    struct timespec wc_start;
    double wc_accum;
    int type;
} MPMY_Timer;

#ifdef AMD6100
#define DEFAULT_MHZ 2300.0e6
#else
#define DEFAULT_MHZ 2668.0e6
#endif

static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

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

int MPMY_CopyTimer(void *p, void *q)
{
    MPMY_Timer *t = p;
    MPMY_Timer *u = q;

    *u = *t;
    return MPMY_SUCCESS;
}

int MPMY_StartTimer(void *p){
    MPMY_Timer *t = p;

    switch(t->type){
    case MPMY_CPU_TIME:
	t->start = rdtsc();
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
	t->accum += rdtsc()-t->start;
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

    t->accum = 0;
    t->wc_accum = 0.0;
    return MPMY_SUCCESS;
}

double MPMY_ReadTimer(void *p){
    MPMY_Timer *t = p;

    switch(t->type){
    case MPMY_CPU_TIME:
	return t->accum/DEFAULT_MHZ;
    case MPMY_WC_TIME:
	return t->wc_accum;
    }
    return -1.0;
}
