/*
 * Copyright (c) 1989, 1990 John Salmon & Mike Warren, 
 *	California Institute of Technology, Pasadena, CA.	
 */

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
 *  Entry points:
 *   void ChnInit(Chn *id, int sz, int nalloc, 
 *                void *(realloc_like)(void *, size_t));
 *   void *ChnAlloc(Chn *id);
 *   void ChnFree(Chn *id, Void *p);
 *   void ChnFreeAll(Chn *id);
 *   void ChnTerminate(Chn *id);
 *   int ChnCheck(Chn *id);
 *  It wouldn't be hard to write "ChnCrunch" which would look for chunks
 *  that have been completely freed, and return them to the system.  This
 *  has limited utility, but it might be handy when running near the edge
 *  of memory.
 */

#define CHNdotC
#include <stddef.h>
#include "Assert.h"
#include "chn.h"
#include "Msgs.h"
#include "malloc.h"

/* The Chain macro is used to chain together 'freed' nodes. */
/* We write a pointer on top of the first word and a magic */
/* number on top of the second word.  */
#if !defined(ChnNext) || !defined(ChnMagic) || !defined(ChnMAGIC)
 # error Chn macros undefined
#endif

/* STOLEN FROM OBSTACK.C */
#ifdef __STDC__
#define PTR_INT_TYPE ptrdiff_t
#else
#define PTR_INT_TYPE long
#endif
/* Determine default alignment.  */
struct fooalign {char x; double d;};
#define DEFAULT_ALIGNMENT  \
  ((PTR_INT_TYPE) ((char *)&((struct fooalign *) 0)->d - (char *)0))
/* END OF STOLEN CODE */
#define ALIGNMENT_MASK (DEFAULT_ALIGNMENT-1)
#define Align(x) ((x)+ALIGNMENT_MASK)&(~(ALIGNMENT_MASK))
#define ChnHDRSZ (Align(sizeof(int)+sizeof(void *)))

void ChnInit(Chn *new, int sz, int nalloc, 
	     void *(*realloc_like)(void *, size_t))
{
    if(sz < ChnHDRSZ)
	sz = ChnHDRSZ;
    
    new->sz = Align(sz);
    Msgf(("chn=%#lx, sz=%d, new->sz=%d\n", (unsigned long)new, sz, new->sz));
    new->nalloc = nalloc;
    new->free_list = NULL;
    new->free_cnt = 0;
    new->nmalloced = 0;
    new->realloc_like = realloc_like;
    new->first_chunk = NULL;
}

void ChnFreeAll(Chn *id)
{
    void *chunk, *nextchunk;

    Msgf(("ChnFreeAll(%#lx)\n", (unsigned long)id));
    for(chunk=id->first_chunk; chunk ; chunk = nextchunk){
	nextchunk = ChnNext(chunk);
	Msgf(("ChnFree(%#lx)\n", (unsigned long)chunk));
	id->realloc_like(chunk, 0);
    }
    id->free_cnt = 0;
    id->free_list = NULL;
    id->nmalloced = 0;
}

void ChnTerminate(Chn *id)
{
    ChnFreeAll(id);
    id->realloc_like = NULL;
}

int ChnCheck(Chn *id)
/* Return < 0 if there is something wrong with the freelist. */
/* Return 0 if all is ok. */
{
    int i=0;
    void *p = id->free_list;

    while(p){
	if(ChnMagic(p) != ChnMAGIC)
	    return -1;
	if(i++ < id->free_cnt)
	    return -2;
	p = ChnNext(p);
    }
    return 0;
}

/*
 *  ChnMoreMem:  grab some more memory for a Chn.
 */
int ChnMoreMem(Chn *id)
{
    void *newchunk;
    int i, sz, nnew;
    char *p;
    
    if( id->free_cnt == 0 && id->free_list != NULL ){
	SeriousWarning("Impossible situation in ChnMoreMem, freecnt=0, free_list = %#lx\n", (unsigned long)id->free_list);
	malloc_print();
    }

    nnew = id->nalloc;
    sz = id->sz;
    while( (newchunk=id->realloc_like(NULL, nnew*sz + ChnHDRSZ)) == 0 && 
	  nnew > 0 ) 
	nnew /= 2;
    if(newchunk == 0){
	return -1;
    }
    Msgf(("ChnMoreMem(chn=%#lx, newsz=%ld, address=%#lx)\n", 
	  (unsigned long)id, (unsigned long)(nnew*sz+ChnHDRSZ), 
	  (unsigned long)newchunk));

    id->nmalloced += nnew;
    ChnNext(newchunk) = id->first_chunk;
    ChnMagic(newchunk) = ChnMAGIC;
    id->first_chunk = newchunk;
    newchunk = (char *)newchunk + ChnHDRSZ;

    p = (char *)newchunk;
    for(i=0; i<nnew-1; i++){
	ChnNext(p) = (int *)(p + sz);
	ChnMagic(p) = ChnMAGIC;
	p += sz;
    }
    ChnNext(p) = id->free_list;
    ChnMagic(p) = ChnMAGIC;
    id->free_list = newchunk;
    id->free_cnt += nnew;
    return 0;
}

/*
 * end of: CHN.C
 */
