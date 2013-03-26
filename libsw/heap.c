/* Super-fast priority queue. ?  Completely inlined by gcc. */
/* Assume that each pointer points at */
/* a key, AND whatever else the caller is interested in keeping. */

#ifndef assert
#include "Assert.h"
#endif
#include <stddef.h>
#include <float.h>
#include "Malloc.h"
#define HEAPdotC
#include "heap.h"

#if !defined(HeapKey) || !defined(HeapLeft)
 # error Heap macros undefined
#endif

/* These save us from an extra comparison inside our loops */
const float HeapMinf = -FLT_MAX;
const float HeapInf = FLT_MAX;

/* I wonder if some const decls would be correct? */
void HeapInit(Heap *hp, unsigned int sz){
    assert(sz > 0);
    hp->arr = Malloc( (sz+1)*sizeof(*(hp->arr)) );
    assert(hp->arr);
    hp->sz = sz;
    hp->cnt = 1;
    hp->arr[0] = &HeapInf;
}
 
void HeapTerminate(Heap *hp){
    Free((void *)hp->arr);
    hp->arr = NULL;
    hp->sz = hp->cnt = 0;
}

int HeapIsBad(const Heap *hp){
    unsigned int i, ri, li;
    const float **arr = hp->arr;

    for( i=1, li=2, ri=3;
	li < hp->cnt;
	i++, li = HeapLeft(i), ri = li+1){
	if( HeapKey(i) < HeapKey(li) )
	    return li;
	if( ri < hp->cnt && HeapKey(i) < HeapKey(ri) )
	    return ri;
    }
    return 0;
}

#ifdef STANDALONE
#include <stdio.h>

float data[] = {5., 1., 3., 2., 7., 3., 4.};

void HeapPrint(Heap *hp){
    unsigned int i;
    for(i=1; i<hp->cnt; i++){
	printf("%d %g\n", i, *(hp->arr[i]));
    }
}

int main(int argc, char **argv){
    Heap H;
    float *top;
    unsigned int i;
    unsigned int ndata;

    HeapInit(&H, 2);
    ndata = sizeof(data)/sizeof(*data);
    for(i=0; i<ndata; i++){
	HeapPush(&H, &data[i]);
	printf("Inserted %g\n", data[i]);
	HeapPrint(&H);
    }

    for(i=0; i<ndata; i++){
	HeapPop(&H, &top);
	printf("popped %g\n", *top);
	HeapPrint(&H);
    }
    return EXIT_SUCCESS;
}
#endif
/* Undefine our private macros because this file might be #include'ed */
