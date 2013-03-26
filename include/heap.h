/* Super-fast priority queue. ?  Completely inlined by gcc. */
/* Assume that each pointer points at */
/* a key, AND whatever else the caller is interested in keeping. */
#ifndef HEAPdotH
#define HEAPdotH


typedef struct{
    const float **arr;
    unsigned int sz;
    unsigned int cnt;
} Heap;

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
extern void HeapInit(Heap *hp, unsigned int initial_nelem);
extern void HeapTerminate(Heap *hp);
extern void HeapPush(Heap *hp, const float *ptr);
extern void HeapPop(Heap *hp, const float **keyp);
extern const float *HeapPeek(const Heap *hp);
extern const float **HeapBase(const Heap *hp);
extern const float **HeapEnd(const Heap *hp);
extern unsigned int HeapCnt(const Heap *hp);
extern int HeapIsBad(const Heap *hp);
extern const float HeapMinf;
extern const float HeapInf;
#ifdef __cplusplus
}
#endif /* __cplusplus */

/* This is an attempt to get the inline functions into the .h file */
/* without having to maintain two source files.  */
#if (defined(__GNUC__) || defined(__ICC__)) || defined(HEAPdotC)

#ifndef assert
#include "Assert.h"
#endif
#include <stddef.h>
#include <float.h>
#include "Malloc.h"

#undef INLINE
#if (defined (__GNUC__) || defined(__ICC__)) && !defined (HEAPdotC)
#define INLINE extern __inline__
#else
#define INLINE
#endif

#ifndef NULL
#define NULL (void *)0
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#if defined(HeapKey) || defined(HeapParent) || defined(HeapLeft)
 # error Problems with conflicting definitions in heap.h
#endif
#define HeapKey(i) (*arr[i])
#define HeapParent(i) (i>>1)
#define HeapLeft(i) (i<<1)
/* Use left+1 for right */

INLINE void HeapPush(Heap *hp, const float *ptr){
    const float **arr = hp->arr;
    const float *tmp;
    unsigned int i = hp->cnt++;
    unsigned int pi;

    if( i == hp->sz ){
	unsigned int newsz = (hp->sz)<<1;
	arr = hp->arr = Realloc(hp->arr, (newsz+1)*sizeof(*(hp->arr)));
	assert(hp->arr);
	hp->sz = newsz;
    }

    arr[i] = ptr;
    pi = HeapParent(i);
    while( HeapKey(pi) <= HeapKey(i) ){
	tmp = arr[pi];
	arr[pi] = arr[i];
	arr[i] = tmp;
	i = pi;
	pi = HeapParent(i);
    }
}

INLINE void HeapPop(Heap *hp, const float **keyp){
    const float **arr = hp->arr;
    const float *save;
    unsigned int i, li, ri, ix, n;
    float kl, kr, ksave, kx;

    *keyp = arr[1];
    n = --hp->cnt;
    assert( n>0 );
    i = 1;
    li = 2;
    ri = 3;
    save = arr[n];
    ksave = *save;
    arr[n] = &HeapMinf;
    while( li < n ){
	kl = HeapKey(li);
	kr = HeapKey(ri);
	if( kl >= kr ){
	    ix = li;
	    kx = kl;
	}else{
	    ix = ri;
	    kx = kr;
	}
	if( ksave >= kx ){
	    break;
	}

	/* Move ix up the heap */
	arr[i] = arr[ix];
	i = ix;
	li = HeapLeft(ix);
	ri = li+1;
    }
    arr[i] = save;
}

INLINE const float *HeapPeek(const Heap *hp){
    return hp->arr[1];
}

INLINE const float **HeapBase(const Heap *hp){
    return &hp->arr[1];
}

INLINE const float **HeapEnd(const Heap *hp){
    return &hp->arr[hp->cnt];
}

INLINE unsigned int HeapCnt(const Heap *hp){
    return hp->cnt-1;
}

/* Undefine our private macros */
#ifndef HEAPdotC
#undef HeapKey
#undef HeapParent
#undef HeapLeft
#endif /* HEAPdotC */
#undef INLINE

#endif /* __GNUC__ || HEAPdotC */
#endif /* already included */
