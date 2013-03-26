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
#define ALLOW_QSORT0		/* allow qsort(p=NUL, nelem=0, ...) */
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

#define MAXDOC 15
int nsendarr[1<<MAXDOC];
int nrecvarr[1<<MAXDOC];

Timer_t SortTm;

struct sortpair{
    int sortkey;
    void *p;
};

static int cmpkey(const void *k1, const void *k2); 
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
    int size, nobj;
    /* Lots of char*, really should be void*, but we do so much arithmetic
       that it's just too tedious to use void* */
    char *p, *q;
    char *data;
    char *tmp;
    char *aux, *auxend;
    int *inarr;
    int dest, doc;
    int incoming, nkeep, nsend;
    int relative;
    char *instart, *inend;
    char *outstart, *outend;
    MPMY_Status stat;
    struct sortpair *sortarr, *sortp;
    int malloc_debug_reset = -1;

    if( Msg_test(__FILE__) )
	malloc_debug_reset = malloc_debug(2);
    Msgf(("pqsort: nobj is %d\n", decompp->nobj));
    getkey_s = getkey;
    
    size = decompp->size;
    nobj = decompp->nobj;
    data = decompp->data;
    if (MPMY_Nproc() == 1) goto sort;

    if (MPMY_Nproc() >= (1 << MAXDOC)) Error("MAXDOC too small\n");
    for (i = 0; i < MPMY_Nproc(); i++) {
	nsendarr[i] = nrecvarr[i] = 0;
    }
    inarr = Calloc(MPMY_Nproc(), sizeof(int));
    tmp = Malloc(size);

    SetupDecomp(decompp, weight, getkey);
    StartTimer(&DecompCommTm);
    p = data;
    q = data + nobj*size;

    assert(size > 0);		/* otherwise it loops forever */
    while(p<q){
	while(p<q && DestDecomp(p) == MPMY_Procnum()){
	    p += size;
	}
	while(p<q && (q-=size, DestDecomp(q)) != MPMY_Procnum()) 
	    /* do nothing */;
	if( p < q ){
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
    while( p < q ){
	nsendarr[DestDecomp(p)]++;
	p += size;
    }
    assert(nsendarr[MPMY_Procnum()] == nkeep);
    Msgf(("Before Combine:\n"));
    MPMY_Alltoall(nsendarr, 1, MPMY_INT, nrecvarr, 1, MPMY_INT);
    MPMY_Combine(nsendarr, inarr, MPMY_Nproc(), MPMY_INT, MPMY_SUM);
    Msgf(("After combine:\n"));
    for( relative=0; relative<MPMY_Nproc(); relative++){
	if (nsendarr[relative] || nrecvarr[relative])
	    Msgf(("[%5d] send %5d recv %5d\n", relative, nsendarr[relative], nrecvarr[relative]));
    }


    /* If we don't free nsend here, we need to make it static so
       it doesn't interfere with growing the btab. */
    incoming = inarr[MPMY_Procnum()];
    Free(inarr);
    Msgf(("Preparing for final particle count of %d\n", incoming));
    if( Msg_test("memleak") ){
	Msg_do("Memory map before pqsort realloc\n");
	malloc_print();
    }
    if( incoming > nobj ){
	data = Realloc(data, size*incoming);
    }
    
    /* This is just a tricky way of sorting the 'aux' array based on
       DestDecomp()^MPMY_Procnum, and minimizing the number of calls to
       DestDecomp().   Of course, it burns temp space.  This is the
       source of the +8*nsend temp space cited in the header comment. */
    sortarr = Malloc(nsend*sizeof(sortarr[0]));

    p = data + nkeep*size;
    outend = p + nsend*size;
    sortp = sortarr;
    Msgf(("After {M,C,Re}alloc: data=%p, p=%p, outend=%p, sortp=%p\n",
	  data, p, outend, sortp));
    while( p < outend ){
	dest = DestDecomp(p);
	sortp->sortkey = dest ^ MPMY_Procnum();
	sortp->p = p;
	p += size;
	sortp++;
    }
    Msgf(("Before qsort:  nsend=%d, sortarr=%p\n", nsend, sortarr)); Msg_flush();
#ifdef ALLOW_QSORT0
    if( nsend > 0 ){
#endif
	/* indirect evidence suggests qsort raises SIGSEGV or SIGBUS
	   on paragon when asked to sort 0 objects.  Direct testing
	   did not discover it though??? */
	qsort(sortarr, nsend, sizeof(sortarr[0]), cmpsort);
#ifdef ALLOW_QSORT0
    }
#endif
    Msgf(("After qsort!\n")); Msg_flush();
    aux = Malloc((size_t)nsend*size);
    auxend = aux + nsend*size;
    q = aux;
    sortp = sortarr;
    Msgf(("Before copying to aux: q=%p, auxend=%p, sortp=%p\n",
	  q, auxend, sortp));
    while(q < auxend){
	memcpy(q, sortp->p, size);
	/* Verify that it's truly sorted. */
#ifdef EXPENSIVE_ASSERTIONS
	assert(q==aux 
	       || (DestDecomp(q)^MPMY_Procnum()) >= (DestDecomp(q-size)^MPMY_Procnum()));
#endif
	q += size;
	sortp++;
    }
    Free(sortarr);
    /* We're done with sorting for now. */
    
    outend = aux;
    instart = data + nkeep*size;
    inend = data + incoming*size;
    Msgf(("aux=%p, data=%p, instart=%p, inend=%p\n", aux, data, instart, inend));
    doc = ilog2(MPMY_Nproc()-1) + 1;
    for (relative = 1; relative < 1<<doc; relative++) {
	int outsize, insize;
	dest = relative ^ MPMY_Procnum();
	if (dest >= MPMY_Nproc())
	    continue;
	if (nsendarr[dest] == 0 && nrecvarr[dest] == 0)
	    continue;
	outstart = outend;
	outsize = nsendarr[dest]*size;
	outend = outstart + outsize;
	insize = nrecvarr[dest]*size;
	inend = instart+insize;
	Msgf(("MPMY_Shift(dest=%d, instart=%p, incnt=%d, outstart=%p, outcnt=%d)\n",
	      dest, instart, (int)((inend-instart)/size), 
	      outstart, (int)((outend-outstart)/size)));
#ifdef EXPENSIVE_ASSERTIONS
	    assert( outend==outstart || 
		   (DestDecomp(outstart) == DestDecomp(outend-size) && 
		   DestDecomp(outstart) == dest));
#endif
	MPMY_Shift(dest, instart, inend-instart,
		   outstart, outend - outstart, &stat);
	instart += MPMY_Count(&stat);
    }
    assert(outend == aux + nsend*size);
    assert(instart == inend);
    Free(aux);

    if( incoming < nobj ){
	data = Realloc(data, size*incoming);
    }
    StopTimer(&DecompCommTm);
    Msgf(("calling FinishDecomp\n"));
    FinishDecomp();

    decompp->data = data;
    decompp->nobj = incoming;
    
  sort:
    if( decompp->nobj ==  0 ){
	/* qsort (nobj=0) fails on paragon!  This might indicate
	 a problem, but at least we won't SEGfault this way... */
	Warning("pqsort produces nobj==0.  Be very afraid\n");
	Msgf(("first key is (null)\nlast key is (null)\n"));
    }else{
	StartTimer(&SortTm);
	qsort(decompp->data, decompp->nobj, decompp->size, cmpkey);
	StopTimer(&SortTm);
	Msgf(("first key is %s, ",  PrintKey(getkey(decompp->data))));
	Msgf(("last key is %s\n", 
	      PrintKey(getkey((char *)decompp->data + (decompp->nobj-1)*size))));
    }
    if( malloc_debug_reset >= 0 )
	malloc_debug(malloc_debug_reset);
    Msg_flush();
    return decompp->data;
}	     

    
static int cmpkey(const void *a, const void *b){
    Key_t ka;
    Key_t kb;
    ka = getkey_s(a);
    kb = getkey_s(b);
    if( KeyGT(ka, kb) )
	return 1;
    else if( KeyLT(ka, kb) )
	return -1;
    else
	return 0;
}

static int cmpsort(const void *a, const void *b){
    return ((struct sortpair *)a)->sortkey - ((struct sortpair *)b)->sortkey;
}

