#include <sys/types.h>
#include "Malloc.h"
#include "mpmy.h"
#include "stk.h"
#include "gc.h"
#include "Msgs.h"

static int setup;
static int mask;

static Stk cstk, outdata;

struct comb_st {
    void *recvbuf;
    int count;
    int datatype;
    union{
	MPMY_Op op;
	MPMY_user_comb_func user_func;
    } u;
};

/* This currently only works with one request outstanding at a time */

int 
MPMY_ICombine_Init(MPMY_Comm_request *reqp)
{
    StkInitEz(&cstk);
    StkInitEz(&outdata);
    setup = 1;
    mask = ~0;
    *reqp = 0;
    return MPMY_SUCCESS;
}

int
MPMY_ICombine(const void *sendbuf, void *recvbuf, int count, 
	      MPMY_Datatype datatype, MPMY_Op op, 
	      MPMY_Comm_request req)
{
    struct comb_st *combuf;
    int total_size;

    if (setup == 0) 
      Error("MPMY_ICombine with no call to Init\n");
    total_size = count * MPMY_Datasize[datatype];

    StkPushData(&outdata, (void *)sendbuf, total_size);
    combuf = StkPush(&cstk, sizeof(struct comb_st));

    combuf->recvbuf = recvbuf;
    combuf->count = count;
    combuf->datatype = datatype;
    combuf->u.op = op;

    return MPMY_SUCCESS;
}

int
MPMY_ICombine_func(const void *sendbuf, void *recvbuf, int size, 
		   void (*func)(const void *, const void *, void*),
		   MPMY_Comm_request req)
{
    struct comb_st *combuf;
    int total_size;

    Msgf(("MPMY_Icombine_func(): %p %p %d %p\n", 
	  sendbuf, recvbuf, size, func));
    if (setup == 0) 
      Error("MPMY_ICombine with no call to Init\n");
    total_size = size;

    StkPushData(&outdata, (void *)sendbuf, total_size);
    combuf = StkPush(&cstk, sizeof(struct comb_st));

    combuf->recvbuf = recvbuf;
    combuf->count = size;
    combuf->datatype = MPMY_USER_DATA;
    combuf->u.user_func = func;

    return MPMY_SUCCESS;
}

void
MPMY_Combine_Mask(unsigned int maskval)
{
    mask = maskval;
}


/* gcc with optimization will cause this to fail efence */
#define Do_Op(outbuf, op, inbuf, type, cnt) \
do{char *oend = (char *)outbuf + StkAlign(&outdata, cnt*sizeof(type)); \
   char *iend = (char *)inbuf + StkAlign(&outdata, cnt*sizeof(type)); \
   while (cnt--) { \
     *(type *)(outbuf) op *(type *)(inbuf); \
     (outbuf) = (char *)(outbuf) + sizeof(type); \
     (inbuf) = (char *)(inbuf) + sizeof(type); \
   } \
   outbuf = oend; \
   inbuf = iend; \
}while(0)


static void 
do_combine(const void *inbuf, void *outbuf, const int n, 
	   const struct comb_st *manifest)
{
    int i;
    int count;
    
    for (i = 0; i < n; i++, manifest++) {
	count = manifest->count;
	if (count < 0) Error("Bad count in Combine\n");
	Msgf(("do_combine: %p %p %d %d\n", 
	      inbuf, outbuf, manifest->datatype, manifest->u.op));
	switch(manifest->datatype) {
	  case MPMY_FLOAT:
#define Type float
#include "op_template.c"
#undef Type
	    break;
	  case MPMY_DOUBLE:
#define Type double
#include "op_template.c"
#undef Type
	    break;
	  case MPMY_INT:
#define BIT_OPS		/* Turns on bitwise ops in op_template.c */
#define Type int
#include "op_template.c"
#undef Type
	    break;
	  case MPMY_CHAR:
#define Type char
#include "op_template.c"
#undef Type
	    break;
	  case MPMY_SHORT:
#define Type short
#include "op_template.c"
#undef Type
	    break;
	  case MPMY_LONG:
#define Type long
#include "op_template.c"
#undef Type
	  case MPMY_OFFT:
#define Type off_t
#include "op_template.c"
#undef Type
	    break;
	  case MPMY_INT64:
#define Type int64_t
#include "op_template.c"
#undef Type
	    break;
	  case MPMY_UNSIGNED_INT:
#define Type unsigned int
#include "op_template.c"
#undef Type
	    break;
	  case MPMY_UNSIGNED_CHAR:
#define Type unsigned char
#include "op_template.c"
#undef Type
	    break;
	  case MPMY_UNSIGNED_SHORT:
#define Type unsigned short
#include "op_template.c"
#undef Type
	    break;
	  case MPMY_UNSIGNED_LONG:
#define Type unsigned long
#include "op_template.c"
#undef Type
	    break;
#undef BIT_OPS
	case MPMY_USER_DATA:
	    /* This came from a MPMY_ICombine_func */
	    (*manifest->u.user_func)(inbuf, outbuf, outbuf);
	    (outbuf) = (char *)(outbuf) + StkAlign(&outdata, manifest->count);
	    (inbuf) = (char *)(inbuf) + StkAlign(&outdata, manifest->count);
	    break;
	default:
	    Error("Unknown type in Combine\n");
	}
    }
}

int
MPMY_ICombine_Wait(MPMY_Comm_request req)
{
    int chan;
    int doc;
    int i;
    void *inbuf;
    void *outbuf = StkBase(&outdata);
    MPMY_Status stat;
    int ret;
    int total_size;
    int sendproc;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();
    int bufsz = StkSz(&outdata);
    const struct comb_st *manifest = StkBase(&cstk);
    int n = StkSz(&cstk)/sizeof(struct comb_st);

    Msgf(("MPMY_ICombine_Wait()\n"));
    doc = ilog2(nproc);
    if (nproc != 1 << doc)
      doc++;			/* for non power-of-two sizes */
    inbuf = Malloc(bufsz);

    for (chan = 0; chan < doc; chan++) {
	if (mask & (1 << chan)) {
	    sendproc = procnum^(1<<chan);
	    if (sendproc >= 0 && sendproc < nproc) {
		MPMY_Shift(sendproc, inbuf, bufsz, outbuf, bufsz, &stat);
		ret = MPMY_Count(&stat);
		if (ret != bufsz) 
		  Error("Shift failed, expected %d got %d\n", bufsz, ret);
		do_combine(inbuf, outbuf, n, manifest);
	    }
	}
    }
    if (nproc != 1 << doc) {
	MPMY_Bcast(outbuf, bufsz, MPMY_CHAR, 0);
    }
    for (i = 0; i < n; i++) {
	total_size = manifest->count * MPMY_Datasize[manifest->datatype];
	memcpy(manifest->recvbuf, outbuf, total_size);
	outbuf = (char *)outbuf + StkAlign(&outdata, total_size);
	manifest = (const struct comb_st *)((char *)manifest + StkAlign(&outdata, sizeof(*manifest)));
    }
    Free(inbuf);
    StkTerminate(&outdata);
    StkTerminate(&cstk);
    setup = 0;
    return MPMY_SUCCESS;
}

int
MPMY_Combine(const void *sendbuf, void *recvbuf, const int count, 
	     const MPMY_Datatype datatype, const MPMY_Op op)
{
    MPMY_Comm_request req;

    MPMY_ICombine_Init(&req);
    MPMY_ICombine(sendbuf, recvbuf, count, datatype, op, req);
    return MPMY_ICombine_Wait(req);
}
