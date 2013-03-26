/* batch.c:  Collect a series of small sends into larger ones */

#include "batch.h"
#include "mpmy.h"
#include "stk.h"
#include "Malloc.h"
#include "Msgs.h"

void PollWait(MPMY_Comm_request req, int tag);

static Stk **stks;
static int tag;
static int batch_size;

void
SetupBatch(int ttag, int size)
{
    int dest;
    
    Msgf(("SetupBatch: tag %d size %d\n", ttag, size));
    tag = ttag;
    batch_size = size;
    stks = Calloc(MPMY_Nproc(), sizeof(Stk *));
    /* allocate all memory beforehand */
    /* Otherwise the incoming poll buffer will fight with the batch stks */
    /* for heap space, and we end up with a bunch of holes in the heap */
    for (dest = 0; dest < MPMY_Nproc(); dest++) {
	stks[dest] = Malloc(sizeof(Stk));
	StkInit(stks[dest], batch_size, Realloc_f, 0);
	StkPushType(stks[dest], dest, int);
    }
}

void
FinishBatch(void)
{
    int i;
    MPMY_Comm_request req;
    int nproc = MPMY_Nproc();
    int procnum = MPMY_Procnum();
    int procs_per_node = MPMY_ProcsPerNode();
    int my_master = (procnum / procs_per_node) * procs_per_node;
    int ddest;

    for (i = 0; i < nproc; i++) {
	Stk *s = stks[i];
	if (StkSz(s) > sizeof(int)) {
	    ddest = (i / procs_per_node) * procs_per_node;
	    if (ddest == my_master) ddest = i;
	    Msgf(("SendBatch: %ld to %d via %d\n", StkSz(s), i, ddest));
	    MPMY_Isend(StkBase(s), StkSz(s), ddest, tag, &req);
	    PollWait(req, tag);
	}
	StkTerminate(s);
	Free(s);
    }
    Free(stks);
    Msgf(("FinishBatch done\n"));
}

void
SendBatch(void *outbuf, int size, int dest)
{
    Stk *s = stks[dest];

    StkPushData(s, outbuf, size);
    if (StkSz(s) > batch_size - size) {
	MPMY_Comm_request req;
	int ddest, my_master;
	int procs_per_node = MPMY_ProcsPerNode();
	int procnum = MPMY_Procnum();
	
	ddest = (dest / procs_per_node) * procs_per_node;
	my_master = (procnum / procs_per_node) * procs_per_node;
	if (ddest == my_master) ddest = dest;
	Msgf(("SendBatch: %ld to %d via %d\n", StkSz(s), dest, ddest));
	MPMY_Isend(StkBase(s), StkSz(s), ddest, tag, &req);
	PollWait(req, tag);
	StkClear(s);
	StkPushType(s, dest, int);
    }
}
