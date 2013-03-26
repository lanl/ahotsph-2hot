#ifndef chnDOTh
#define chnDOTh
#include <stddef.h>

/*
  CHN.C:  Routines for allocating and freeing memory
  in fixed length blocks.  CHN is mnemonic for either 'chain' or 'chunk'.
  The idea is that malloc is called infrequently to get large chunks
  which are broken down into smaller pieces which are chained together
  to make a freelist.  ChnAlloc and ChnFree are very fast O(1), and
  use memory efficiently even for small objects.  In contrast,
  malloc and free are often slow and typically waste several bytes per 
  allocated object.  The down side is that fragmentation problems can be 
  magnified.  Once a large chunk is allocated it never gets freed, even
  if all the objects in it have been freed.
   Entry points:
    void ChnInit(Chn *id, int sz, int nalloc, 
                 void *(realloc_like)(void *, size_t));
    void *ChnAlloc(Chn *id);
    void ChnFree(Chn *id, Void *p);
    void ChnFreeAll(Chn *id);
    void ChnTerminate(Chn *id);
    int ChnCheck(Chn *id);
    int ChnFreeCnt(Chn *id);
    int ChnAllocCnt(Chn *id);
  It wouldn't be hard to write "ChnCrunch" which would look for chunks
  that have been completely freed, and return them to the system.  This
  has limited utility, but it might be handy when running near the edge
  of memory.

  For diagnostic purposes, use ChnFreeCnt to get the length of the current
  freelist.  Note that more space will be dynamically allocated, so this
  is NOT an upper limit.  ChnAllocCnt is the count of how many chunks
  are currently allocated.
 */

typedef struct {
    int sz;
    int nalloc;
    void *free_list;
    void *first_chunk;
    int free_cnt;
    int nmalloced;		/* diagnostic purposes only */
    int tbl_sz;
    void *(*realloc_like)(void *, size_t);
} Chn;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
extern int ChnMoreMem(Chn *id);
extern void ChnInit(Chn *new, int sz, int nalloc, 
	     void *(*realloc_like)(void *, size_t));
extern void ChnTerminate(Chn *id);
extern void ChnFreeAll(Chn *id);
extern int ChnCheck(Chn *id);
#if !(__STDC_VERSION__ >= 199901L)
extern void *ChnAlloc(Chn *id);
extern void ChnFree(Chn *id, void *p);
extern int ChnFreeCnt(Chn *id);
extern int ChnAllocCnt(Chn *id);
extern size_t ChnUnitSz(Chn *id);
#endif
#ifdef __cplusplus
}
#endif /* __cplusplus */

#if (defined(__GNUC__) || defined(__ICC__)) || defined(CHNdotC)

#undef INLINE
#if (__STDC_VERSION__ >= 199901L) && !defined (CHNdotC)
#define INLINE inline
#else
#if (defined (__GNUC__) || defined(__ICC__)) && !defined (CHNdotC)
#define INLINE extern __inline__
#else
#define INLINE
#endif
#endif

#if defined(ChnNext) || defined(ChnMagic) || defined(ChnMAGIC)
 # error define conflict in __FILE__
#endif
#define ChnNext(x) (*(void**)(x))
#define ChnMagic(x) (*(int *)((void**)x+1))
#define ChnMAGIC 0x82345078

#ifndef assert
#include "Assert.h"
#endif

INLINE void *ChnAlloc(Chn *id)
{
    void *c;

    if (id->free_cnt <= 0) {
	if(ChnMoreMem(id))
	    return 0;
    }
    c = id->free_list;

    assert(c);
    assert(ChnMagic(c) == ChnMAGIC);

    id->free_cnt--;
    id->free_list = ChnNext(c);
    return (c);
}

INLINE void ChnFree(Chn *id, void *p)
{
    /* This assertion can give a false-error reading if the "user" */
    /* has stored the exact value ChnMAGIC in the right location. */
    /* assert(ChnMagic(p) != ChnMAGIC); */
    ChnNext(p) = id->free_list;
    ChnMagic(p) = ChnMAGIC;
    id->free_list = p;
    id->free_cnt++;
}

INLINE int ChnFreeCnt(Chn *id){
    return id->free_cnt;
}

INLINE int ChnAllocCnt(Chn *id){
    return id->nmalloced - id->free_cnt;
}

INLINE size_t ChnUnitSz(Chn *id){
  return id->sz;
}

#ifndef CHNdotC
#undef ChnNext
#undef ChnMagic
#undef ChnMAGIC
#endif
#undef INLINE

#endif /* __GNUC__ || CHNdotC */
#endif
