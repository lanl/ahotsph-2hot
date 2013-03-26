#include <string.h>
#include "mpmy.h"
#include "Msgs.h"
#include "assert.h"
#include "gc.h"
#include "timers.h"
#include "error.h"
#include "dll.h"
#include "chn.h"
#include "poll.h"

#define INBUFSZ (16384*sizeof(int))
#define MAXRELAY 512
#define MAXLOCAL 64
#define MAXREMOTE 2048
Timer_t PollWaitTm;

static void (*func)();
static int size;
static int polldone;
static int localdone;
static int remotedone;
static int localid;
static int nremote;
static MPMY_Comm_request inreq;
static char localpoll[MAXLOCAL];
static char remotepoll[MAXREMOTE];
static int bufused[MAXRELAY];
MPMY_Comm_request Rreq[MAXRELAY];
static int relaybuf[MAXRELAY][INBUFSZ/sizeof(int)];
static int inbuf[INBUFSZ/sizeof(int)]; /* avoid using malloc */

static int
allocbuf(void)
{
    int i;
    int flag;
    int inuse;

    inuse = 0;
    for (i = 0; i < MAXRELAY; i++) {
	if (bufused[i] == 1) {
	    if (MPMY_Test(Rreq[i], &flag, 0), flag) {
		bufused[i] = 0;
	    } else inuse++;
	}
    }
    Msgf(("%d relay inuse\n", ++inuse));
    for (i = 0; i < MAXRELAY; i++) {
	if (bufused[i] == 0) {
	    bufused[i] = 1;
	    return i;
	}
    }
    Error("Out of relay buffers\n");
}


static void
process(int count, int tag)
{
    int dest = inbuf[0];

    if (dest < 0 || dest >= MPMY_Nproc()) {
	Error("Bad dest in Poll()\n");
    }
    if (dest != MPMY_Procnum()) { /* relay */
	int i;
	i = allocbuf();
	Msgf(("PollRelay: %d to %d using buffer %d\n", count, dest, i));
	memcpy(relaybuf[i], inbuf, count);
	MPMY_Isend(relaybuf[i], count, dest, tag, Rreq+i);
	MPMY_Irecv(&inbuf, size, MPMY_SOURCE_ANY, tag, &inreq);
    } else if (count == 2*sizeof(int)) {
	int src = inbuf[1];
	if (src < 0 || src >= MAXLOCAL+MAXREMOTE) {
	    Error("Bad src in Poll()\n");
	}
	Msgf(("PollDone msg from %d\n", src));
	if (localid) polldone = 1;
	else if (src < MAXLOCAL) {
	    /* local done message */
	    if (src >= MPMY_ProcsPerNode()) Error("Bad src in Poll()\n");
	    if (localpoll[src]) 
		Error("localpoll[%d] already set!\n", src);
	    if (localdone >= MPMY_ProcsPerNode()) Error("localdone already acheived\n");
	    localpoll[src] = 1;
	    localdone++;
	    Msgf(("Poll: localdone is %d\n", localdone));
		} else {
	    /* remote done message */
	    src -= MAXLOCAL;
	    if (src >= nremote) Error("Bad src in Poll()\n");
	    if (remotepoll[src])
		Error("remotepoll[%d] already set!\n", src);
	    if (remotedone >= nremote) Error("remotedone already acheived\n");
	    remotepoll[src] = 1;
	    if (++remotedone >= nremote) polldone = 1;
	    Msgf(("Poll: remotedone is %d\n", remotedone));
	}
	MPMY_Irecv(&inbuf, size, MPMY_SOURCE_ANY, tag, &inreq);
    } else {
	Msgf(("PollProcess: %d\n", count));
	func(inbuf+1, count-sizeof(int));
	MPMY_Irecv(&inbuf, size, MPMY_SOURCE_ANY, tag, &inreq);
    } 
}


void
PollSetup(void put(void *buf, int size), int max_size, int tag)
{
    int i;
    int procs_per_node = MPMY_ProcsPerNode();

    Msgf(("PollSetup\n"));
    func = put;
    if (max_size > INBUFSZ) SinglError("INBUFSZ too small\n");
    if (procs_per_node > MAXLOCAL) SinglError("MAXLOCAL too small\n");
    if ((MPMY_Nproc()-1)/procs_per_node >= MAXREMOTE) SinglError("MAXREMOTE too small\n");
    polldone = localdone = remotedone = 0;
    localid = MPMY_Procnum() % procs_per_node;
    nremote = (MPMY_Nproc() + procs_per_node - 1) / procs_per_node;
    for (i = 0; i < procs_per_node; i++) localpoll[i] = 0;
    for (i = 0; i < nremote; i++) remotepoll[i] = 0;
    for (i = 0; i < MAXRELAY; i++) bufused[i] = 0;
    if (MPMY_Nproc() % procs_per_node) {
	if (MPMY_Procnum() == MPMY_Nproc() - 1) {
	    for (i = MPMY_Nproc() % procs_per_node; i < procs_per_node; i++) {
		localpoll[i] = 1;
		++localdone;
	    }
	}
    }

    /* In fact, this test is insufficient if somebody decides to send
       a short message anyway we'll still be confused! */
    if (max_size == sizeof(int) || max_size == 2*sizeof(int))
	SinglError("Poll uses size for message sorting.  You can't use size=%ld or %ld without some new coding\n", (long)sizeof(int), (long)2*sizeof(int));

    size = max_size;
    MPMY_Irecv(&inbuf, size, MPMY_SOURCE_ANY, tag, &inreq);
}

void
Poll(int tag)
{
    int flag;
    MPMY_Status stat;
    
    Msgf(("P(tag=%d)\n", tag));
    while (MPMY_Test(inreq, &flag, &stat), flag) {
	process(stat.count, tag);
	MPMY_Flick();
    }
}

/* If we use a plain MPMY_Wait() during a poll session, deadlock may */
/* result from isends blocking */
void
PollWait(MPMY_Comm_request req, int tag)
{
    int flag;
    MPMY_Status stat;
    
    Msgf(("PW(tag=%d)\n", tag));
    while (1) {
	if (MPMY_Test(req, &flag, 0), flag) return;
	if (MPMY_Test(inreq, &flag, &stat), flag) process(stat.count, tag);
	MPMY_Flick();
    }
}

void
PollUntilDone(int tag)
{
    MPMY_Comm_request req;
    MPMY_Status stat;
    int buf[2];
    int i;
    int procnum = MPMY_Procnum();
    int procs_per_node = MPMY_ProcsPerNode();

    Msg("polldone", ("PUD(tag=%d)\n", tag));
    StartTimer(&PollWaitTm);
    if (localid == 0) {
	/* I'm a group master */
	if (localpoll[0]) Error("localpoll[0] already set!\n");
	localpoll[0] = 1;
	++localdone;
	while (localdone != procs_per_node) { /* not right if Nproc() % procs_per_node */
	    MPMY_Wait(inreq, &stat);
	    process(stat.count, tag);
	}
	Msg("polldone", ("PollDone local group finished\n"));
	/* Tell masters our group is done */
	for (i = 0; i < nremote; i++) {
	    buf[0] = i*procs_per_node;
	    buf[1] = MAXLOCAL + MPMY_Procnum() / procs_per_node;
	    Msg("polldone", ("PollDone sent to remote master %d\n", i*procs_per_node));
	    MPMY_Isend(buf, 2*sizeof(int), i*procs_per_node, tag, &req);
	    PollWait(req, tag);
	}
	while (!polldone) {
	    MPMY_Wait(inreq, &stat);
	    process(stat.count, tag);
	    for (i = 0; i < MAXRELAY; i++) {
		int flag;
		if (bufused[i] == 1) {
		    if (MPMY_Test(Rreq[i], &flag, 0), flag) {
			bufused[i] = 0;
		    }
		}
	    }
	}
	/* Tell local group we are done */
	for (i = MPMY_Procnum()+1; i < MPMY_Procnum()+procs_per_node && i < MPMY_Nproc(); i++) {
	    buf[0] = i;
	    buf[1] = 0;
	    Msg("polldone", ("PollDone sent to %d\n", i));
	    MPMY_Isend(buf, 2*sizeof(int), i, tag, &req);
	    PollWait(req, tag);
	}
    } else {
	/* I'm a slave in the local group */
	int dest = (procnum / procs_per_node) * procs_per_node;
	buf[0] = dest;
	buf[1] = localid;	
	/* Tell local master we're done */
	Msg("polldone", ("PollDone sent to local master %d\n", dest));
	MPMY_Isend(buf, 2*sizeof(int), dest, tag, &req);
	PollWait(req, tag);
	while (!polldone) {
	    MPMY_Wait(inreq, &stat);
	    process(stat.count, tag);
	}
    }
    /* self send to clean up inreq */
    MPMY_Isend(buf, sizeof(int), MPMY_Procnum(), tag, &req);
    MPMY_Wait(req, 0);
    MPMY_Wait(inreq, 0);
    StopTimer(&PollWaitTm);
    Msgf(("PollDone\n"));
}
