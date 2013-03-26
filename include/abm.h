#ifndef ABM_dot_H
#define ABM_dot_H

#include "chn.h"
#include "dll.h"
#include "mpmy.h"
#include "timers.h"

/* Notice that memcpy is a perfectly good ABMpktz_t.  Overzealous
 compilers will complain because arg2 isn't const and arg3 is an int
 rather than a size_t.  AAAARRRRGGGGHHHH....  */
typedef void (ABMpktz_t)(void *to, void *arg, int sz);
typedef void (ABMhndlr_t)(int src, int len, void *ptr);

typedef struct {
    int nfuncs;
    ABMhndlr_t **hndlarray;
    Dll undeliveredLL;  /* An LL of all messages that have been ISent, but
			   not Test'ed affirmative. */
    int done;			/* I hate these! */
    int doc;
    int allbitsdone;
    int alldone;   
    Dll *Enqueued;		/* array of DLL's, one for each dest */
    int *destarr;		/* which of Enqueued are non-empty? */
    int ndests;			/* how many of Enqueued are non-empty? */
    int *cntarr;		/* how much data for each dest? */
    Chn undelChn;		/* chain for undelivereLL */
    Chn QelmtChn;		/* chain for all of the Enqueued Dll's */
    MPMY_Comm_request Recv_Hndl;
    int tag;
    int pktsize;
    char *recvbuf1;
    char *recvbuf2;
    char *recvbufA;
    char *recvbufB;
} ABM ;

#ifdef __cplusplus
extern "C"{
#endif
/* Set the whole thing up.  State goes into abm */
void ABMSetup(ABM *abm, int pktsize, int tag, int nfuncs, ABMhndlr_t *hndlarray[]);

/* Post a message of given size.  When it's time to deliver it,
   the packetizing func will be called-back with the given arg.
   When it arrives, the 'handler' hndlarray[type] on the dest node
   will be called to process it.  */
void ABMPost(ABM *abm, int dest, int sz, int type, ABMpktz_t *func, void *arg);

/* Poll for incoming messages.  Handlers get called under here.  */
int ABMPoll(ABM *abm);

/* Poll for incoming messages.  But wait until something arrives. */
int ABMPollWait(ABM *abm);

/* Flush any Posted messages to dest.  Packetizers get called under here.  (but
   this may be called by ABMPost if we run out of space) */
void ABMFlush(ABM *abm);

/* Assert that we won't be sending out any more 'requests' AND that they,
   along with any 'cascades' that they may have generated have been received.
   This is automatic with a request/reply type protocol, but requires some
   kind of ack if messages do not generate a reply to the originator.
   See pqsort.c for one way to do the acks. */
void ABMIamDone(ABM *abm);

/* Return true if everybody has called ABMIamDone */
int ABMAllDone(ABM *abm);

/* Free all memory.  Etc. */
void ABMShutdown(ABM *abm);

/* Maintain a bunch of 'informative' Counter_t's.  They record the number
   of bytes sent in messages of logarithmically binned lenths between
   lo and hi. */
void ABMHistEnable(int log2lo, int log2hi);

#define ABMHISTFIRST 3		/* don't bother with the hist below 8 bytes */
#define ABMHISTLEN 16
extern Counter_t ABMIsendCnt;	/* How many 'buffers' did we actualy Isend. */
extern Counter_t ABMPostCnt;	/* How many 'messages' did we Post. */
extern Counter_t ABMByteCnt;	/* How many bytes were Isent. */
extern Counter_t ABMHistCnt[ABMHISTLEN];

#ifdef __cplusplus
}
#endif


#endif
