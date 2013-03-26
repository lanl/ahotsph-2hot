/* 
   Not-so-trivial implementation of the mpmy interface for a single process(or)
   that handles messages sent to itself.

   This is not designed for speed.  In fact, it's not designed at all.  It's
   just meant to limp along.  I think it's a bad idea to be sending a lot
   of messages to yourself.
*/
#include <string.h>
#include "Msgs.h"
#include "mpmy.h"
#include "Assert.h"
#include "mpmy_io.h"
#include "mpmy_time.h"
#include "mpmy_abnormal.h"

#define IN 1
#define OUT 2

#ifdef __SUN4__
/* ARCH=sun4 code may have been compiled and linked with __CM5VU__, which
   means that if we're not careful we'll hit some illegal instructions when
   we try to call do_grav.  This variable sidesteps those Vector-Unit
   calls at run-time.  It's twisted, but it's the price we pay for using
   ARCH=sun4 for the CM5. */
int have_vu = 0;
void *VUHeap;
int VUaux_allocated;

#if 0
void *aux_alloc_heap(int n){
    Error("aux_alloc_heap:  You shouldn't reach this on a non-cm5 processor!\n");
}
#endif

#endif

struct comm_s{
    int cnt;
    int tag;
    void *buf;
    int inout;
    int finished;
};

/* These do a little  more than mpmy_alloc_generic.  It lets us search */
/* the list of allocated requests too.  Unfortunately, it takes O(Nreq) */
/* time to do a Dealloc or a Match.  I'm sure there's something simpler than */
/* hashing that would do better, but I'm being dense. */

#define MAXCOMM 200
static struct comm_s _comms[MAXCOMM];
static int freecomm[MAXCOMM];
static int usedcomm[MAXCOMM];
static int mpmy_nfree = 0;
static int mpmy_nused = 0;

static void CommInit(void){
    int i;
    for(i=0; i<MAXCOMM; i++){
	freecomm[i] = i;
    }
    mpmy_nfree = MAXCOMM;
    mpmy_nused = 0;
}

static int CommAlloc(void){
    int ret;
    if( mpmy_nfree >= 0 ){
	ret = freecomm[--mpmy_nfree];
	usedcomm[mpmy_nused++] = ret;
	return ret;
    }else
	return -1;
}

static void CommDealloc(int req){
    int i;
    assert(mpmy_nfree < MAXCOMM);
    for(i=0; i<mpmy_nused; i++){
	if( usedcomm[i] == req ){
	    usedcomm[i] = usedcomm[--mpmy_nused];
	    break;
	}
    }
    freecomm[mpmy_nfree++] = req;
}

static int find_match(int inout, int tag){
    int i, ui;
    struct comm_s *comm;

    for(i=0; i<mpmy_nused; i++){
	ui = usedcomm[i];
	comm = &_comms[ui];
	if( !comm->finished 
	   && (comm->tag == tag || tag == MPMY_TAG_ANY || comm->tag == MPMY_TAG_ANY) 
	   && comm->inout == inout )
	    return ui;
    }
    return -1;
}

int MPMY_Isend(const void *buf, int cnt, int dest, int tag, MPMY_Comm_request *reqp){
    struct comm_s *comm;
    int req;

    if( dest != 0 )
	return MPMY_FAILED;
    req = CommAlloc();
    if( req < 0 )
	return MPMY_FAILED;

    comm = &_comms[req];
    comm->inout = OUT;
    comm->cnt = cnt;
    comm->tag = tag;
    comm->buf = (void *)buf;	/* drop const. modifier */
    comm->finished = 0;
    *reqp = comm;
    IncrCounter(&MPMYSendCnt);
    return MPMY_SUCCESS;
}

int MPMY_Irecv(void *buf, int cnt, int src, int tag, MPMY_Comm_request *reqp){
    struct comm_s *comm;
    int req;

    if( src != 0 && src != MPMY_SOURCE_ANY )
	return MPMY_FAILED;
    req = CommAlloc();
    if( req < 0 )
	return MPMY_FAILED;

    comm = &_comms[req];
    comm->inout = IN;
    comm->cnt = cnt;
    comm->tag = tag;
    comm->buf = buf;
    comm->finished = 0;
    *reqp = comm;
    IncrCounter(&MPMYRecvCnt);
    return MPMY_SUCCESS;
}

int MPMY_Test(MPMY_Comm_request req, int *flag, MPMY_Status *stat){
    struct comm_s *comm = req;
    struct comm_s *mcomm;
    int match;
    int ireq = comm - _comms;

    if( comm->finished ){
	*flag = 1;
	if( comm->inout == IN && stat ){
	    stat->count = comm->cnt;
	    stat->tag = comm->tag;
	    stat->src = 0;
	}
	CommDealloc(ireq);
	IncrCounter(&MPMYDoneCnt);
	return MPMY_SUCCESS;
    }
    if( comm->inout == IN ){
	match = find_match(OUT, comm->tag);
	if( match >= 0 ){
	    mcomm = &_comms[match];
	    if( mcomm->cnt > comm->cnt ){
		SeriousWarning("MPMY_Test message too long\n");
		CommDealloc(ireq);
		return MPMY_FAILED;
	    }
	    memcpy(comm->buf, mcomm->buf, mcomm->cnt);
	    if( stat ){
		stat->count = mcomm->cnt;
		stat->tag = mcomm->tag;
		stat->src = 0;
	    }
	    mcomm->finished = 1;
	    *flag = 1;
	    IncrCounter(&MPMYDoneCnt);
	    CommDealloc(ireq);
	    return MPMY_SUCCESS;
	}else{
	    *flag = 0;
	    return MPMY_SUCCESS;
	}
    }else{
	match = find_match(IN, comm->tag);
	if( match >= 0 ){
	    mcomm = &_comms[match];
	    if( comm->cnt > mcomm->cnt ){
		SeriousWarning("MPMY_Test message too long\n");
		CommDealloc(ireq);
		return MPMY_FAILED;
	    }
	    memcpy(mcomm->buf, comm->buf, comm->cnt);
	    mcomm->cnt = comm->cnt;
	    mcomm->tag = comm->tag;
	    mcomm->finished = 1;
	    *flag = 1;
	    IncrCounter(&MPMYDoneCnt);
	    CommDealloc(ireq);
	    return MPMY_SUCCESS;
	}else{
	    *flag = 0;
	    return MPMY_SUCCESS;
	}
    }
}

int
Native_MPMY_Alltoall(void *sendbuf, int sendcount, MPMY_Datatype sendtype, 
	      void *recvbuf, int recvcount, MPMY_Datatype recvtype)
{
    memcpy(recvbuf, sendbuf, sendcount*MPMY_Datasize[sendtype]);
    return MPMY_SUCCESS;
}

int
Native_MPMY_Allgather(void *sendbuf, int sendcount, MPMY_Datatype type, void *recvbuf)
{
    memcpy(recvbuf, sendbuf, sendcount*MPMY_Datasize[type]);
    return MPMY_SUCCESS;
}

int
Native_MPMY_Allgatherv(void *sendbuf, int sendcount, MPMY_Datatype type, void *recvbuf,
		       int *rcounts, int *roffsets)
{
    memcpy(recvbuf, sendbuf, sendcount*MPMY_Datasize[type]);
    return MPMY_SUCCESS;
}



int MPMY_Init(int *argcp, char ***argvp){
    CommInit();
    _MPMY_nproc_ = 1;
    _MPMY_procnum_ = 0;
    _MPMY_initialized_ = 1;
    /* There should really be a better way to opt out of MPMY abnormal
       signal handling.  For now, this will work for programs that might
       use SDF, but which have their own carefully crafted signal handlers,
       e.g., SM */
    if( argcp )
	_MPMY_setup_absigs();
    MPMY_OnAbnormal(MPMY_SystemAbort);
    MPMY_OnAbnormal(MPMY_Abannounce);
    return MPMY_SUCCESS;
}

#ifdef USE_GETTIME
#include "timers_gettime.c"
#endif

#ifdef USE_HWCLOCK
#include "timers_hwclock.c"
#endif

#include "mpmy_io.c"
#include "mpmy_abnormal.c"
#include "mpmy_generic.c"
