#ifndef _MpiDOTh
#define _MpiDOTh

#define MPI_Init SWAMPI_Init
#define MPI_Finalize SWAMPI_Finalize
#define MPI_Abort SWAMPI_Abort
#define MPI_Comm_rank SWAMPI_Comm_rank
#define MPI_Comm_size SWAMPI_Comm_size
#define MPI_Comm_free SWAMPI_Comm_free
#define MPI_Get_count SWAMPI_Get_count
#define MPI_Isend SWAMPI_Isend
#define MPI_Irecv SWAMPI_Irecv
#define MPI_Issend SWAMPI_Isend	/* can Isend handle Issends? */
#define MPI_Test SWAMPI_Test
#define MPI_Wait SWAMPI_Wait
#define MPI_Waitall SWAMPI_Waitall
#define MPI_Send SWAMPI_Send
#define MPI_Recv SWAMPI_Recv
#define MPI_Sendrecv SWAMPI_Sendrecv
#define MPI_Bcast SWAMPI_Bcast
#define MPI_Reduce SWAMPI_Reduce
#define MPI_Allreduce SWAMPI_Allreduce
#define MPI_Barrier SWAMPI_Barrier
#define MPI_Alltoallv SWAMPI_Alltoallv
#define MPI_Alltoall SWAMPI_Alltoall
#define MPI_Comm_dup SWAMPI_Comm_dup
#define MPI_Comm_split SWAMPI_Comm_split
#define MPI_Type_contiguous SWAMPI_Type_contiguous
#define MPI_Type_commit SWAMPI_Type_commit
#define MPI_Wtime SWAMPI_Wtime
#define MPI_Wtick SWAMPI_Wtick

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

typedef struct {
    int MPI_SOURCE;
    int MPI_TAG;
    int MPI_ERROR;
    int count;
} MPI_Status;

/* MAXLOC and MINLOC structures */
typedef struct { float x; int i;} MPI_float_int;
typedef struct { double x; int i;} MPI_double_int;
typedef struct { long x; int i;} MPI_long_int;
typedef struct { int x; int i;} MPI_2int;
typedef struct { short x; int i;} MPI_short_int;
typedef struct { long double x; int i;} MPI_long_double_int;

/* Fortran types */
typedef struct { float real; float imag; } MPI_complex;
typedef struct { double real; double imag; } MPI_double_complex;

/* must match MPI_Datasize array in swampi.c */
typedef enum {
    MPI_FLOAT, MPI_DOUBLE, MPI_LONG_DOUBLE, 
    MPI_BYTE, MPI_CHAR, MPI_SHORT, MPI_INT, 
    MPI_LONG, MPI_LONG_LONG,
    MPI_UNSIGNED, MPI_UNSIGNED_INT, MPI_UNSIGNED_CHAR, 
    MPI_UNSIGNED_SHORT, MPI_UNSIGNED_LONG, MPI_UNSIGNED_LONG_LONG,
    MPI_FLOAT_INT, MPI_DOUBLE_INT, MPI_LONG_INT, 
    MPI_2INT, MPI_SHORT_INT, MPI_LONG_DOUBLE_INT,
    MPI_COMPLEX, MPI_DOUBLE_COMPLEX, /* for Fortran */
    MPI_USER_DATA, _MPI_NUMDATATYPES
} MPI_Datatype;

typedef enum {
    MPI_SUM, MPI_PROD, MPI_MAX, MPI_MIN, MPI_BAND, MPI_BOR,
    MPI_BXOR, MPI_LAND, MPI_LOR, MPI_LXOR, MPI_MAXLOC, MPI_MINLOC,
    _MPI_NUMOPS
} MPI_Op;


typedef void (*MPI_user_comb_func)(void *from1, void *from2, void *to);
typedef int MPI_Comm;
typedef void * MPI_Request;

enum MPI_comm { MPI_COMM_WORLD, MPI_COMM_PRIVATE, MPI_COMM_NULL };
enum MPI_src { MPI_ANY_SOURCE = -1 };
enum MPI_tag { MPI_ANY_TAG = -1 };
enum MPI_ret { MPI_ERR_OTHER = -1, MPI_SUCCESS = 0 };

#define MPI_UNDEFINED (-32766)
#define MPI_REQUEST_NULL ((MPI_Request) 0)

int MPI_Init(int *argcp, char ***argvp);
int MPI_Finalize(void);
int MPI_Abort(MPI_Comm comm, int errorcode);
int MPI_Comm_rank(MPI_Comm comm, int *rank);
int MPI_Comm_size(MPI_Comm comm, int *size);
int MPI_Get_count(MPI_Status *status, MPI_Datatype type, int *cnt);
int MPI_Isend(void *buf, int cnt, MPI_Datatype type, int dest, int tag, 
	      MPI_Comm comm, MPI_Request *req);
int MPI_Irecv(void *buf, int cnt, MPI_Datatype type, int src, int tag, 
	      MPI_Comm comm, MPI_Request *req);
int MPI_Test(MPI_Request *cptr, int *flag, MPI_Status *stat);
int MPI_Wait(MPI_Request *cptr, MPI_Status *status);
int MPI_Waitall(int count, MPI_Request *reqv, MPI_Status *statusv);
int MPI_Send(void *buf, int cnt, MPI_Datatype type, int dest, int tag, 
	     MPI_Comm comm);
int MPI_Recv(void *buf, int cnt, MPI_Datatype type, int src, int tag, 
	     MPI_Comm comm, MPI_Status *status);
int MPI_Sendrecv(void *sendbuf, int sendcount, MPI_Datatype sendtype,
		 int dest, int sendtag, void *recvbuf, int recvcount, 
		 MPI_Datatype recvtype, int source, int recvtag, 
		 MPI_Comm comm, MPI_Status *status);
int MPI_Bcast(void *buf, int cnt, MPI_Datatype type, int src, MPI_Comm comm);
int MPI_Reduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, 
	       MPI_Op op, int root, MPI_Comm comm);
int MPI_Allreduce(void *sendbuf, void *recvbuf, int count, 
		  MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
int MPI_Barrier(MPI_Comm comm);
int MPI_Alltoallv(void *sbuf, int *sendcnts, int *sdispls, MPI_Datatype stype, 
		  void *rbuf, int *recvcnts, int *rdispls, MPI_Datatype rtype, 
		  MPI_Comm comm);
int MPI_Alltoall(void *sendbuf, int sendcount, MPI_Datatype sendtype,
		 void *recvbuf, int recvcount, MPI_Datatype recvtype, 
		 MPI_Comm comm);
int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm);
int MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm);
int MPI_Comm_free(MPI_Comm *comm);
int MPI_Type_contiguous(int len, MPI_Datatype type, MPI_Datatype *ptr);
int MPI_Type_commit(MPI_Datatype *ptr);

double MPI_Wtime(void);
double MPI_Wtick(void);

/* These convert the enums to a text description */
char *mpi_op_name[_MPI_NUMOPS], *mpi_datatype_name[_MPI_NUMDATATYPES];

/* These are private for mpi_reduce.c and mpi_bcast.c */
extern int _MPI_Procnum, _MPI_Nproc;
extern unsigned int *_MPI_Datasize;

/* These are for mpirun.c */
void _MPI_init_host1(int *portp, char **namep, int nproc);
void _MPI_init_host(int nproc);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MpiDOTh */
