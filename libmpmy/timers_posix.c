/* This file tries to use only ANSI/POSIX-approved timer constructs. */
/* It should compile correctly everywhere (Ha!) */

/* time.h should define CLOCKS_PER_SECOND and prototype clock() and time()
   and it should have typedefs for time_t and clock_t. */
#include <time.h>
#include "mpmy_time.h"
#include "chn.h"
#include "Malloc.h"

#ifndef CLOCKS_PER_SECOND
/* We've got a non-standard time.h.  At least we have a time.h...*/
#ifdef CLOCKS_PER_SEC	/* This works for linux */
#define CLOCKS_PER_SECOND CLOCKS_PER_SEC
#else
#define CLOCKS_PER_SECOND 1000000 /* this is just a wild guess!! */
#endif
#endif

/* POSIX only guarantees 'time', which returns a time_t.  The option
   is available for the 'implementor' to make time_t a double.  Does
   the friendly implementor at Sun do this?  Nooooo.  If we want more
   precision we have to use gettimeofday, which is a non-POSIX BSD-ism.

   Better still, gettimeofday has mutually incompatible definitions in
   SVr4 (one argument) and XSH4.2 (two arguments).  Sigh...
*/
#if defined(sun) || defined(__INTEL_SSD__) || defined(_AIX) || defined(__x86_64__)
# define USE_GETTIMEOFDAY
# include <sys/time.h>
#else  /* don't use gettimeofday. use time() instead */
extern time_t time(time_t *);
#endif

/* This ought to be in one of the system headers... */
extern clock_t clock(void);

static Chn timer_chn;
static int initialized;

typedef struct {
    int type;
    clock_t cpu_start, cpu_accum;
#ifdef USE_GETTIMEOFDAY
    struct timeval wc_start;
    struct timeval wc_accum;
#else
    time_t wc_start, wc_accum;
#endif
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
    case MPMY_WC_TIME:
#ifdef USE_GETTIMEOFDAY
	gettimeofday(&t->wc_start, 0);
#else
	t->wc_start = time(0);
#endif
	break;
    case MPMY_CPU_TIME:
	t->cpu_start = clock();
	break;
    }
    return MPMY_SUCCESS;
}

int MPMY_StopTimer(void *p){
    MPMY_Timer *t = p;

    switch(t->type){
    case MPMY_WC_TIME:
#ifdef USE_GETTIMEOFDAY
	{ 
	    struct timeval tnow;
	    gettimeofday(&tnow, 0);
	    t->wc_accum.tv_sec += tnow.tv_sec - t->wc_start.tv_sec;
	    t->wc_accum.tv_usec += tnow.tv_usec - t->wc_start.tv_usec;
	}
#else
	t->wc_accum += time(0) - t->wc_start;
#endif
	break;
    case MPMY_CPU_TIME:
	t->cpu_accum += clock() - t->cpu_start;
	break;
    }
    return MPMY_SUCCESS;
}

int MPMY_ClearTimer(void *p){
    MPMY_Timer *t = p;

#ifdef USE_GETTIMEOFDAY
    t->wc_accum.tv_sec = 0;
    t->wc_accum.tv_usec = 0;
#else
    t->wc_accum = 0;
#endif
    t->cpu_accum = 0;
    return MPMY_SUCCESS;
}

double MPMY_ReadTimer(void *p){
    MPMY_Timer *t = p;

    switch(t->type){
    case MPMY_WC_TIME:
#ifdef USE_GETTIMEOFDAY
	return (double)t->wc_accum.tv_sec + (double)t->wc_accum.tv_usec*1.0e-6;
#else
	return (double)t->wc_accum;
#endif	
    case MPMY_CPU_TIME:
	return (double)t->cpu_accum * (1.0/CLOCKS_PER_SECOND);
    }
    return -1.0;
}
