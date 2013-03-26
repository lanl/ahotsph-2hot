#include <stdlib.h>
#include "swampi.h"
#include "stk.h"
#include "gc.h"
#include "Msgs.h"
#include "error.h"

static int setup;
static int mask;

static Stk cstk, outdata;

#define TAG 5

struct comb_st {
    void *recvbuf;
    int count;
    MPI_Datatype datatype;
    union{
	MPI_Op op;
	MPI_user_comb_func user_func;
    } u;
};

/* This currently only works with one request outstanding at a time */

static int 
MPI_IreduceInit(MPI_Request *reqp)
{
    StkInitEz(&cstk);
    StkInitEz(&outdata);
    setup = 1;
    mask = ~0;
    *reqp = 0;
    return MPI_SUCCESS;
}

static int
MPI_Ireduce(void *sendbuf, void *recvbuf, int count, 
	    MPI_Datatype datatype, MPI_Op op, 
	    MPI_Request req, MPI_Comm comm)
{
    struct comb_st *combuf;
    int total_size;

    if (setup == 0) 
      Error("MPI_Ireduce with no call to MPI_IreduceInit\n");
    total_size = count * _MPI_Datasize[datatype];

    StkPushData(&outdata, (void *)sendbuf, total_size);
    combuf = StkPush(&cstk, sizeof(struct comb_st));

    combuf->recvbuf = recvbuf;
    combuf->count = count;
    combuf->datatype = datatype;
    combuf->u.op = op;

    return MPI_SUCCESS;
}

#if 0
static int
MPI_IreduceFunc(void *sendbuf, void *recvbuf, int size, 
		 void (*func)(void *, void *, void*),
		 MPI_Request req)
{
    struct comb_st *combuf;
    int total_size;

    if (setup == 0) 
      Error("MPI_Ireduce_func with no call to MPI_IreduceInit\n");
    total_size = size;

    StkPushData(&outdata, (void *)sendbuf, total_size);
    combuf = StkPush(&cstk, sizeof(struct comb_st));

    combuf->recvbuf = recvbuf;
    combuf->count = size;
    combuf->datatype = MPI_USER_DATA;
    combuf->u.user_func = func;

    return MPI_SUCCESS;
}

static int
MPI_Setmask(unsigned int maskval)
{
    mask = maskval;
    return MPI_SUCCESS;
}
#endif

#define ALIGN(n) ((n + _STK_DEFAULT_ALIGNMENT)&~_STK_DEFAULT_ALIGNMENT)

/* gcc with optimization will cause this to fail efence */
#define Do_Op(outbuf, op, inbuf, type, cnt) \
do{char *oend = (char *)outbuf + ALIGN(cnt*sizeof(type)); \
   char *iend = (char *)inbuf + ALIGN(cnt*sizeof(type)); \
   while (cnt--) { \
     *(type *)(outbuf) op *(type *)(inbuf); \
     (outbuf) = (char *)(outbuf) + sizeof(type); \
     (inbuf) = (char *)(inbuf) + sizeof(type); \
   } \
   outbuf = oend; \
   inbuf = iend; \
}while(0) 


static void 
do_combine(void *inbuf, void *outbuf, int n, struct comb_st *manifest)
{
    int i;
    int count;
    
    for (i = 0; i < n; i++, manifest++) {
	count = manifest->count;
	if (count < 0) Error("Bad count in reduce\n");

	Msgf(("mpi: reduce Op %s Datatype %s\n", mpi_op_name[manifest->u.op], 
	      mpi_datatype_name[manifest->datatype]));
	switch(manifest->datatype) {
	  case MPI_FLOAT:
#define Type float
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_DOUBLE:
#define Type double
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_LONG_DOUBLE:
#define Type double
#include "mpi_template.c"
#undef Type
	    break;

#define BIT_OPS		/* Turns on bitwise ops in mpi_template.c */
	  case MPI_BYTE:
	  case MPI_CHAR:
#define Type char
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_SHORT:
#define Type short
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_INT:
#define Type int
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_LONG:
#define Type long
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_LONG_LONG:
#define Type long
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_UNSIGNED:
	  case MPI_UNSIGNED_INT:
#define Type unsigned int
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_UNSIGNED_CHAR:
#define Type unsigned char
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_UNSIGNED_SHORT:
#define Type unsigned short
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_UNSIGNED_LONG:
#define Type unsigned long
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_UNSIGNED_LONG_LONG:
#define Type unsigned long
#include "mpi_template.c"
#undef Type
	    break;
#undef BIT_OPS

#define LOC_OPS
	  case MPI_FLOAT_INT:
#define Type MPI_float_int
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_DOUBLE_INT:
#define Type MPI_double_int
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_LONG_INT:
#define Type MPI_long_int
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_2INT:
#define Type MPI_2int
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_SHORT_INT:
#define Type MPI_short_int
#include "mpi_template.c"
#undef Type
	    break;
	  case MPI_LONG_DOUBLE_INT:
#define Type MPI_long_double_int
#include "mpi_template.c"
#undef Type
	    break;
#undef LOC_OPS
	  case MPI_COMPLEX:
#define Type MPI_complex
	  {
	      Type *out = outbuf;
	      Type *in = inbuf;
	      outbuf = (char *)outbuf + ALIGN(count*sizeof(Type));
	      inbuf = (char *)inbuf + ALIGN(count*sizeof(Type));
	      switch(manifest->u.op) {
		  case MPI_SUM:
		      while (count--) {
			  out->real += in->real;
			  out->imag += in->imag;
			  out++;
			  in++;
		      }
		      break;
		  case MPI_PROD:
		      while (count--) {
			  out->real = out->real*in->real - out->imag*in->imag;
			  out->imag = out->real*in->imag + out->imag*in->real;
			  out++;
			  in++;
		      }
		      break;
		  default:
		      Error("Unknown op in MPI_reduce\n");
	      }
	  }
#undef Type
	    break;
	  case MPI_DOUBLE_COMPLEX:
#define Type MPI_double_complex
	  {
	      Type *out = outbuf;
	      Type *in = inbuf;
	      outbuf = (char *)outbuf + ALIGN(count*sizeof(Type));
	      inbuf = (char *)inbuf + ALIGN(count*sizeof(Type));
	      switch(manifest->u.op) {
		  case MPI_SUM:
		      while (count--) {
			  out->real += in->real;
			  out->imag += in->imag;
			  out++;
			  in++;
		      }
		      break;
		  case MPI_PROD:
		      while (count--) {
			  out->real = out->real*in->real - out->imag*in->imag;
			  out->imag = out->real*in->imag + out->imag*in->real;
			  out++;
			  in++;
		      }
		      break;
		  default:
		      Error("Unknown op in MPI_reduce\n");
	      }
	  }
#undef Type
	    break;
	case MPI_USER_DATA:
	    /* This came from a MPI_ICombine_func */
	    (*manifest->u.user_func)(inbuf, outbuf, outbuf);
	    (outbuf) = (char *)(outbuf) + ALIGN(manifest->count);
	    (inbuf) = (char *)(inbuf) + ALIGN(manifest->count);
	    break;
	default:
	    Error("Unknown type in Combine\n");
	}
    }
}

int
MPI_IreduceWait(MPI_Request req, MPI_Comm comm)
{
    int chan;
    int doc;
    int i;
    void *inbuf;
    void *outbuf = StkBase(&outdata);
    MPI_Status stat;
    int ret;
    int total_size;
    int sendproc;
    int procnum = _MPI_Procnum;
    int nproc = _MPI_Nproc;
    int bufsz = StkSz(&outdata);
    struct comb_st *manifest = StkBase(&cstk);
    int n = StkSz(&cstk)/sizeof(struct comb_st);

    doc = ilog2(nproc);
    if (nproc != 1 << doc)
      doc++;			/* for non power-of-two sizes */
    if ((inbuf = malloc(bufsz)) == NULL)
	Error("out of memory\n");

    for (chan = 0; chan < doc; chan++) {
	if (mask & (1 << chan)) {
	    sendproc = procnum^(1<<chan);
	    if (sendproc >= 0 && sendproc < nproc) {
		MPI_Sendrecv(outbuf, bufsz, MPI_BYTE, sendproc, TAG, 
			     inbuf, bufsz, MPI_BYTE, sendproc, TAG, 
			     comm, &stat);
		MPI_Get_count(&stat, MPI_BYTE, &ret);
		if (ret != bufsz) 
		    Error("Shift failed, expected %d got %d\n", bufsz, ret);
		do_combine(inbuf, outbuf, n, manifest);
	    }
	}
    }
    if (nproc != 1 << doc) {
	MPI_Bcast(outbuf, bufsz, MPI_BYTE, 0, comm);
    }
    for (i = 0; i < n; i++) {
	total_size = manifest->count * _MPI_Datasize[manifest->datatype];
	memcpy(manifest->recvbuf, outbuf, total_size);
	outbuf = (char *)outbuf + ALIGN(total_size);
	manifest = (struct comb_st *)((char *)manifest + ALIGN(sizeof(*manifest)));
    }
    free(inbuf);
    StkTerminate(&outdata);
    StkTerminate(&cstk);
    setup = 0;
    return MPI_SUCCESS;
}

int
MPI_Allreduce(void *sendbuf, void *recvbuf, int count, 
	      MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
{
    MPI_Request req;

    Msgf(("mpi: Allreduce\n"));
    if (op < 0 || op >= _MPI_NUMOPS) Error("Bad MPI_Op\n");
    MPI_IreduceInit(&req);
    MPI_Ireduce(sendbuf, recvbuf, count, datatype, op, req, MPI_COMM_PRIVATE);
    MPI_IreduceWait(req, MPI_COMM_PRIVATE);
    return MPI_SUCCESS;
}

int
MPI_Reduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, 
	   MPI_Op op, int root, MPI_Comm comm)
{
    Msgf(("mpi: Reduce\n"));
    if (op < 0 || op >= _MPI_NUMOPS) Error("Bad MPI_Op\n");
    if (_MPI_Procnum != root)
	if ((recvbuf = malloc(count * _MPI_Datasize[datatype])) == NULL)
	    Error("out of memory\n");
    MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
    if (_MPI_Procnum != root) free(recvbuf);
    return MPI_SUCCESS;
}

