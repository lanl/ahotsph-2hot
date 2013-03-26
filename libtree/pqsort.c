/* Yet another pqsort.  This one uses a deterministic comm pattern and a
   deterministic amount of memory.  The memory used is:
     nfinal*size + nsend*(size + 8) + nproc*4
   where nfinal is the number of bodies we will end up with at the end,
   and nsend is the number of bodies we have at the beginning that don't
   belong to us.  nproc is, obviously, the number of processors.

   In the typical case where we are keeping most of what we have, the
   memory overhead is very low.  In the worst case, where we are
   sending out everything, the overhead is somewhat larger than the minimum
   memory needed to store the array, i.e., a factor of 2+eps.  This is
   better than the factor of three that we used to see.  Of course,
   all this extra memory is freed before returning.  The temp mem usage
   could probably still be improved for the worst case, but I am not
   convinced that the tree code runs with totalmem < 2*(bodymem) anyway,
   so what would be the point.  */
#define EXPENSIVE_ASSERTIONS  /* Allow some assertions involving DestDecomp */
#include <stdlib.h>
#include "pqsort.h"
#include "Assert.h"
#include "key.h"
#include "Malloc.h"
#include "malloc.h"
#include "mpmy.h"
#include "gc.h"
#include "Msgs.h"
#include "stk.h"
#include "timers.h"
#include "decomp.h"
#include "abm.h"
#include "rsort.h"

Timer_t PQSortTm;
Timer_t SortTm;

struct sortpair{
    int sortkey;
    void *p;
};

static int cmpsort(const void *k1, const void *k2); 
static Key_t (*getkey_s)(const void *);

#define SORT_TAG 4326

void pqsortsetup(sortresult_t *decompp, void *bp, int nobj, 
		int size, float median_tol,
		void *(*realloc_like)(void *, size_t)) {
    pqsortsetup_order(decompp, bp, nobj, size, median_tol, 0, realloc_like);
}

/* This should replace pqsortsetup in the next "major release" */
void pqsortsetup_order(sortresult_t *decompp, void *bp, int nobj, 
		int size, float median_tol, int proc_order, 
		void *(*realloc_like)(void *, size_t)) {
    decompp->data = bp;
    decompp->nobj = nobj;
    decompp->size = size;
    decompp->median_tol = median_tol;
    decompp->proc_order = proc_order;
    decompp->loadbal_target = 1.0;  /* default to no load balance */
    decompp->realloc_like = realloc_like;
}

void *pqsort(sortresult_t *decompp,
	    float (*weight)(const void *), Key_t (*getkey)(const void *))
{
    int i;
    size_t size;
    int  nobj;
    /* Lots of char*, really should be void*, but we do so much arithmetic
       that it's just too tedious to use void* */
    char *p, *q;
    char *data;
    char *tmp;
    char *aux, *auxend;
    int *nsendarr, *nrecvarr;
    int *sendoffsets, *recvoffsets;
    int dest;
    int incoming, nkeep, nsend;
    char *instart, *outstart, *outend;
    struct sortpair *sortarr, *sortp;
    int malloc_debug_reset = -1;

    StartTimer(&PQSortTm);
    if( Msg_test(__FILE__) )
	malloc_debug_reset = malloc_debug(2);
    Msgf(("pqsort: nobj is %d\n", decompp->nobj));
    getkey_s = getkey;
    
    size = decompp->size;
    nobj = decompp->nobj;
    data = decompp->data;
    if (MPMY_Nproc() == 1) goto sort;

    nsendarr = Calloc(MPMY_Nproc(), sizeof(int));
    tmp = Malloc(size);

    SetupDecomp(decompp, weight, getkey);
    StartTimer(&DecompCommTm);
    p = data;
    q = data + nobj*size;

    assert(size > 0);		/* otherwise it loops forever */
    while (p<q) {
	while (p < q && DestDecomp(p) == MPMY_Procnum()) {
	    p += size;
	}
	while (p < q && (q -= size, DestDecomp(q)) != MPMY_Procnum()) 
	    /* do nothing */;
	if (p < q ) {
	    memcpy(tmp, p, size);
	    memcpy(p, q, size);
	    memcpy(q, tmp, size);
	    p += size;
	}
    }
    Free(tmp);
    nkeep = (p - data)/size;
    nsend = nobj - nkeep;
    Msgf(("Finished testing particle destinations, nsend=%d, nkeep=%d\n",
	  nsend, nkeep));
    /* If I were clever, I could have incremented nsendarr while doing
       the loop above.  The loop overhead is trivial, but DestDecomp
       might be expensive.  But this way I can read the code! */

    q = data + nobj*size;
    nsendarr[MPMY_Procnum()] = nkeep;
    while (p < q) {
	nsendarr[DestDecomp(p)]++;
	p += size;
    }
    assert(nsendarr[MPMY_Procnum()] == nkeep);
    Msgf(("Before Combine:\n"));
    MPMY_Combine(nsendarr, nsendarr, MPMY_Nproc(), MPMY_INT, MPMY_SUM);
    Msgf(("After combine:\n"));

    /* If we don't free the arrays we've alloced here, we need to make them static so
       they don't interfere with growing the btab. */
    incoming = nsendarr[MPMY_Procnum()];
    Free(nsendarr);
    Msgf(("Preparing for final particle count of %d\n", incoming));
    if( Msg_test("memleak") ){
	Msg_do("Memory map before pqsort realloc\n");
	malloc_print();
    }
    if (incoming > nobj) {
	data = Realloc(data, (size_t)size*incoming);
    }
    
    /* It would have been nice to keep nsendarr around, but we had to free
       it to avoid fragmentation when we Realloc the data buffer. */
    nsendarr = Calloc(MPMY_Nproc(), sizeof(int));
    /* This is just a tricky way of sorting the 'aux' array based on
       DestDecomp(), and minimizing the number of calls to
       DestDecomp().   Of course, it burns temp space.  This is the
       source of the +8*nsend temp space cited in the header comment. */
    sortarr = Malloc(nsend*sizeof(sortarr[0]));

    p = data + nkeep*size;
    outend = p + nsend*size;
    sortp = sortarr;
    Msgf(("After {M,C,Re}alloc: data=%p, p=%p, outend=%p, sortp=%p\n",
	  data, p, outend, sortp));
    while (p < outend) {
	dest = DestDecomp(p);
	nsendarr[dest]++;
	sortp->sortkey = dest;
	sortp->p = p;
	p += size;
	sortp++;
    }
    Msgf(("Before qsort:  nsend=%d, sortarr=%p\n", nsend, sortarr));
    if (nsend > 0) {
	qsort(sortarr, nsend, sizeof(sortarr[0]), cmpsort);
    }
    Msgf(("After qsort!\n"));
    aux = Malloc((size_t)nsend*size);
    auxend = aux + nsend*size;
    q = aux;
    sortp = sortarr;
    Msgf(("Before copying to aux: q=%p, auxend=%p, sortp=%p\n",
	  q, auxend, sortp));
    while (q < auxend) {
	memcpy(q, sortp->p, size);
	/* Verify that it's truly sorted. */
#ifdef EXPENSIVE_ASSERTIONS
	assert(q==aux || DestDecomp(q) >= DestDecomp(q-size));
#endif
	q += size;
	sortp++;
    }
    Free(sortarr);
    /* We're done with sorting for now. */

    nrecvarr = Calloc(MPMY_Nproc(), sizeof(int));
    Native_MPMY_Alltoall(nsendarr, 1, MPMY_INT, nrecvarr, 1, MPMY_INT);

    outstart = aux;
    instart = data + nkeep*size;
    Msgf(("aux=%p, data=%p, instart=%p\n", aux, data, instart));

    sendoffsets = Calloc(MPMY_Nproc(), sizeof(int));
    recvoffsets = Calloc(MPMY_Nproc(), sizeof(int));
    
    assert(size % sizeof(int) == 0);
    for (i = 0; i < MPMY_Nproc(); i++) {
	nsendarr[i] *= size/sizeof(int);
	nrecvarr[i] *= size/sizeof(int);
	if (i != 0) {
	    sendoffsets[i] = sendoffsets[i-1] + nsendarr[i-1];
	    recvoffsets[i] = recvoffsets[i-1] + nrecvarr[i-1];
	} else {
	    sendoffsets[i] = 0;
	    recvoffsets[i] = 0;
	}
#if 0
	if (nsendarr[i] || nrecvarr[i]) {
	    Msgf(("[%5d] %5d @ %5d %5d @ %5d\n", i, nsendarr[i], sendoffsets[i],
		  nrecvarr[i], recvoffsets[i]));
	}
#endif
    }

    Msgf(("Before Alltoallv:\n"));
    MPMY_Alltoallv(outstart, nsendarr, sendoffsets, MPMY_INT, 
		   instart, nrecvarr, recvoffsets, MPMY_INT);
    Msgf(("After Alltoallv:\n"));
#ifdef EXPENSIVE_ASSERTIONS
    for (i = 0; i < MPMY_Nproc(); i++) {
	if (nrecvarr[i]) {
	    dest = DestDecomp(instart+recvoffsets[i]*sizeof(int));
	    if (dest != MPMY_Procnum()) {
		Error("Bad dest from %d (%d) after Alltoallv\n", i, dest);
	    }
	}
    }
#endif
    
    Free(recvoffsets);
    Free(sendoffsets);
    Free(nrecvarr);
    Free(aux);
    Free(nsendarr);

    if (incoming < nobj) {
	data = Realloc(data, size*incoming);
    }
    StopTimer(&DecompCommTm);
    Msgf(("calling FinishDecomp\n"));
    FinishDecomp();

    decompp->data = data;
    decompp->nobj = incoming;
    
  sort:
    if (decompp->nobj ==  0) {
	Msgf(("first key is (null)\nlast key is (null)\n"));
    } else {
	StartTimer(&SortTm);
	rsort(decompp->data, decompp->nobj, decompp->size, 12, KEYBITS, getkey);
	/* qsort(decompp->data, decompp->nobj, decompp->size, cmpkey); */
	StopTimer(&SortTm);
	Msgf(("first key is %s, ",  PrintKey(getkey(decompp->data))));
	Msgf(("last key is %s\n", 
	      PrintKey(getkey((char *)decompp->data + (decompp->nobj-1)*size))));
    }
    if( malloc_debug_reset >= 0 )
	malloc_debug(malloc_debug_reset);
    StopTimer(&PQSortTm);
    return decompp->data;
}	     

static int cmpsort(const void *a, const void *b){
    return ((struct sortpair *)a)->sortkey - ((struct sortpair *)b)->sortkey;
}

