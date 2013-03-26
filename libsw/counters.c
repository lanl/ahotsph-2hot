#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include "Malloc.h"
#include "timers.h"
#include "mpmy.h"
#include "Assert.h"

#define MAXENABLED 100

static Counter_t *enabled_counters[MAXENABLED];
static int nenabled_counters;

void ClearCounter(Counter_t *c){
    c->counter = 0;
}

void ClearEnabledCounters(void){
    int i;
    for(i=0; i<nenabled_counters; i++){
	ClearCounter(enabled_counters[i]);
    }
}

void EnableCounter(Counter_t *c, char *name){
    assert(nenabled_counters < MAXENABLED);
    enabled_counters[nenabled_counters++] = c;
    c->name = Malloc(strlen(name)+1);
    strcpy(c->name, name);
    c->enabled = 1;
    c->counter = 0;
}

void DisableCounter(Counter_t *t){
    int i;

    for(i=0; i<nenabled_counters; i++){
	if( enabled_counters[i] == t )
	    break;
    }
    assert(i < nenabled_counters);
    t->enabled = 0;
    Free(t->name);
    t->name = NULL;
    enabled_counters[i] = enabled_counters[--nenabled_counters];
}

void SumCounters(void){
    double nprocinv;
    Counter_t *c;
    int i;
    MPMY_Comm_request req;

    MPMY_ICombine_Init(&req);
    for(i=0; i<nenabled_counters; i++){
	c = enabled_counters[i];
	c->sum = c->min = c->max = c->counter;
	MPMY_ICombine(&c->min, &c->min, 1, MPMY_INT64, MPMY_MIN, req);
	MPMY_ICombine(&c->max, &c->max, 1, MPMY_INT64, MPMY_MAX, req);
	MPMY_ICombine(&c->sum, &c->sum, 1, MPMY_DOUBLE, MPMY_SUM, req);
    }
    MPMY_ICombine_Wait(req);

    /* Now loop a second time and divide the mean by Nproc */
    nprocinv = 1./MPMY_Nproc();
    for(i=0; i<nenabled_counters; i++){
	c = enabled_counters[i];
	c->mean = c->sum * nprocinv;
    }
}

void OutputCounters(int (*Printf_Like)(const char *, ...)){
    int i;
    Counter_t *c;

    SumCounters();
    Printf_Like("%12s %12s %12s %15s %14s\n", 
	   "Counters", "Min", "Max", "Sum", "Avg");
    for (i = 0; i < nenabled_counters; i++) {
	c = enabled_counters[i];
	if( c->enabled && c->name )
	    Printf_Like("%12s %12ld %12ld %15.0f %14.2f\n", c->name, 
			c->min, c->max, c->sum, c->mean);
    }	
}

void OutputIndividualCounters(int (*Printf_Like)(const char *, ...)){
    int i;
    Counter_t *c;

    Printf_Like("%12s %12s\n", "Counters", "Count");
    for (i = 0; i < nenabled_counters; i++) {
	c = enabled_counters[i];
	if( c->enabled && c->name )
	    Printf_Like("%12s %12ld\n", c->name, c->counter);
    }	
}

int64_t ReadCounter(Counter_t *c){
    return c->counter;
}

int64_t ReadCounter64(Counter_t *c){
    return c->counter;
}

static void SumOneCounter(Counter_t *c){
  MPMY_Comm_request req;
  MPMY_ICombine_Init(&req);
  c->sum = c->min = c->max = c->counter;
  MPMY_ICombine(&c->min, &c->min, 1, MPMY_INT64, MPMY_MIN, req);
  MPMY_ICombine(&c->max, &c->max, 1, MPMY_INT64, MPMY_MAX, req);
  MPMY_ICombine(&c->sum, &c->sum, 1, MPMY_DOUBLE, MPMY_SUM, req);
  MPMY_ICombine_Wait(req);
  c->mean = c->sum / MPMY_Nproc();
}

void OutputOneCounter(Counter_t *c, int (*Printf_Like)(const char *, ...)){
    SumOneCounter(c);
    Printf_Like("%12s %12ld %12ld %14.0g %14.2g\n", 
		(c->name)?c->name:"(noname)", 
		c->min, c->max, c->sum, c->mean);
}
