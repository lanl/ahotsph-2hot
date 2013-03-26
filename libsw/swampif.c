void _F77(mpi_init)(int *ierr)
{
    *ierr = MPI_Init(0, 0);
}

void _F77(mpi_finalize)(int *ierr)
{
    *ierr = MPI_Finalize();
}

void _F77(mpi_abort)(int *comm, int *err, int *ierr)
{
    *ierr = MPI_Abort(*comm, *err);
}

void _F77(mpi_comm_rank)(int *comm, int *rank, int *ierr)
{
    *ierr = MPI_Comm_rank(*comm, rank);
}

void _F77(mpi_comm_size)(int *comm, int *size, int *ierr)
{
    *ierr = MPI_Comm_size(*comm, size);
}

void _F77(mpi_get_count)(MPI_Status *status, int *type, int *cnt, int *ierr)
{
    *ierr = MPI_Get_count(status, *type, cnt);
}

void _F77(mpi_isend)(void *buf, int *cnt, int *type, int *dest, int *tag,
		     int *comm, MPI_Request req, int *ierr)
{
    *ierr = MPI_Isend(buf, *cnt, *type, *dest, *tag, *comm, req);
}

void _F77(mpi_irecv)(void *buf, int *cnt, int *type, int *src, int *tag,
		     int *comm, MPI_Request req, int *ierr)
{
    *ierr = MPI_Irecv(buf, *cnt, *type, *src, *tag, *comm, req);
}

void _F77(mpi_test)(MPI_Request req, int *flag, MPI_Status *stat, int *ierr)
{
    *ierr = MPI_Test(req, flag, stat);
}

void _F77(mpi_wait)(MPI_Request req, MPI_Status *status, int *ierr)
{
    *ierr = MPI_Wait(req, status);
}

void _F77(mpi_waitall)(int *count, MPI_Request *reqv, 
		       MPI_Status *statusv, int *ierr)
{
    *ierr = MPI_Waitall(*count, reqv, statusv);
}

void _F77(mpi_send)(void *buf, int *cnt, int *type, int *dest, int *tag, 
		    int *comm, int *ierr)
{
    *ierr = MPI_Send(buf, *cnt, *type, *dest, *tag, *comm);
}

void _F77(mpi_recv)(void *buf, int *cnt, int *type, int *src, int *tag, 
		    int *comm, MPI_Status *status, int *ierr) 
{
    *ierr = MPI_Recv(buf, *cnt, *type, *src, *tag, *comm, status);
}

void _F77(mpi_sendrecv)(void *sendbuf, int *sendcnt, int *sendtype,
			int *dest, int *sendtag, void *recvbuf, int *recvcnt, 
			int *recvtype, int *source, int *recvtag,
			int *comm, MPI_Status *status, int *ierr) 
{
    *ierr = MPI_Sendrecv(sendbuf, *sendcnt, *sendtype, *dest, *sendtag,
			 recvbuf, *recvcnt, *recvtype, *source, *recvtag,
			 *comm, status);
}

void _F77(mpi_bcast)(void *buf, int *cnt, int *type, int *src, int *comm, 
		     int *ierr)
{
    *ierr = MPI_Bcast(buf, *cnt, *type, *src, *comm);
}

void _F77(mpi_reduce)(void *sendbuf, void *recvbuf, int *count, int *datatype, 
		      int *op, int *root, int *comm, int *ierr)
{
    *ierr = MPI_Reduce(sendbuf, recvbuf, *count, *datatype, *op, *root, *comm);
}

void _F77(mpi_allreduce)(void *sendbuf, void *recvbuf, int *count, 
			 int *datatype, int *op, int *comm, int *ierr)
{
    *ierr = MPI_Allreduce(sendbuf, recvbuf, *count, *datatype, *op, *comm);
}

void _F77(mpi_barrier)(int *comm, int *ierr)
{
    *ierr = MPI_Barrier(*comm);
}

void _F77(mpi_alltoallv)(void *sbuf, int *sendcnts, int *sdispls, int *stype, 
			 void *rbuf, int *recvcnts, int *rdispls, int *rtype, 
			 int *comm, int *ierr)
{
    *ierr = MPI_Alltoallv(sbuf, sendcnts, sdispls, *stype,
			  rbuf, recvcnts, rdispls, *rtype, *comm);
}

void _F77(mpi_alltoall)(void *sendbuf, int *sendcount, int *sendtype,
			void *recvbuf, int *recvcount, int *recvtype, 
			int *comm, int *ierr) 
{
    *ierr = MPI_Alltoall(sendbuf, *sendcount, *sendtype,
			 recvbuf, *recvcount, *recvtype, *comm);
}

void _F77(mpi_comm_dup)(MPI_Comm *comm, MPI_Comm *newcomm, int *ierr)
{
    *ierr = MPI_Comm_dup(*comm, newcomm);
}

void _F77(mpi_comm_split)(MPI_Comm *comm, int *color, int *key, 
			  MPI_Comm *newcomm, int *ierr) 
{
    *ierr = MPI_Comm_split(*comm, *color, *key, newcomm);
}


double _F77(mpi_wtime)(void)
{
    return MPI_Wtime();
}

double _F77(mpi_wtick)(void)
{
    return MPI_Wtick();
}
