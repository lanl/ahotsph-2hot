#include "swampi.h"
#include "Msgs.h"
#include "gc.h"			/* for ilog2 */
#include "error.h"

#define TAG 4

int
MPI_Bcast(void *buf, int count, MPI_Datatype type, int srcproc, MPI_Comm comm)
{
    int chan;
    int doc;
    int sendproc;
    int ret;
    MPI_Request rreq, sreq;
    MPI_Status status;
    int procnum = _MPI_Procnum;
    int nproc = _MPI_Nproc;
    int nbytes = _MPI_Datasize[type] * count;

    Msgf(("mpi: Bcast\n"));
    if (srcproc != 0) {		/* Is this stupid? */
	if (_MPI_Procnum == 0) {
	    MPI_Irecv(buf, nbytes, MPI_BYTE, srcproc, TAG, MPI_COMM_PRIVATE, 
		      &rreq);
	    MPI_Wait(&rreq, &status);
	    MPI_Get_count(&status, MPI_BYTE, &ret);
	    if (ret != nbytes) Error("Bcast got wrong len\n");
	} else if (_MPI_Procnum == srcproc) {
	    MPI_Isend(buf, nbytes, MPI_BYTE, 0, TAG, MPI_COMM_PRIVATE, &sreq);
	    MPI_Wait(&sreq, 0);
	}
    }

    doc = ilog2(nproc);
    if (nproc != 1 << doc)
      doc++;			/* for non power-of-two sizes */

    for (chan = 0; chan < doc; chan++) {
	sendproc = procnum ^ (1 << chan);
	if (sendproc >= 0 && sendproc < nproc) {
	    if (procnum & (1 << chan)) {
		MPI_Irecv(buf, nbytes, MPI_BYTE, sendproc, TAG, 
			  MPI_COMM_PRIVATE, &rreq);
		MPI_Wait(&rreq, &status);
		MPI_Get_count(&status, MPI_BYTE, &ret);
		if (ret != nbytes) Error("Bcast got wrong len\n");
	    } else {
		MPI_Isend(buf, nbytes, MPI_BYTE, sendproc, TAG, 
			  MPI_COMM_PRIVATE, &sreq);
		MPI_Wait(&sreq, 0);
	    }
	}
    }
    return MPI_SUCCESS;
}
