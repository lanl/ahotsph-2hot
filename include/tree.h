/*
 * Copyright 1992 Michael S. Warren and John K. Salmon.  All Rights Reserved.
 */

#ifndef _TreeDOTh
#define _TreeDOTh

#include <string.h>
#include "timers.h"
#include "key.h"
#include "stk.h"
#include "chn.h"
#include "pqsort.h"

/* Why not use bitfields???  Can't the compiler can do as well as these */
/* ugly macros... */

/* These are Flags which get put in the type field */
/* If we try to go to NDIM>4, we'll run out of bits in the Source field! */
#define MAXNDIM 3
#define MAXNSUB (1 << MAXNDIM)
#define Sub_Flags_Type(x) ((x) & ((1<<MAXNSUB) - 1))
#define Sub_Flags(x) Sub_Flags_Type(x->type)
#define Set_Sub_Flag(x, b) ((x)->type |= (b))

/* 
  NONLOCAL tells us if the data in a cell must/has come from somewhere else.
  The GetSource macro is only valid for NONLOCAL cells.

  DATAHERE and REQUESTED are only meaningful for NONLOCAL.  If the data is
  HERE, we can trust sub_flags.  That doesn't mean the children are
  here, though.  If KIDSHERE is true then we can expect to be able to Find
  the kids.  Otherwise, the kids might be REQUESTED, in which case we
  shouldn't lodge another request.  They will arrive eventually.
  DATAHERE is used primarily in cofm.  KIDSHERE is used in walk.  This is
  still far too ugly!

  SHARED means the cell spans processor boundaries.  The sub_flags are
  valid.  Find'ing the daughters will return one or more 
  SHARED|NONLOCAL cells.

  Determine if a cell is terminal by looking at its sub_flags.  There is
  no way to determine if a NONLOCAL, !HERE cell is terminal without requesting
  it. 

  LOCKED tells us if the program has locked the cell in memory.  If not,
  it is a candidate for removal to conserve memory.  NOT IMPLEMENTED!
*/
#define NONLOCAL (1<<(0+MAXNSUB))
#define DATAHERE	(1<<(1+MAXNSUB))
#define KIDSHERE	(1<<(2+MAXNSUB))
#define REQUESTED (1<<(3+MAXNSUB))
#define SHARED (1<<(4+MAXNSUB))
#define LOCKED (1<<(5+MAXNSUB))
/* This says how many bits are used above.  It controls where the */
/* subflags go.  It should be one more than the last #define above...*/
#define NTYPEBITS 6

#define TreeDataOK(type) ((!(type&NONLOCAL))||(type&DATAHERE))
#define TreeKidsOK(type) ((!(type&NONLOCAL))||(type&KIDSHERE))
#define TreeShared(type) (type&SHARED)
#define TreeLocal(type) (!(type&NONLOCAL))

/* We offset by one to distinguish a source of zero from an unset source  */
#define MAXDOC 15
#define MAXSRCBITS (MAXDOC+1)
#define SOURCEBIT (NTYPEBITS+MAXNSUB)
#define SOURCEMASK (((1<<MAXSRCBITS)-1)<<SOURCEBIT)
#define GetSource(x) (((x) >> SOURCEBIT)-1)
#define PutSource(x) (((x)+1) << SOURCEBIT)
#define PutSource2(src,typ) (((typ)&(~SOURCEMASK)) | (((src)+1)<<SOURCEBIT))

#ifndef HASH_BITS
#define HASH_BITS 21
#endif

/* HASH_BITS must be less than the number of bits in a word */
#define HASH_MASK ((unsigned int)((1 << HASH_BITS) - 1))
#define HASH_TABLE_SIZE (1 << HASH_BITS)

/* How large (approximately) should should the blocks in which we */
/* allocate tbodies, cells, etc. be.  Larger leads to possible */
/* fragmentation, but smaller makes malloc work harder and makes */
/* malloc_print output somewhat unwieldy. */
#define CHUNKSZ 8384512

typedef int hcell_type;	/* Try to anticipate future need for more bits */

typedef struct hashref {
    struct hashref *next;
    Key_t key;
    void *ptr;
    hcell_type type;
} hcell, *hcellptr;
#define Type(h) ((h)->type)

typedef struct tree_s{
    sortresult_t *bodies;
    int body_sz;		/* entries in btab */
    int tbody_sz;		/* communicated bodies */
    int cell_sz;		/* communicated cells */
    int cell2_sz;		/* communicated cells */
    int cell4_sz;		/* communicated cells */
    int cell8_sz;		/* communicated cells */
    int cofmdata_sz;		/* intermediate cofm data objects */
    int ndim;
    Chn cellchn;
    Chn cell2chn;
    Chn cell4chn;
    Chn cell8chn;
    Chn tbodychn;
    Chn hcellchn;
    Chn cofmchn;
    hcellptr *htab;
    unsigned int hash_mask;
    Key_t (*GetKey)(const void *);
    float (*GetCost)(const void *);
    void (*CofmFromDaugh)(hcell *, hcell **);
    void *(*CellFromCofm)(void */*cofm*/);
    int (*CellSz)(void *cp);
} tree_t;

typedef void *(*cellfromcofm_t)(void *);
typedef void (*walkinit_t)(Stk *, Stk *);
typedef void (*macv_t)(void *, const hcell **, int *, int *, int);
typedef void (*inherit_t)(const void *, void *, hcell *);

/* Values to return from MACs */
#define MAC_ERROR -1
#define MAC_SPLIT_SINK 1
#define MAC_SPLIT_SRC 2
#define MAC_ACCEPT 3

/* in tree.c */
#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
void SetupTree(tree_t *tp, int ndim,
	       int bodysz, int cellsz, int cell2sz, int cell4sz,
	       int tbodysz, int cofmdatasz,
	       Key_t (*GetKey)(const void *), float (*GetCost)(const void *),
	       void (*CofmFromDaugh)(hcell *, hcell **),
	       void *(*CellFromCofm)(void *cofm),
	       int (*CellSz)(void *p));
void SetupTree8(tree_t *tp, int ndim,
		int bodysz, int cellsz, int cell2sz, int cell4sz, int cell8sz,
		int tbodysz, int cofmdatasz,
		Key_t (*GetKey)(const void *), float (*GetCost)(const void *),
		void (*CofmFromDaugh)(hcell *, hcell **),
		void *(*CellFromCofm)(void *cofm),
		int (*CellSz)(void *p));
void BuildTree(tree_t *treep, sortresult_t *bodies);
void FreeTree(tree_t *treep);
void LoadTNodes2(tree_t *tp, void *inbuf, void *endbuf);
hcell *Enter(tree_t *tp, Key_t key, void *c, hcell_type type);
char *PrintType(hcell_type type);
char *hcellPrint(hcellptr p);
void Traverse(tree_t *tp, hcellptr pp,
	      int (*pref)(tree_t *, hcellptr), 
	      void (*postf)(tree_t *, hcellptr, hcellptr *));
void *Tr0(tree_t *tp, Key_t key,
	  int (*pref)(hcellptr), void *(*postf)(hcellptr, void **));
void PrintTree(tree_t *tp,
	  char *(*PrintBody)(/* const cell * */),
	  char *(*PrintCell)(/* const body * */));

/* A few accumulators for diagnostic purposes */
extern Counter_t DeferCnt;	/* in walk.c */
extern Counter_t RequestCnt;	/* in walk.c */
extern Timer_t WalkDeferTm;	/* in walk.c */
extern Timer_t MakeTreeTm;	/* in tree.c */
extern Timer_t SharedCellsTm;	/* in tree.c */
extern Timer_t SharedCellsCommTm; /* in tree.c */
extern Counter_t SharedCnt;	/* in tree.c */

/* in walk.c */
void Walk(tree_t *srctp, tree_t *sinktp, int sinksz, 
	  walkinit_t init_src, macv_t MACv, inherit_t InheritSink);
void WalkInit(tree_t *srctp, tree_t *sinktp, int sinksz, 
	      walkinit_t init_src, macv_t MAC, inherit_t InheritSink);
void  WalkNT(tree_t *sinktp);
void WalkPoll(void);
void WalkTerminate(void);
int WalkFlushFreq(int flush_freq);
#ifdef __cplusplus
}
#endif /* __cplusplus */

/* Here are all the inlined definitions.  Non-inlined functions */
/* are in tree.c */

#undef INLINE
#if (__STDC_VERSION__ >= 199901L) && !defined (TREEdotC)
#define INLINE inline
#else
#if (defined (__GNUC__) || defined(__ICC__)) && !defined (TREEdotC)
#define INLINE extern __inline__
#else
#define INLINE
#endif
#endif

#if defined(__GNUC__) || defined(TREEdotC)


/* This version of Find is improved in two ways:
   1) hcells don't move around.  Once found, you can rely on the pointer's
   continued validity.
   2) Relinking the found pointer to the head of the chain costs 4 operations.
   It's cheap enough that we don't need a separate Find_And_Relink.
*/
   
INLINE hcellptr
Find(tree_t *tp, Key_t key)
{
    hcellptr *firstp = tp->htab+KeyAndInt(key, tp->hash_mask);
    hcellptr np;
    hcellptr *prevnext;

    /* Look for a match.  Don't go past the end of the chain. Remember */
    /* the 'next' that points to 'np' */
    for(np = *(prevnext = firstp); 
	np && KeyNEQ(np->key, key) ; 
	np = *(prevnext = &np->next) ){}

    if( np ){
	/* Put the 'found' np at the head of the chain.  */
	*prevnext = np->next;
	np->next = *firstp;
	*firstp = np;
    }
    return(np);
}

INLINE hcellptr
Findx(tree_t *tp, Key_t key)
{
    hcellptr *firstp = tp->htab+KeyAndInt(key, tp->hash_mask);
    hcellptr np;
    hcellptr *prevnext;

    /* Look for a match.  Don't go past the end of the chain. Remember */
    /* the 'next' that points to 'np' */
    for(np = *(prevnext = firstp); 
	np && KeyNEQ(np->key, key) ; 
	np = *(prevnext = &np->next) ){}

    if( np ){
	/* Put the 'found' np at the head of the chain.  */
	*prevnext = np->next;
	np->next = *firstp;
	*firstp = np;
    }
    return(np);
}

#undef INLINE
#endif /* __GNUC__ || TREEdotC */

#endif /* _TreeDOTh */
