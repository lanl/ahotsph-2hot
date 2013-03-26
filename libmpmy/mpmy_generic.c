/* This file is included in most (all?) of the mpmy_PAROS files. 
 It defines several of the required mpmy functions in terms of a 
 more primitive set.  In some cases, however, there will be a PAROS 
 specific way to achieve these results defined in the mpmy_PAROS file
 When that happens, the mpmy_PAROS file will also 
   #define HAVE_MPMY_FUNCNAME
so we know that we shouldn't define it here. */

/* These are set to the "right" thing on a uniprocessor, where it is
   most likely that we will neglect to call MPMY_Init, but where
   we really can proceed without any problems.  I tried
   setting them to -1, but that just led to hard-to-understand
   crashes.  We really should test occasionally that MPMY_Init has
   been called.  But I'll leave that for another day... */
int _MPMY_procnum_ = 0;
int _MPMY_nproc_ = 1;
int _MPMY_procs_per_node_ = 1;
int _MPMY_initialized_ = 0;

Counter_t MPMYSendCnt;
Counter_t MPMYRecvCnt;
Counter_t MPMYDoneCnt;

#ifndef HAVE_MPMY_FLICK
int MPMY_Flick(void){return MPMY_SUCCESS;}
#endif /* HAVE_MPMY_FLICK */

#ifndef HAVE_MPMY_IRSEND
int MPMY_Irsend(const void *buf, int cnt, int dest, int tag, MPMY_Comm_request *req){
    return MPMY_Isend(buf, cnt, dest, tag, req);
}
#endif /* HAVE_MPMY_IRSEND */

#ifndef HAVE_MPMY_SHIFT
/* An implementation of shift that just uses mpmy_isend/irecv */
/* Some systems will have a better option, but this should always work. */

#define SHIFT_TAG 0x1492
/* Because NX can't distinguish different sources when reading */
/* messages, we help it out by adding processor info to the tag. */
/* Is this really necessary for the "generic" implementation? */
int MPMY_Shift(int proc, void *recvbuf, int recvcnt, 
	       const void *sendbuf, int sendcnt, MPMY_Status *stat){
    MPMY_Comm_request inreq, outreq;
    Msgf(("Starting MPMY_Shift(proc=%d, recvcnt=%d, sendcnt=%d:\n",
	  proc, recvcnt, sendcnt));
    if (proc > _MPMY_procnum_) {
      MPMY_Irecv(recvbuf, recvcnt, proc, SHIFT_TAG+proc, &inreq);
      Msgf(("Irecv posted\n"));
      MPMY_Wait(inreq, stat);
      Msgf(("Irecv done\n"));
      MPMY_Isend(sendbuf, sendcnt, proc, SHIFT_TAG+MPMY_Procnum(), &outreq);
      Msgf(("Isend posted\n"));
      MPMY_Wait(outreq, NULL);
      Msgf(("Isend done\n"));
    } else {
      MPMY_Isend(sendbuf, sendcnt, proc, SHIFT_TAG+MPMY_Procnum(), &outreq);
      Msgf(("Isend posted\n"));
      MPMY_Wait(outreq, NULL);
      Msgf(("Isend done\n"));
      MPMY_Irecv(recvbuf, recvcnt, proc, SHIFT_TAG+proc, &inreq);
      Msgf(("Irecv posted\n"));
      MPMY_Wait(inreq, stat);
      Msgf(("Irecv done\n"));
    }

    Msgf(("Finished MPMY_Shift\n"));    
    return MPMY_SUCCESS;
}
#endif /* HAVE_MPMY_SHIFT */

/* 
   This could be smarter.  In particular, it could recover gracefully
   from malloc failing to deliver.
 */
#ifndef HAVE_MPMY_SHIFT_OVERLAP
#include "Malloc.h"
#include <stdlib.h>

int MPMY_Shift_overlap(int proc, void *recvbuf, int recvcnt,
		       const void *sendbuf, int sendcnt,  MPMY_Status *stat){
    void *tmp = Malloc(sendcnt);
    int ret;

    if( tmp == NULL && sendcnt>0 )
	return MPMY_FAILED;
    memcpy(tmp, sendbuf, sendcnt);
    ret = MPMY_Shift(proc, recvbuf, recvcnt, tmp, sendcnt, stat);
    Free(tmp);
    return ret;
}
#endif /* HAVE_MPMY_SHIFT_OVERLAP */

#ifndef HAVE_MPMY_SYNC
/* Implementation of MPMY_Sync in terms of the 'combine' functions. */
/* Some systems might provide a more useful interface. */

int MPMY_Sync(void){
    int junk=0;

    /* I'm sure this is overkill! */
    return MPMY_Combine(&junk, &junk, 1, MPMY_INT, MPMY_BOR);
}
#endif

#ifndef HAVE_MPMY_FINALIZE
/* Any system specific stuff gets handled by a system-specific finalizer */
int MPMY_Finalize(void){
    return MPMY_SUCCESS;
}
#endif

#ifndef HAVE_MPMY_WAIT
/* An implementation of mpmy_wait that just busy-waits on MPMY_Test. */
/* Some systems will have a better option, but this should always work. */

int MPMY_Wait(MPMY_Comm_request req, MPMY_Status *stat){
    /* Should we do a 'MPMY_Flick'' ? */
    int flag = 0;
    int ret;

    do{
	MPMY_Flick();
	ret = MPMY_Test(req, &flag, stat);
    }while( ret == MPMY_SUCCESS && flag==0 );
    return ret;
}
#endif /* HAVE_MPMY_WAIT */

#ifndef HAVE_MPMY_WAIT2
int MPMY_Wait2(MPMY_Comm_request req1, MPMY_Status *stat1,
	       MPMY_Comm_request req2, MPMY_Status *stat2){
    /* Should we do a 'MPMY_Flick'' ? */
    int done1, done2;
    int ret;

    done1 = done2 = 0;
    do{				/* loop until at least one is finished */
	MPMY_Flick();
	ret = MPMY_Test(req1, &done1, stat1);
	if( ret != MPMY_SUCCESS )
	    return ret;
	ret = MPMY_Test(req2, &done2, stat2);
	if( ret != MPMY_SUCCESS )
	    return ret;
    }while( done1 == 0 && done2 == 0 );

    /* Now there's only one left, so we can call MPMY_Wait */
    if( !done1 ){
	ret = MPMY_Wait(req1, stat1);
	if( ret != MPMY_SUCCESS )
	    return ret;
    }
    if( !done2 ){
	ret = MPMY_Wait(req2, stat2);
	if( ret != MPMY_SUCCESS )
	    return ret;
    }
    return MPMY_SUCCESS;
}
#endif 


#ifndef HAVE_MPMY_DIAGNOSTIC
void MPMY_Diagnostic(int (*printflike)(const char *, ...)){
    (*printflike)("No Diagnostic info for this MPMY\n");
}
#endif

#ifndef HAVE_MPMY_INITIALIZED
int MPMY_Initialized(void){
    return _MPMY_initialized_;
}
#endif

#ifndef HAVE_MPMY_PHYSNODE
const char *MPMY_Physnode(void){
  return "?";
}
#endif

#ifndef HAVE_MPMY_TIMERS
/* This is a nightmare!  Timers should really be ARCH dependent */
/* but because of the CM5's special closeness to ARCH=sun4, they */
/* are PAROS dependent, instead.  That means that we would otherwise */
/* repeat this code block in lots of mpmy_PAROS.c files */
#ifdef _AIX
/* No longer working?? Nov 3 1994 
   #  include "timers_readrtc.c" */
#if !defined(USE_HWCLOCK) && !defined(USE_GETTIME)
#include "timers_posix.c"
#endif
/* it's not inconceivable to compile sequentially for the intels */
/* However, we won't necessarily be linking against, e.g, hwclock 
#elif defined(__INTEL_SSD__)
#  include "timers_nx.c"
*/
#else
#if !defined(USE_HWCLOCK) && !defined(USE_GETTIME)
#  include "timers_posix.c"
#endif
#endif /* _AIX */
#endif  /* HAVE_MPMY_TIMERS */

static time_t job_start;
static time_t job_end;
static time_t checkpoint_last;
static time_t checkpoint_next;
static time_t checkpoint_interval;
static int checkpoint_setup;

void
MPMY_CheckpointSetup(int job_seconds, int interval_seconds, int step_seconds)
{
    if (MPMY_Procnum() == 0) {
	job_start = time(NULL);
	if (job_seconds == -1) job_end = -1;
	else job_end = job_start + job_seconds - step_seconds;
	checkpoint_interval = interval_seconds - step_seconds;
	checkpoint_last = job_start;
	checkpoint_next = job_start + checkpoint_interval;
	Msg_do("Checkpoint Setup interval %ld start %ld next %ld end %ld\n",
	       checkpoint_interval, job_start, checkpoint_next, job_end);
    }
    checkpoint_setup = 1;
}

int
MPMY_CheckpointDue(int next_output_seconds)
{
    time_t t;
    int retval = 0;

    if (MPMY_Procnum() == 0) {
	t = time(NULL);
	if (t >= checkpoint_next) retval = 1;
	if (next_output_seconds < checkpoint_interval/4) {
	    Msg_do("Postponing checkpoint since output expected in %d seconds\n", 
		  next_output_seconds);
	    retval = 0;
	}
	if (job_end > 0 && t >= job_end) retval = 1;
    }
    if (retval) Msg_do("Checkpoint Due\n");
    MPMY_Bcast(&retval, 1, MPMY_INT, 0);
    return retval;
}

void
MPMY_CheckpointFinished(void)
{
    time_t t;

    if (MPMY_Procnum() == 0) {
	t = time(NULL);
	checkpoint_last = t;
	checkpoint_next = t + checkpoint_interval;
	if (job_end > 0 && checkpoint_next > job_end) checkpoint_next = job_end;
	Msg_do("next checkpoint %ld (%ld from now)\n", checkpoint_next, checkpoint_next-t);
    }
}

int
MPMY_JobDone(void)
{
    time_t t;
    int retval = 0;

    if (MPMY_Procnum() == 0) {
	t = time(NULL);
	if (job_end > 0 && t >= job_end) {
	    retval = 1;
	    Msg_do("Job Done\n");
	}
    }
    MPMY_Bcast(&retval, 1, MPMY_INT, 0);
    return retval;
}

#ifndef HAVE_MPMY_JOBREMAINING
int
MPMY_JobRemaining(void)
{
    return -1;
}
#endif
