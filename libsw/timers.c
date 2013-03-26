#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include "Malloc.h"
#include "timers.h"
#include "mpmy.h"
#include "mpmy_time.h"
#include "Assert.h"

/* Make sure any #defines in timers.h don't interfere... */
#undef StartTimer
#undef StopTimer
#undef StartWCTimer
#undef StopWCTimer

#define MAXENABLED 100

static Timer_t *enabled_timers[MAXENABLED];
static int nenabled_timers;

void ClearTimer(Timer_t *t)
{
    MPMY_ClearTimer(t->mpmy_tm);
    return;
}

double ReadTimer(Timer_t *t)
{
    return MPMY_ReadTimer(t->mpmy_tm);
}

void StartTimer(Timer_t *t)
{
    if (t->enabled)
	MPMY_StartTimer(t->mpmy_tm);
    return;
}

void CopyTimer(Timer_t *src, Timer_t *dest)
{
    MPMY_CopyTimer(src->mpmy_tm, dest->mpmy_tm);
}

void StopTimer(Timer_t *t)
{
    if (t->enabled)
	MPMY_StopTimer(t->mpmy_tm);
    return;
}

void ClearEnabledTimers(void){
    int i;
    for(i=0; i<nenabled_timers; i++){
	ClearTimer(enabled_timers[i]);
    }
}

void EnableWCTimer(Timer_t *t, char *name){
    assert(nenabled_timers < MAXENABLED);
    enabled_timers[nenabled_timers++] = t;
    t->name = Malloc(strlen(name)+1);
    strcpy(t->name, name);
    t->enabled = 1;
    t->mpmy_tm = MPMY_CreateTimer(MPMY_WC_TIME);
    ClearTimer(t);
}

void EnableCPUTimer(Timer_t *t, char *name){
    assert(nenabled_timers < MAXENABLED);
    enabled_timers[nenabled_timers++] = t;
    t->name = Malloc(strlen(name)+1);
    strcpy(t->name, name);
    t->enabled = 1;
    t->mpmy_tm = MPMY_CreateTimer(MPMY_CPU_TIME);
    ClearTimer(t);
}

void DisableTimer(Timer_t *t){
    int i;

    for(i=0; i<nenabled_timers; i++){
	if( enabled_timers[i] == t )
	    break;
    }
    assert(i < nenabled_timers);
    t->enabled = 0;
    Free(t->name);
    t->name = NULL;
    enabled_timers[i] = enabled_timers[--nenabled_timers];
    MPMY_DestroyTimer(t->mpmy_tm);
}

void SumTimers(void){
    double nprocinv;
    Timer_t *t;
    int i;
    MPMY_Comm_request req;

    MPMY_ICombine_Init(&req);
    for(i=0; i<nenabled_timers; i++){
	t = enabled_timers[i];
	t->mean = t->min = t->max = MPMY_ReadTimer(t->mpmy_tm);
	MPMY_ICombine(&t->min, &t->min, 1, MPMY_DOUBLE, MPMY_MIN, req);
	MPMY_ICombine(&t->max, &t->max, 1, MPMY_DOUBLE, MPMY_MAX, req);
	MPMY_ICombine(&t->mean, &t->mean, 1, MPMY_DOUBLE, MPMY_SUM, req);
    }
    MPMY_ICombine_Wait(req);

    /* Now loop a second time and divide the mean by Nproc */
    nprocinv = 1./MPMY_Nproc();
    for(i=0; i<nenabled_timers; i++){
	t = enabled_timers[i];
	t->mean *= nprocinv;
    }
}

void OutputTimers(int (*Printf_Like)(const char *, ...)){
    int i;
    Timer_t *t;

    SumTimers();
    Printf_Like("%12s %10s %10s %10s\n", "Timers", "Min", "Max", "Mean");
    for (i = 0; i < nenabled_timers; i++) {
	t = enabled_timers[i];
	if( t->enabled && t->name )
	    Printf_Like("%12s %10.2f %10.2f %10.2f\n", t->name,
			t->min, t->max, t->mean);
    }
}

void OutputTimer(Timer_t *t, int (*Printf_Like)(const char *, ...)){
    MPMY_Comm_request req;

    if( t->enabled && t->name ) {
	MPMY_ICombine_Init(&req);
	t->mean = t->min = t->max = MPMY_ReadTimer(t->mpmy_tm);
	MPMY_ICombine(&t->min, &t->min, 1, MPMY_DOUBLE, MPMY_MIN, req);
	MPMY_ICombine(&t->max, &t->max, 1, MPMY_DOUBLE, MPMY_MAX, req);
	MPMY_ICombine(&t->mean, &t->mean, 1, MPMY_DOUBLE, MPMY_SUM, req);
	MPMY_ICombine_Wait(req);
	t->mean /= MPMY_Nproc();
	Printf_Like("%12s %10.2f %10.2f %10.2f\n", t->name,
		    t->min, t->max, t->mean);
    }
}

void OutputIndividualTimers(int (*Printf_Like)(const char *, ...)){
    int i;
    Timer_t *t;

    Printf_Like("%12s %10s\n", "Timers", "(sec)");
    for (i = 0; i < nenabled_timers; i++) {
	t = enabled_timers[i];
	if( t->enabled && t->name ){
	    Printf_Like("%12s %10.2f\n", t->name, MPMY_ReadTimer(t->mpmy_tm));
	}
    }
}

