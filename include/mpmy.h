#ifndef _MpMYdotH
#define _MpMYdotH

#include "timers.h"

typedef void *MPMY_Comm_request;
typedef struct {
    int tag;
    int src;
    int count;
}MPMY_Status;

#define MPMY_SUCCESS (0)
#define MPMY_FAILED (1)
/* This corresponds to the ANY-source in udp.c. */
/* What about other PAROS??? */
#define MPMY_SOURCE_ANY -2
#define MPMY_TAG_ANY -1

/* Data types and operations supported in MPMY_Combine */

typedef enum {
    MPMY_SUM, MPMY_PROD, MPMY_MAX, MPMY_MIN, MPMY_BAND, MPMY_BOR, MPMY_BXOR
} MPMY_Op;

typedef void (*MPMY_user_comb_func)(const void *from1, const void *from2, 
				    void *to);

typedef enum {
    MPMY_FLOAT, MPMY_DOUBLE, MPMY_INT, MPMY_CHAR, MPMY_SHORT, MPMY_LONG, 
    MPMY_UNSIGNED_INT, MPMY_UNSIGNED_CHAR, MPMY_UNSIGNED_SHORT, 
    MPMY_UNSIGNED_LONG, MPMY_OFFT, MPMY_INT64, MPMY_USER_DATA
} MPMY_Datatype;

extern unsigned int MPMY_Datasize[];

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
/* Reduction prototypes */
int MPMY_Combine(const void *sendbuf, void *recvbuf, const int count, 
		 const MPMY_Datatype datatype, const MPMY_Op op);
int MPMY_ICombine_Init(MPMY_Comm_request *reqp);
int MPMY_ICombine_Wait(MPMY_Comm_request req);
int MPMY_ICombine(const void *sendbuf, void *recvbuf, int count, 
		  MPMY_Datatype datatype, MPMY_Op op, 
		  MPMY_Comm_request req);
/* A separate entry point for the user-specified-function version */
int MPMY_ICombine_func(const void *sendbuf, void *recvbuf, int size,
		       MPMY_user_comb_func func,
		       MPMY_Comm_request req);
int MPMY_AllGather(const void *sndbuf, int count, MPMY_Datatype type, 
		   void *rcvbuf);
int MPMY_Gather(const void *sendbuf, int count, MPMY_Datatype type, 
		void *recvbuf, int recvproc);
int MPMY_NGather(const void *sendbuf, int count, MPMY_Datatype type, 
		 void **recvhndl, int recvproc);
int MPMY_Bcast(void *buf, int count, MPMY_Datatype type, int sendproc);
int MPMY_BcastTag(void *buf, int count, MPMY_Datatype type, int sendproc, int Tag0);
int MPMY_Alltoall(void *sendbuf, int sendcount, MPMY_Datatype sendtype, 
		  void *recvbuf, int recvcount, MPMY_Datatype recvtype);
int MPMY_Alltoallv(void *sendbuf, int *sendcounts, int *sendoffsets, MPMY_Datatype sendtype, 
		   void *recvbuf, int *recvcounts, int *recvoffsets, MPMY_Datatype recvtype);
int Native_MPMY_Allgather(void *sendbuf, int sendcount, MPMY_Datatype type, void *recvbuf);
int Native_MPMY_Allgatherv(void *sendbuf, int sendcount, MPMY_Datatype type, void *recvbuf,
			   int *rcounts, int *roffsets);
int Native_MPMY_Alltoall(void *sendbuf, int sendcount, MPMY_Datatype sendtype, 
			 void *recvbuf, int recvcount, MPMY_Datatype recvtype);
int Native_MPMY_Alltoallv(void *sendbuf, int *sendcounts, int *sendoffsets, MPMY_Datatype sendtype, 
			  void *recvbuf, int *recvcounts, int *recvoffsets, MPMY_Datatype recvtype);

/*
   A NULL stat argument is allowed, indicating that you aren't interested in
   the status.
*/
int MPMY_Init(int *argcp, char ***argvp);
int MPMY_Isend(const void *buf, int cnt, int dest, int tag, MPMY_Comm_request *req);
int MPMY_Irsend(const void *buf, int cnt, int dest, int tag, MPMY_Comm_request *req);
int MPMY_Irecv(void *buf, int cnt, int src, int tag, MPMY_Comm_request *req);
int MPMY_Test(MPMY_Comm_request request, int *flag, MPMY_Status *stat);
int MPMY_Wait(MPMY_Comm_request request, MPMY_Status *stat);
/* I don't know how to write the general WaitN, but we seem to use Wait2
   often enough that it's worth providing in the library.  Note that this
   waits for BOTH.  Not EITHER.  */
int MPMY_Wait2(MPMY_Comm_request req1, MPMY_Status *stat1,
	      MPMY_Comm_request req2, MPMY_Status *stat2);
/* send with wait */
void MPMY_send(const void *buf, int cnt, int dest, int tag);
/* Blocking recv of exactly cnt bytes */
void MPMY_recvn(void *buf, int cnt, int src, int tag);
int MPMY_Finalize(void);

/* These are occasionally useful and seem to be highly system-dependent */
int MPMY_Sync(void);
int MPMY_Flick(void);

/* Desperate times require desperate measures... (c.f. malloc_print)
   Consider using Msg_do or Shout as the argument.  Note that, despite
   the name, they aren't strictly printf-identical because they don't
   return an int.  C'est la vie. */
void MPMY_Diagnostic(int (*printflike)(const char *, ...));

/* And a version suitable for passing to OnAbnormal */
void PrintMPMYDiags(void);

/* These don't really have analogues in mpi.  MPI does have Sendrecv
   and Sendrecv_replace, but those are both more general (allowing
   different sources and destinations, allowing tags, allowing *_ANY)
   and less general ('replace' instead of 'overlap'). 

*/
int MPMY_Shift(int proc, void *recvbuf, int recvcnt, 
	       const void *sendbuf, int sendcnt, MPMY_Status *stat);
	       
int MPMY_Shift_overlap(int proc, void *recvbuf, int recvcnt,
		       const void *sendbuf, int sendcnt,  MPMY_Status *stat);


/* In MPI, they actually give you the name of the field element.  For backward
   compatibility, I'll also give them as macros */
#define MPMY_SOURCE src
#define MPMY_TAG tag
#define MPMY_Source(stat) ((stat)->src)
#define MPMY_Tag(stat) ((stat)->tag)
/* In MPI, this is still a function because it has to deal with typing.
   We don't worry about typing... */
#define MPMY_COUNT count
#define MPMY_Count(stat) ((stat)->count)

int MPMY_Nproc(void);
int MPMY_Procnum(void);
/* Returns a pointer to a static char string describing the phys node. */
const char *MPMY_Physnode(void);
/* We call these an awful lot.  Let's just set them up in init and
   save a function-call */
extern int _MPMY_nproc_;
extern int _MPMY_procnum_;
extern int _MPMY_procs_per_node_;
#define MPMY_Nproc() (_MPMY_nproc_)
#define MPMY_Procnum() (_MPMY_procnum_)
#define MPMY_ProcsPerNode() (_MPMY_procs_per_node_)

/* How can a "subsystem" like SDF know if MPMY has been initialized? */
extern int MPMY_Initialized(void);
extern int _MPMY_initialized_;	/* internal use only! */

/* Counters for the number of Isends, Irecvs and (successful Tests + Waits) */
extern Counter_t MPMYSendCnt;
extern Counter_t MPMYRecvCnt;
extern Counter_t MPMYDoneCnt;

void MPMY_CheckpointSetup(int job_seconds, int interval_seconds, int step_seconds);
int MPMY_CheckpointDue(int next_output_seconds);
void MPMY_CheckpointFinished(void);
int MPMY_JobDone(void);
int MPMY_JobRemaining(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* INTERNAL USE ONLY!! */

#endif
