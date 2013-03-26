#define NO_TIMERS /* On sun4, the timer in ABMDlvr is a huge penalty.
		     It takes up to half the total processing time! */
/*#define NO_MSGS*/
/*
   ABM:  Asynchronous Buffered Messages.
    Otherwise known as "Active Messages, through the looking glass"
*/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "abm.h"
#include "protos.h"
#include "Assert.h"
#include "mpmy.h"
#include "Malloc.h"
#include "Msgs.h"
#include "verify.h"
#include "gc.h"
#include "dll.h"

#define Ver(x) Verify((x) == MPMY_SUCCESS)

#define ABMDONETYPE -1
#define ABMALLDONETYPE -2

/* Use this for queueing entries for later packetization */
typedef struct{
    void *arg;
    int type;			/* request/reply/done */
    int sz;
    ABMpktz_t *func;		/* how to bundle the data */
} Qelmt_t ;

/* Send exaclty one pkt */
static void SendPkt(ABM *abm, void *ptr, int sz, int dest); 

/* Keep track of the resources tied up in outgoing messages */
static void DeliveryWait(ABM *abm);
static void DeliveryTest(ABM *abm);

/* Common subroutine between ABMPoll and ABMPollWait */
static int ABMPoll_common(ABM *abm, MPMY_Status *status);

/* Keep track of the messages that haven't been MPMY_Test'ed affirmative yet */
typedef struct {
    void *ptr;
    MPMY_Comm_request req;
} undelivered_t;

/* To be "safe", ABMFlushOne should also eliminate dest from destarr[], but 
   the precise behavior of the callers of this function make that
   unnecessary.  If you call ABMFlushOne, you must guarantee that
   destarr[] is left in a correct state when you are done. */
static void ABMFlushOne(ABM *abm, int dest);

static int hist_enable;
Timer_t ABMDlvrTm;
Counter_t ABMIsendCnt;
Counter_t ABMPostCnt;
Counter_t ABMByteCnt;
Counter_t ABMHistCnt[ABMHISTLEN];

void 
ABMHistEnable(int log2lo, int log2hi){
    int i;
    char name[32];

    /* Sanity check the args.  Maybe we should assert these?? */
    if( log2lo < 0 )
	log2lo = 0;
    if( log2hi >= ABMHISTLEN )
	log2hi = ABMHISTLEN-1;
    if( log2lo > log2hi )
	log2lo = log2hi;
    if( log2hi < log2lo )
	log2hi = log2lo;
    for(i=log2lo; i<=log2hi; i++){
	sprintf(name, "Pkt(>=%d)", 1<<i);
	EnableCounter(&ABMHistCnt[i], name);
    }
    hist_enable = 1;
}

void 
ABMSetup(ABM *abm, int pktsize, int tag, int nfuncs, ABMhndlr_t *hndlarray[]){
    int nproc, procnum;
    int i, bit;

    abm->pktsize = pktsize;
    abm->nfuncs = nfuncs;
    abm->hndlarray = Malloc(nfuncs*sizeof(ABMhndlr_t *));
    memcpy(abm->hndlarray, hndlarray, nfuncs*sizeof(ABMhndlr_t *));
    nproc = MPMY_Nproc();
    procnum = MPMY_Procnum();
    abm->doc = ilog2(nproc);
    if (nproc != 1 << (abm->doc))
      abm->doc++;			/* for non power-of-two sizes */
    /* This shouldn't require 10 lines of code... */
    if( procnum == 0 ){
	abm->allbitsdone = (1 << (abm->doc+1))-1;
    }else if( lobit(procnum) == 0 ){
	abm->allbitsdone = 1;
    }else{
	bit = 1<<(lobit(procnum)-1);
	Msg("abmdone", ("bit=%d\n", bit));
	while( (bit^procnum) >= nproc ){
	    Msg("abmdone", ("looping, bit=%d\n", bit));
	    bit >>= 1;
	}
	abm->allbitsdone = (bit==0)? 1 : (bit<<2)-1;
    }
    abm->done = 0;
    abm->alldone = 0;
    Msg("abmdone", ("ABM:allbitsdone=%d, done=%d\n", abm->allbitsdone, abm->done));
    abm->recvbufA = abm->recvbuf1 = Malloc(pktsize);
    abm->recvbufB = abm->recvbuf2 = Malloc(pktsize);
    abm->tag = tag;
    Ver(MPMY_Irecv(abm->recvbufA, abm->pktsize, 
		   MPMY_SOURCE_ANY, abm->tag, &abm->Recv_Hndl));
    abm->Enqueued = Malloc(sizeof(Dll)*nproc);
    abm->destarr = Malloc(sizeof(int)*nproc);
    abm->cntarr = Calloc(nproc, sizeof(int));
    abm->ndests = 0;
    DllCreateChn(&abm->QelmtChn, sizeof(Qelmt_t), 10);
    for(i=0; i<nproc; i++){
	DllCreate(&abm->Enqueued[i], &abm->QelmtChn);
    }

    DllCreateChn(&abm->undelChn, sizeof(undelivered_t), 20);
    DllCreate(&abm->undeliveredLL, &abm->undelChn);
}

void
ABMIamDone(ABM *abm){
    int i, procnum;

    procnum = MPMY_Procnum();
    if( abm->done & 1 ){
	SeriousWarning("ABMIamDone apparently called twice!\n");
	Shout("allbitsdone=%d, done=%d\n", abm->allbitsdone, abm->done);
	return;
    }
    if( abm->alldone ){
	SeriousWarning("ABMIamDone called after alldone\n");
	Shout("allbitsdone=%d, done=%d\n", abm->allbitsdone, abm->done);
	return;
    }
    Msg("abmdone", ("ABMIamDone\n"));
    ABMPost(abm, procnum, 0, ABMDONETYPE, NULL, NULL);
    ABMFlushOne(abm, procnum);
    for(i=abm->ndests-1; i>=0; --i){
	if( abm->destarr[i] == procnum ){
	    abm->destarr[i] = abm->destarr[--abm->ndests];
	    break;
	}
    }
    assert(i >= 0);
}

int
ABMAllDone(ABM *abm){
    return abm->alldone;
}

void
ABMShutdown(ABM *abm){
    int i;
    int junk = 0;
    MPMY_Status stat;
    MPMY_Comm_request req;

    while( !ABMAllDone(abm) ){
	ABMFlush(abm);
	if( ABMPoll(abm) < 0 )
	    Error("AbmPoll failed in ABMShutdown!\n");
    }
    /* There's still a recv outstanding in NLPoll */
    Ver(MPMY_Isend(&junk, sizeof(int), MPMY_Procnum(), abm->tag, &req));
    MPMY_Wait2(req, NULL, abm->Recv_Hndl, &stat);
    assert(MPMY_Source(&stat) == MPMY_Procnum() 
	   && MPMY_Count(&stat) == sizeof(int));

    for(i=0; i<MPMY_Nproc(); i++){
	DllTerminate(&abm->Enqueued[i]);
    }
    ChnTerminate(&abm->QelmtChn);
    Free(abm->hndlarray);
    Free(abm->Enqueued);
    Free(abm->destarr);
    Free(abm->cntarr);
    Free(abm->recvbuf1);
    Free(abm->recvbuf2);
    DllTerminate(&abm->undeliveredLL);
    ChnTerminate(&abm->undelChn);
}

static void 
Donehndlr(ABM *abm, int who){
    int mask, dest, type;
    int procnum= MPMY_Procnum();

    mask = who ^ procnum;

    if( mask == 0 )
	mask = 1;
    else
	mask <<= 1;
    Msg("abmdone", ("Donehndlr(who=%d), mask=%x\n", who, mask));
    if( (abm->done & mask) || !(abm->allbitsdone & mask)){
	SeriousWarning("Unexepected bit set in 'done' or 'allbitsdone':\n");
	Shout("done=%x, allbitsdone=%x, who=%x, procnum=%d, mask=%x\n",
		       abm->done, abm->allbitsdone, who, procnum, mask);
    }
    abm->done |= mask;

    if( abm->done == abm->allbitsdone ){
	if( procnum == 0 ){
	    type = ABMALLDONETYPE;
	    dest = 0;
	    Msg("abmdone", ("Proc 0 is done.  Initiatiing ALLDONE cascade\n"));
	}else{
	    type = ABMDONETYPE;
	    dest = (1<<lobit(procnum)) ^ procnum;
	    Msg("abmdone", ("all bits done.  Forwarding DONE to %d\n", dest));
	    Msg_flush();
	}
	ABMPost(abm, dest, 0, type, NULL, NULL);
	ABMFlush(abm);
    }else{
	Msg("abmdone", ("alldone: %d != abm->done: %d\n", abm->alldone, abm->done));
    }
}

static void
AllDonehndlr(ABM *abm)
{
    int chan;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();

    Msg("abmdone", ("AllDonehndlr\n"));

    for (chan = hibit(MPMY_Procnum())+1; chan < abm->doc; chan++) {
	int dest = procnum ^ (1 << chan);
	if (!(procnum & (1 << chan)) && dest < nproc) {
	    Msgf(("AllDone: informing p%d\n", dest));
	    ABMPost(abm, dest, 0, ABMALLDONETYPE, NULL, NULL);
	    ABMFlush(abm);
	}
    }
    assert(abm->ndests == 0);
    Msg("abmdone", ("Waiting for all Isends to complete\n"));
    Msg_flush();
    DeliveryWait(abm);
    Msg("abmdone", ("Everybody done.  Everybody informed\n"));
    Msg_flush();
    abm->alldone = 1;
}

int
ABMPoll(ABM *abm){
    MPMY_Status status;
    int flag;

    Ver(MPMY_Test(abm->Recv_Hndl, &flag, &status));
    if( flag == 0 )
	return 0;
    return ABMPoll_common(abm, &status);
}

int
ABMPollWait(ABM *abm){
    MPMY_Status status;

    ABMFlush(abm);
    if( Msg_test(__FILE__) ){
	Msg_do("Waiting in ABMPollWait\n"); 
	Msg_flush();
    }
    Ver(MPMY_Wait(abm->Recv_Hndl, &status));
    return ABMPoll_common(abm, &status);
}

static int
ABMPoll_common(ABM *abm, MPMY_Status *status){
    int type;
    int flag;
    int len;
    int sz;
    int src;
    char *in, *end;
    int nloop = 0;
    
    do{
	/* Testing inside the while with a "," operator breaks the T3D */
	nloop++;
	if (MPMY_Tag(status) != abm->tag)
	    Error("Bad tag (%d) in ABMPoll(), should be %d, source %d, len %d\n",
		  MPMY_Tag(status), abm->tag, MPMY_Source(status), 
		  MPMY_Count(status));
	src = MPMY_Source(status);
	len = MPMY_Count(status);
	Msgf(("ABMPoll Received %d byte packet from p%d\n", len, src));

	in = abm->recvbufA;
	Ver(MPMY_Irecv(abm->recvbufB, abm->pktsize, 
		       MPMY_SOURCE_ANY, abm->tag, &abm->Recv_Hndl));
	abm->recvbufA = abm->recvbufB;
	abm->recvbufB = in;
	end = in + len;
	while( in < end ){
	    if( abm->alldone ){
		long int *ip = (long int *)in;
		SeriousWarning("message arrived after alldone.\n");
		Shout("allbitsdone=%d, done=%d\n", abm->allbitsdone,
		      abm->done);
		Shout("src=%d, total len=%d, buf=%#lx, in=%#lx\n",
		      src, len, (long)abm->recvbufB, (long)in);
		Shout("type=%ld, message size: %ld, contents: %#lx %#lx %#lx %#lx\n", 
		      ip[0], ip[1], ip[2], ip[3], ip[4], ip[5]);
		return -1;
	    }
	    type = *(int *)in;
	    in += sizeof(int);
	    sz = *(int *)in;
	    in += sizeof(int);

	    switch(type){
	    case ABMALLDONETYPE:
		Msgf(("ABMALLDONE recvd from p%d.\n", src));
		AllDonehndlr(abm);
		break;
	    case ABMDONETYPE:
		Msgf(("ABMDONE recvd from p%d.\n", src));
		Donehndlr(abm, src);
		break;
	    default:
		Msgf(("ABMPoll type %d\n", type));
		if( type >= 0 && type < abm->nfuncs )
		    abm->hndlarray[type](src, sz, in);
		else{
		    Error("Bad type, %d!\n", type);
		}
		break;
	    }
	    in += sz;
	}
	Ver(MPMY_Test(abm->Recv_Hndl, &flag, status));
    }while(flag);
    return nloop;
}

void
ABMPost(ABM *abm, int dest, int sz, int type, ABMpktz_t *func, void *arg){
    Dll *Q = &abm->Enqueued[dest];
    Qelmt_t *new;

    IncrCounter(&ABMPostCnt);
    if( sz + 2*sizeof(int) > abm->pktsize){
	Error("Can't ABMPost a message of size %d.  pktsize=%d\n", 
	      sz, abm->pktsize);
    } 
    if( DllLength(Q) == 0 ){
	abm->destarr[abm->ndests++] = dest;
    }
    if( abm->cntarr[dest] + sz + 2*sizeof(int) > abm->pktsize ){
	ABMFlushOne(abm, dest);
    }
    abm->cntarr[dest] += sz + 2*sizeof(int);
    Msgf(("ABMPost %ld for p%d, cntarr now %d\n", sz + 2*sizeof(int), dest, abm->cntarr[dest]));
    new = DllData( DllInsertAtBottom(Q) );
    new->sz = sz;
    new->func = func;
    new->arg = arg;
    new->type = type;
}

static int
cmp_xor(const void *p1, const void *p2){
    int d1 = *(int *)p1;
    int d2 = *(int *)p2;
    int procnum = MPMY_Procnum();

    return (d1^procnum) - (d2^procnum);
}

/* To be "safe", ABMFlushOne should also eliminate dest from destarr[], but 
   the precise behavior of the callers of this function make that
   unnecessary.  If you call this function, you must guarantee that
   destarr[] is left in a correct state whey you are done. */
static void
ABMFlushOne(ABM *abm, int dest){
    char *cp;
    void *buf;
    Qelmt_t *Qelmt;
    Dll *Q;
    Dll_elmt *q;
    int szleft, used;

    Q = &abm->Enqueued[dest];
    if( DllLength(Q) == 0 )
	return;

#if 0
    /* This loop is wasting time.  If we get here from ABMFlush, then
       we should already know which element in destarr is in question.
       If we get here from ABMPost, then we know the very next thing
       we are going to do is put the same destination back in destarr.
       Thus, we can just forget this altogether! */
    for(i=0; i<abm->ndests; i++){
	if( abm->destarr[i] == dest )
	    break;
    }
    assert(i < abm->ndests);; /* we actually found one! */
    abm->destarr[i] = abm->destarr[--abm->ndests];
#endif

    Msgf(("ABMFl to p%d\n", dest));
    cp = buf = Malloc(abm->pktsize);
    szleft = abm->pktsize;

    for(q = DllTop(Q); q!=DllInf(Q); q=DllDeleteDown(Q, q)){
	Qelmt = (Qelmt_t *)DllData(q);
	if( Qelmt->sz + 2*sizeof(int) > szleft ){
	    /* Now that ABMPost flushes automatically when the count
	       passes pktsize, this test may be overkill.  It's not worth
	       simplifying. */
	    used = abm->pktsize - szleft;
	    buf = Realloc(buf, used);
	    Msgf(("SendQ full packet for p%d (len=%d)\n", 
		  dest, used));
	    SendPkt(abm, buf, used, dest);
	    cp = buf = Malloc(abm->pktsize);
	    szleft = abm->pktsize;
	}
	/* I think it would be possible to recover here if we
	   introduce another TYPE of message 'NLPKTGROWTYPE' which
	   tells the recipient to Realloc his receive buf.  A good project
	   for a rainy day.... */
	assert(Qelmt->sz + 2*sizeof(int) <= szleft && Qelmt->sz >= 0 );
	*(int *)cp = Qelmt->type;
	cp += sizeof(int);
	szleft -= sizeof(int);
	*(int *)cp = Qelmt->sz;
	cp += sizeof(int);
	szleft -= sizeof(int);
	if( Qelmt->func ){
	    Qelmt->func(cp, Qelmt->arg, Qelmt->sz);
	    cp += Qelmt->sz;
	    szleft -= Qelmt->sz;
	}
    }
    assert(szleft < abm->pktsize);
    used = abm->pktsize - szleft;
    Msgf(("SendQ: Remaining packet for p%d (len=%d)\n", 
	  dest, used));
    SendPkt(abm, buf, used, dest);
    abm->cntarr[dest] = 0;
}

void 
ABMFlush(ABM *abm){
    int i;

    if (abm->ndests == 0) return; /* Required to avoid huge overhead. */

    /* reorder the destinations so everybody doesn't immediately dump on 0 ! */
    qsort(abm->destarr, abm->ndests, sizeof(int), cmp_xor); /* unnecessary? */

    /* loop over the destinations that have something queued. */
    for(i=0; i<abm->ndests; i++){
	ABMFlushOne(abm, abm->destarr[i]);
    }
    abm->ndests = 0;
    DeliveryTest(abm);
}


static
void SendPkt(ABM *abm, void *ptr, int sz, int dest){
    undelivered_t *new;

    new = DllData(DllInsertAtTop(&abm->undeliveredLL));
    new->ptr = ptr;
    AddCounter(&ABMByteCnt, sz);
    IncrCounter(&ABMIsendCnt);
    if( hist_enable ){
	int h = ilog2(sz);
	/* If I weren't so lazy, I'd record the outliers separately... */
	if( h < ABMHISTFIRST ) h = ABMHISTFIRST;
	if( h >=ABMHISTLEN ) h = ABMHISTLEN-1;
	/* We could just increment these by 1, or by sz.  You learn
	   something slightly different either way... */
	AddCounter(&ABMHistCnt[h], sz);
    }
    Ver(MPMY_Isend(ptr, sz, dest, abm->tag, &new->req));
    /* Calling DeliveryTest here is not strictly necessary for correctness,
       but it is a good idea to try to reduce the number of outstanding
       requests.  See the comment near DeliverTest for other ideas */
    DeliveryTest(abm);
}

/* It might be useful to have two versions of this.  The one called
   from ABMFlush (and hence at user request) should be aggressive and try
   each and every outstanding request.  The one called from ABMFlushOne,
   (and hence more frequently, but perhaps asynchronously), could give up
   after the first failure.  This would keep those machines that need
   constant prodding working, without putting an unnecessary burden on
   others. */
static void DeliveryTest(ABM *abm){
    int flag;
    undelivered_t *p;
    Dll_elmt *pp;

    /* Another good opportunity to call Flick?  This is done once per
     ABMFlush. */
    StartTimer(&ABMDlvrTm);
    MPMY_Flick();
    for(pp=DllBottom(&abm->undeliveredLL); 
	pp != DllSup(&abm->undeliveredLL); 
	/* pp incremented inside body */){
	p = DllData(pp);
	Ver( MPMY_Test(p->req, &flag, NULL) );
	if( flag ){
	    Free(p->ptr);
	    pp = DllDeleteUp(&abm->undeliveredLL, pp);
	}else{
	    pp = DllUp(pp);
	}
    }
    StopTimer(&ABMDlvrTm);
}

static void DeliveryWait(ABM *abm){
    Msgf(("ABMWait\n"));
    while( DllLength(&abm->undeliveredLL) ){
	DeliveryTest(abm);
    }
}

