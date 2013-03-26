#include <string.h>
#include <sys/types.h>
#include "mpmy.h"
#include "Msgs.h"
#include "Malloc.h"
#include "gc.h"			/* for ilog2 */
#include "verify.h"

/* Could we implement gather using MPMY_Combine with MPMY_Op == MPMY_GATHER? */


#define BCAST_DEFAULT_TAG 0x47

#define GATHER_BCAST_TAG 0x1145
#define GATHER_TAG 0x2145
#define NGATHER_TAG 0x3145
#define ALLTOALL_TAG 0x4145
#define ALLTOALL_RTAG 0x4146
#define ALLTOALL_NTAG 0x4147

unsigned int MPMY_Datasize[] = 
{ sizeof(float), sizeof(double), sizeof(int), sizeof(char), sizeof(short),
  sizeof(long), sizeof(unsigned int), sizeof(unsigned char), 
  sizeof(unsigned short), sizeof(unsigned long), sizeof(off_t), sizeof(int64_t), 1/*user_data*/
};

void
MPMY_send(const void *buf, int cnt, int dest, int tag)
{
    MPMY_Comm_request req;

    MPMY_Isend(buf, cnt, dest, tag, &req);
    MPMY_Wait(req, 0);
    if( Msg_test(__FILE__)){
	int i;
	int sum = 0;
	const char *cbuf = buf;
	for(i=0; i<cnt; i++){
	    sum ^= cbuf[i];
	}
	Msg_do("mpmy_gather: send(cnt=%d, dest=%d, tag=%d), sum=%d\n", 
	       cnt, dest, tag, sum);
    }

}

void
MPMY_recvn(void *buf, int cnt, int src, int tag)
{
    MPMY_Status stat;
    MPMY_Comm_request req;

    Verify(MPMY_Irecv(buf, cnt, src, tag, &req)==MPMY_SUCCESS);
    Verify(MPMY_Wait(req, &stat)==MPMY_SUCCESS);
    if (MPMY_Count(&stat) != cnt) 
      Error("Recv failed, expected %d got %d\n", cnt, MPMY_Count(&stat));
    if( Msg_test(__FILE__)){
	int i;
	int sum = 0;
	char *cbuf = buf;
	for(i=0; i<cnt; i++){
	    sum ^= cbuf[i];
	}
	Msg_do("mpmy_gather: recvn(cnt=%d, dest=%d, tag=%d), sum=%d\n", 
	       cnt, src, tag, sum);
    }
}

int
MPMY_AllGather(const void *sendbuf, int count, MPMY_Datatype type, 
	       void *recvbuf)
{
    int chan;
    int doc;
    MPMY_Status stat;
    int sendproc;
    int bufsz;
    unsigned int mask;
    int ret;
    void *inptr;
    const void *outptr;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();
    int nbytes = MPMY_Datasize[type] * count;

    doc = ilog2(nproc);
    if (nproc != 1 << doc) {	/* for non power-of-two sizes */
	MPMY_Gather(sendbuf, count, type, recvbuf, 0);
	MPMY_BcastTag(recvbuf, nproc*count, type, 0, GATHER_BCAST_TAG);
	return MPMY_SUCCESS;
    }

    inptr = (char *)recvbuf + procnum * nbytes;
    outptr = sendbuf;

    /* Self contribution */
    if (inptr != outptr)
      memcpy(inptr, outptr, nbytes);

    mask = ~0;
    for (chan = 0; chan < doc; chan++) {
	sendproc = procnum ^ (1 << chan);
	bufsz = (1 << chan) * nbytes;
	inptr = (char *)recvbuf + (sendproc & mask) * nbytes;
	outptr = (char *)recvbuf + (procnum & mask) * nbytes;
	MPMY_Shift(sendproc, inptr, bufsz, outptr, bufsz, &stat);
	ret = MPMY_Count(&stat);
	if (ret != bufsz) 
	  Error("Shift failed, expected %d got %d\n", bufsz, ret);
	mask <<= 1;
    }
    return MPMY_SUCCESS;
}

int
MPMY_Gather(const void *sendbuf, int count, MPMY_Datatype type, void *recvbuf,
	    int recvproc)
{
    int chan;
    int doc;
    int sendproc;
    int inbufsz, outbufsz;
    int nin, nout;
    unsigned int mask;
    void *inptr;
    const void *outptr;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();
    int nbytes = MPMY_Datasize[type] * count;

    if (recvproc != 0) Error("Gather to procnum != 0 not supported yet.\n");

    doc = ilog2(nproc);
    if (nproc != 1 << doc)
      doc++;			/* for non power-of-two sizes */

    if (procnum != recvproc)
      recvbuf = Malloc(nproc*nbytes); /* overkill */

    inptr = (char *)recvbuf + procnum * nbytes;
    outptr = sendbuf;
    if (inptr != outptr) memcpy(inptr, outptr, nbytes);

    mask = ~0;
    for (chan = 0; chan < doc; chan++) {
	sendproc = procnum ^ (1 << chan);
	if (sendproc >= 0 && sendproc < nproc) {
	    nin = nout = (1 << chan);
	    if (procnum & (1 << chan)) {
		if (nproc - procnum < nout) nout = nproc - procnum;
		outbufsz = nout * nbytes;
		outptr = (char *)recvbuf + (procnum & mask) * nbytes;
		Msgf(("Gather: to %d, outidx %d, outsz %d\n", 
		       sendproc, (procnum & mask), nout));
		MPMY_send(outptr, outbufsz, sendproc, GATHER_TAG+chan);
		break;
	    } else {
		if (nproc - sendproc < nin) nin = nproc - sendproc;
		inbufsz = nin * nbytes;
		inptr = (char *)recvbuf + (sendproc & mask) * nbytes;
		Msgf(("Gather: from %d, inidx %d, insz %d\n", 
		       sendproc, (sendproc & mask), nin));
		MPMY_recvn(inptr, inbufsz, sendproc, GATHER_TAG+chan);
	    }
	}
	mask <<= 1;
    }
    if (procnum != recvproc)
      Free(recvbuf);
    return MPMY_SUCCESS;
}

/* "count" can vary for each processor in NGather */

int
MPMY_NGather(const void *sendbuf, int count, MPMY_Datatype type, 
	     void **recvhndl, int recvproc)
{
    int chan;
    int doc;
    int sendproc;
    int bufsz;
    int inbytes;
    unsigned int mask;
    void *buf;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();
    int nbytes = MPMY_Datasize[type] * count;

    if (recvproc != 0) Error("NGather to procnum != 0 not supported yet.\n");

    doc = ilog2(nproc);
    if (nproc != 1 << doc)
      doc++;			/* for non power-of-two sizes */

    buf = Malloc(nbytes); 
    memcpy(buf, sendbuf, nbytes);
    bufsz = nbytes;

    mask = ~0;
    for (chan = 0; chan < doc; chan++) {
	sendproc = procnum ^ (1 << chan);
	if (sendproc >= 0 && sendproc < nproc) {
	    if (procnum & (1 << chan)) {
		Msgf(("NGather: to %d, bufsz %d\n", sendproc, bufsz));
		MPMY_send(&bufsz, sizeof(int), sendproc, NGATHER_TAG+chan);
		MPMY_send(buf, bufsz, sendproc, NGATHER_TAG+chan);
		break;
	    } else {
		MPMY_recvn(&inbytes, sizeof(int), sendproc, NGATHER_TAG+chan);
		Msgf(("NGather: from %d, inbytes %d\n", sendproc, inbytes));
		buf = Realloc(buf, bufsz+inbytes);
		MPMY_recvn(bufsz+(char*)buf, inbytes, sendproc, NGATHER_TAG+chan);
		bufsz += inbytes;
	    }
	}
	mask <<= 1;
    }
    if (procnum != recvproc) {
	Free(buf);
	return 0;
    } else {
	Msgf(("NGather: Final bufsz %d\n", bufsz));
	*recvhndl = buf;
	return bufsz/MPMY_Datasize[type];
    }
}

int MPMY_Bcast(void *buf, int count, MPMY_Datatype type, int srcproc)
{
    return MPMY_BcastTag(buf, count, type, srcproc, BCAST_DEFAULT_TAG);
}

int
MPMY_BcastTag(void *buf, int count, MPMY_Datatype type, int srcproc, int tag)
{
    int chan;
    int doc;
    int sendproc;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();
    int nbytes = MPMY_Datasize[type] * count;

    if (srcproc != 0) Error("Bcast from procnum != 0 not supported yet.\n");

    Msgf(("MPMYBcast(buf=%p, count=%d, type=%d, srcproc=%d\n",
	  buf, count, type, srcproc)); Msg_flush();
    doc = ilog2(nproc);
    if (nproc != 1 << doc)
      doc++;			/* for non power-of-two sizes */

    for (chan = 0; chan < doc; chan++) {
	sendproc = procnum ^ (1 << chan);
	if (sendproc >= 0 && sendproc < nproc) {
	    if (procnum & (1 << chan)) {
		Msgf(("Bcast: recv from %d\n", sendproc));
		MPMY_recvn(buf, nbytes, sendproc, tag+chan);
	    } else {
		Msgf(("Bcast: send to %d\n", sendproc));
		MPMY_send(buf, nbytes, sendproc, tag+chan);
	    }
	}
    }
    return MPMY_SUCCESS;
}


int
MPMY_Alltoall(void *sendbuf, int sendcount, MPMY_Datatype sendtype, 
	      void *recvbuf, int recvcount, MPMY_Datatype recvtype)
{
    MPMY_Status stat;
    int i;
    int ret;
    int doc_procs, doc_nodes, relative, dest, proc;
    int sendbytes = MPMY_Datasize[sendtype] * sendcount;
    int recvbytes = MPMY_Datasize[recvtype] * recvcount;
    int my_node = MPMY_Procnum() / PROCS_PER_NODE;
    int my_leader = my_node * PROCS_PER_NODE;
    int local_rank = MPMY_Procnum() % PROCS_PER_NODE;
    int dest_leader;
    char *tmpbuf_s = NULL;
    char *tmpbuf_r = NULL;
    MPMY_Comm_request req;

    doc_nodes = ilog2(MPMY_Nproc()/PROCS_PER_NODE-1) + 1;
    doc_procs = ilog2(PROCS_PER_NODE-1) + 1;
    Msgf(("Alltoall my_leader %d, local_rank %d, doc_nodes %d, doc_procs %d\n", 
	  my_leader, local_rank, doc_nodes, doc_procs));
    /* Within a node */
    for (relative = 0; relative < 1 << doc_procs; relative++) {
	dest = my_leader + (relative ^ local_rank);
	if ((dest >= MPMY_Nproc()) || (dest >= my_leader + PROCS_PER_NODE))
	    continue;
	Msgf(("local Alltoall MPMY_Shift(dest=%d, instart=%p, incnt=%d, outstart=%p, outcnt=%d)\n",
	      dest, recvbuf + dest * recvbytes, recvbytes,
	      sendbuf + dest * sendbytes, sendbytes));
	MPMY_Shift(dest, recvbuf + dest * recvbytes, recvbytes,
		   sendbuf + dest * sendbytes, sendbytes, &stat);
	ret = MPMY_Count(&stat);
	if (ret != recvbytes)
	    Error("Shift failed, expected %d got %d\n", recvbytes, ret);
    }
    if (MPMY_Procnum() == my_leader) {
	tmpbuf_s = Malloc(sendbytes*PROCS_PER_NODE);
	tmpbuf_r = Malloc(recvbytes*PROCS_PER_NODE);
    }
    /* Across nodes */
    for (relative = 1; relative < 1 << doc_nodes; relative++) {
	for (proc = 0; proc < PROCS_PER_NODE; proc++) {
	    dest_leader = (relative ^ my_node) * PROCS_PER_NODE;
	    dest = dest_leader + proc;
	    if (dest >= MPMY_Nproc())
		continue;
	    if (MPMY_Procnum() != my_leader) {
		MPMY_Isend(sendbuf + dest * sendbytes, sendbytes, my_leader, ALLTOALL_RTAG, &req);
		MPMY_Wait(req, 0);
		if (MPMY_Procnum() == my_leader + proc) {
		    Msgf(("Alltoall relay recv from %d (source=%d, incnt=%d)\n", 
			  my_leader, dest_leader, PROCS_PER_NODE * recvbytes));
		    MPMY_Irecv(recvbuf + dest_leader * recvbytes, PROCS_PER_NODE * recvbytes, 
			       my_leader, ALLTOALL_TAG, &req);
		    MPMY_Wait(req, &stat);
		    ret = MPMY_Count(&stat);
		    if (ret != PROCS_PER_NODE * recvbytes) {
			Error("Irecv failed, expected %d got %d\n", PROCS_PER_NODE * recvbytes, ret);
		    }
		}
	    } else {
		memcpy(tmpbuf_s, sendbuf + dest * sendbytes, sendbytes);
		for (i = 1; i < PROCS_PER_NODE; i++) {
		    if (my_leader + i >= MPMY_Nproc()) break;
		    MPMY_Irecv(tmpbuf_s + i * recvbytes, recvbytes, my_leader+i, ALLTOALL_RTAG, &req);
		    MPMY_Wait(req, &stat);
		    ret = MPMY_Count(&stat);
		    if (ret != recvbytes) {
			Error("Irecv failed, expected %d got %d\n", recvbytes, ret);
		    }
		}
		Msgf(("Alltoall relay for %d MPMY_Shift(dest=%d, incnt=%d, outcnt=%d)\n",
		      dest, dest_leader, PROCS_PER_NODE * recvbytes, PROCS_PER_NODE * sendbytes));
		MPMY_Shift(dest_leader, tmpbuf_r, PROCS_PER_NODE * recvbytes,
			   tmpbuf_s, PROCS_PER_NODE * sendbytes, &stat);
		ret = MPMY_Count(&stat);
		if (ret != PROCS_PER_NODE * recvbytes)
		    Error("Shift failed, expected %d got %d\n", PROCS_PER_NODE * recvbytes, ret);
		if (proc == 0) {
		    Msgf(("Alltoall local copy (source=%d, incnt=%d)\n", 
			  dest_leader, PROCS_PER_NODE * recvbytes));
		    memcpy(recvbuf + dest_leader * recvbytes, tmpbuf_r, PROCS_PER_NODE * recvbytes);
		} else {
		    Msgf(("Alltoall relay send to %d (source=%d, incnt=%d)\n", 
			  my_leader+proc, dest_leader, PROCS_PER_NODE * recvbytes));
		    MPMY_Isend(tmpbuf_r, PROCS_PER_NODE * recvbytes, my_leader+proc, ALLTOALL_TAG, &req);
		    MPMY_Wait(req, 0);
		}
	    }
	}
    }
    if (MPMY_Procnum() == my_leader) {
	Free(tmpbuf_r);
	Free(tmpbuf_s);
    }
    return MPMY_SUCCESS;
}

int
MPMY_Alltoallv(void *sendbuf, int *scount, int *soff, MPMY_Datatype sendtype, 
	       void *recvbuf, int *rcount, int *roff, MPMY_Datatype recvtype)
{
    MPMY_Status stat;
    int i;
    int ret;
    int doc_procs, doc_nodes, relative, dest, proc;
    int ssize = MPMY_Datasize[sendtype];
    int rsize = MPMY_Datasize[recvtype];
    int my_node = MPMY_Procnum() / PROCS_PER_NODE;
    int my_leader = my_node * PROCS_PER_NODE;
    int local_rank = MPMY_Procnum() % PROCS_PER_NODE;
    int dest_leader;
    char *tmpbuf_s = NULL;
    char *tmpbuf_r = NULL;
    int *stcnt = NULL;
    int *stoff = NULL;
    MPMY_Comm_request req;
    int sendbytes, recvbytes;

    doc_nodes = ilog2(MPMY_Nproc()/PROCS_PER_NODE-1) + 1;
    doc_procs = ilog2(PROCS_PER_NODE-1) + 1;
    if (MPMY_Procnum() == my_leader) {
	stcnt = Calloc(PROCS_PER_NODE, sizeof(int));
	stoff = Calloc(PROCS_PER_NODE, sizeof(int));
    }
    Msgf(("Alltoallv my_leader %d, local_rank %d, doc_nodes %d, doc_procs %d\n", 
	  my_leader, local_rank, doc_nodes, doc_procs));
    /* Within a node */
    for (relative = 0; relative < 1 << doc_procs; relative++) {
	dest = my_leader + (relative ^ local_rank);
	if ((dest >= MPMY_Nproc()) || (dest >= my_leader + PROCS_PER_NODE))
	    continue;
	if ((rcount[dest] == 0) && (scount[dest] == 0))
	    continue;
	Msgf(("local Alltoallv MPMY_Shift(dest=%d, instart=%p, incnt=%d, outstart=%p, outcnt=%d)\n",
	      dest, recvbuf + roff[dest] * rsize, rcount[dest] * rsize,
	      sendbuf + soff[dest] * ssize, scount[dest] * ssize));
	MPMY_Shift(dest, recvbuf + roff[dest] * rsize, rcount[dest] * rsize,
		   sendbuf + soff[dest] * ssize, scount[dest] * ssize, &stat);
	ret = MPMY_Count(&stat);
	if (ret != rcount[dest] * rsize)
	    Error("Shift failed, expected %d got %d\n", rcount[dest] * rsize, ret);
    }
    /* Across nodes */
    for (relative = 1; relative < 1 << doc_nodes; relative++) {
	for (proc = 0; proc < PROCS_PER_NODE; proc++) {
	    dest_leader = (relative ^ my_node) * PROCS_PER_NODE;
	    dest = dest_leader + proc;
	    if (dest >= MPMY_Nproc())
		continue;
	    if (MPMY_Procnum() != my_leader) {
		MPMY_Isend(&scount[dest], sizeof(int), my_leader, ALLTOALL_NTAG, &req);
		MPMY_Wait(req, 0);
		if (MPMY_Procnum() == my_leader + proc) {
		    for (recvbytes = 0, i = dest_leader; i < dest_leader + PROCS_PER_NODE; i++) {
			if (i >= MPMY_Nproc()) break;
			recvbytes += rcount[i] * rsize;
		    }
		    MPMY_Isend(&recvbytes, sizeof(int), my_leader, ALLTOALL_NTAG, &req);
		    MPMY_Wait(req, 0);
		}
		if (scount[dest]) {
		    MPMY_Isend(sendbuf + soff[dest] * ssize, scount[dest] * ssize, my_leader, 
			       ALLTOALL_RTAG, &req);
		    MPMY_Wait(req, 0);
		}
		if ((MPMY_Procnum() == my_leader + proc) && recvbytes) {
		    Msgf(("Alltoallv relay recv from %d (source=%d, incnt=%d)\n", 
			  my_leader, dest_leader, recvbytes));
		    MPMY_Irecv(recvbuf + roff[dest_leader] * rsize, recvbytes, 
			       my_leader, ALLTOALL_TAG, &req);
		    MPMY_Wait(req, &stat);
		    ret = MPMY_Count(&stat);
		    if (ret != recvbytes) {
			Error("Irecv failed, expected %d got %d\n", recvbytes, ret);
		    }
		}
	    } else {
		stcnt[0] = scount[dest];
		stoff[0] = 0;
		sendbytes = stcnt[0] * ssize;
		for (i = 1; i < PROCS_PER_NODE; i++) {
		    if (my_leader + i >= MPMY_Nproc()) break;
		    MPMY_Irecv(stcnt+i, sizeof(int), my_leader+i, ALLTOALL_NTAG, &req);
		    MPMY_Wait(req, &stat);
		    ret = MPMY_Count(&stat);
		    if (ret != sizeof(int)) {
			Error("Irecv failed, expected %ld got %d\n", sizeof(int), ret);
		    }
		    stoff[i] = stoff[i-1] + stcnt[i-1];
		    sendbytes += stcnt[i] * ssize;
		}
		if (proc == 0) {
		    for (recvbytes = 0, i = dest_leader; i < dest_leader + PROCS_PER_NODE; i++) {
			if (i >= MPMY_Nproc()) break;
			recvbytes += rcount[i] * rsize;
		    }
		} else if (my_leader+proc < MPMY_Nproc()) {
		    MPMY_Irecv(&recvbytes, sizeof(int), my_leader+proc, ALLTOALL_NTAG, &req);
		    MPMY_Wait(req, &stat);
		    ret = MPMY_Count(&stat);
		    if (ret != sizeof(int)) {
			Error("Irecv failed, expected %ld got %d\n", sizeof(int), ret);
		    }
		}
		if (sendbytes || recvbytes) {
		    Msgf(("allocating %d bytes for send\n", sendbytes));
		    tmpbuf_s = Malloc(sendbytes);
		    memcpy(tmpbuf_s, sendbuf + soff[dest] * ssize, scount[dest] * ssize);
		    for (i = 1; i < PROCS_PER_NODE; i++) {
			if (my_leader + i >= MPMY_Nproc()) break;
			if (stcnt[i]) {
			    MPMY_Irecv(tmpbuf_s + stoff[i] * ssize, stcnt[i] * ssize, 
				       my_leader+i, ALLTOALL_RTAG, &req);
			    MPMY_Wait(req, &stat);
			    ret = MPMY_Count(&stat);
			    if (ret != stcnt[i] * ssize) {
				Error("Irecv failed, expected %d got %d\n", stcnt[i] * ssize, ret);
			    }
			}
		    }
		    Msgf(("allocating %d bytes for recv\n", recvbytes));
		    Msgf(("Alltoallv relay for %d MPMY_Shift(dest=%d, incnt=%d, outcnt=%d)\n",
			  dest, dest_leader, recvbytes, sendbytes));
		    tmpbuf_r = Malloc(recvbytes);
		    if (sendbytes || recvbytes) {
			MPMY_Shift(dest_leader, tmpbuf_r, recvbytes, tmpbuf_s, sendbytes, &stat);
			ret = MPMY_Count(&stat);
			if (ret != recvbytes)
			    Error("Shift failed, expected %d got %d\n", recvbytes, ret);
		    }
		    Free(tmpbuf_s);
		    if (recvbytes) {
			if (proc == 0) {
			    Msgf(("Alltoallv local copy (source=%d, incnt=%d)\n", 
				  dest_leader, recvbytes));
			    memcpy(recvbuf + roff[dest_leader] * rsize, tmpbuf_r, recvbytes);
			} else if (my_leader+proc < MPMY_Nproc()) {
			    Msgf(("Alltoallv relay send to %d (source=%d, incnt=%d)\n", 
				  my_leader+proc, dest_leader, recvbytes));
			    MPMY_Isend(tmpbuf_r, recvbytes, my_leader+proc, ALLTOALL_TAG, &req);
			    MPMY_Wait(req, 0);
			}
		    }
		    Free(tmpbuf_r);
		}
	    }
	}
    }
    if (MPMY_Procnum() == my_leader) {
	Free(stcnt);
	Free(stoff);
    }
    return MPMY_SUCCESS;
}

int
MPMY_Alltoallv_simple(void *sendbuf, int *scount, int *soff, MPMY_Datatype sendtype, 
	       void *recvbuf, int *rcount, int *roff, MPMY_Datatype recvtype)
{
    MPMY_Status stat;
    int ret;
    int doc, relative, i;
    int ssize = MPMY_Datasize[sendtype];
    int rsize = MPMY_Datasize[recvtype];

    doc = ilog2(MPMY_Nproc()-1) + 1;
    i = MPMY_Procnum();
    if (scount[i]) {
	memcpy(recvbuf + roff[i] * rsize, sendbuf + soff[i] * ssize, scount[i] * ssize);
    }
    for (relative = 1; relative < 1<<doc; relative++) {
	i = relative ^ MPMY_Procnum();
	if (i >= MPMY_Nproc())
	    continue;
	if ((scount[i] == 0) && (rcount[i] == 0)) 
	    continue;
	Msgf(("Alltoallv MPMY_Shift(dest=%d, instart=%p, incnt=%d, outstart=%p, outcnt=%d)\n",
	      i, recvbuf + roff[i] * rsize, rcount[i] * rsize,
	      sendbuf + soff[i] * ssize, scount[i] * ssize));
	MPMY_Shift(i, recvbuf + roff[i] * rsize, rcount[i] * rsize,
		   sendbuf + soff[i] * ssize, scount[i] * ssize, &stat);
	ret = MPMY_Count(&stat);
	if (ret != rcount[i] * rsize)
	    Error("Shift failed, expected %d got %d\n", rcount[i] * rsize, ret);
    }
    return MPMY_SUCCESS;
}
