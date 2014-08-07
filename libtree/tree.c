/* This version fails when the "correct" slot for a body has been
 filled by an oob-adjusted body, i.e., if the last bits look like
 ...0, ...0, ...1;
   or
 ...0, ...2, ...3, ...3, ...3, ...4;
 In the first case, the second ..0 is put in the ..1 slot.  The next body
 fails.  In the second case, the second ..3 goes in slot 1, the third ..3
 goes in slot 4.  The ..4 fails.

 Is this sufficiently rare?  Would fixing it really buy us anything?  If
 there's a non-negligible probability of finding this failure mode, then
 there's also a non-negligible probability of running completely out 
 of bits, no?

 The failure mode is a failed assertion in Enter.  The msg file will look
 something like this:

WARNING: tree.c(352) in make_tree:
Negative rshiftbits, i=12550!
bkey: 10777777562
getkey(bp-1): 10777777562
getkey(bp-2): 10777777561
WARNING: tree.c(377) in make_tree:
Identical keys: 10777777562, parent = 1077777756 L     -1 0x06 0, using slot 0
WARNING: tree.c(377) in make_tree:
Identical keys: 10777777562, parent = 1077777756 L     -1 0x07 0, using slot 3
WARNING: tree.c(352) in make_tree:
Negative rshiftbits, i=12552!
bkey: 10777777563
getkey(bp-1): 10777777562
getkey(bp-2): 10777777562
ERROR: Assertion (Find(tp, key) == NULL) failed: file "tree.c", line 58

*/
/*
 * Copyright 1992 Michael S. Warren and John K. Salmon.  All Rights Reserved.
 */

#if defined(__hpux) && !defined(__GNUC__)
/* It's really +Oregionsched that causes grief on the spp2000 (May, 1997),
   but this is the easiest way to get around it. */
#pragma OPT_LEVEL 1
#endif

#define TREEdotC
#include <stddef.h>
#include <stdio.h>
#include "Assert.h"
#include "verify.h"
#include "key.h"
#include "tree.h"
#include "pqsort.h"
#include "chn.h"
#include "Malloc.h"
#include "gc.h"
#include "mpmy.h"
#include "Msgs.h"
#include "error.h"
#include "protos.h"
#include "timers.h"

Timer_t MakeTreeTm;
Timer_t SharedCellsTm;
Timer_t SharedCellsCommTm;
Timer_t SharedCellsWaitTm;
Counter_t SharedCnt;

/* Local functions */
static void make_tree (tree_t *tp);
static void DoSharedCells(tree_t *tp);

/* Local variables */
static Key_t hibound, lobound;

static hcellptr
NewCell(tree_t *tp, Key_t key)
/* Like Enter, but don't fill anything in. */
{
    hcellptr *np = tp->htab+KeyAndInt(key,tp->hash_mask);
    hcellptr new = ChnAlloc(&tp->hcellchn);

#ifndef __OPTIMIZE__
    if (Find(tp, key)) Error("NewCell found existing key %s\n", PrintKey(key));
#endif
    new->next = *np;
    new->key = key;
    *np = new;
    return new;
}

hcellptr
Enter(tree_t *tp, Key_t key, void *c, hcell_type type)
{
    hcellptr np = NewCell(tp, key);
    np->ptr = c;
    np->type = type;
    return(np);
}

static void CofmFromDaughNOOP(hcell *hp, hcell **daugh, sortresult_t *bodies){}
static void *CellFromCofmNOOP(void *cofm){return NULL;}

void 
SetupTree(tree_t *tp, int ndim, int bodysz, int cellsz, 
	  int cell2sz, int cell4sz, int tbodysz, 
	  int cofmdatasz, Key_t (*GetKey)(const void *), 
	  float (*GetCost)(const void *),
	  void (*CofmFromDaugh)(hcell *, hcell **, sortresult_t *),
	  void *(*CellFromCofm)(void *cofm),
	  int (*CellSz)(void *p))
{
    tp->body_sz = bodysz;
    tp->tbody_sz = tbodysz;
    tp->cell_sz = cellsz;
    tp->cell2_sz = cell2sz;
    tp->cell4_sz = cell4sz;
    tp->cell8_sz = cell4sz;
    tp->cofmdata_sz = cofmdatasz;
    tp->ndim = ndim;
    tp->GetKey = GetKey;
    tp->GetCost = GetCost;
    tp->hash_mask = HASH_MASK;
    tp->CofmFromDaugh = (CofmFromDaugh) ? CofmFromDaugh : CofmFromDaughNOOP;
    tp->CellFromCofm = (CellFromCofm) ? CellFromCofm : CellFromCofmNOOP;
    tp->CellSz = CellSz;
}

void 
SetupTree8(tree_t *tp, int ndim, int bodysz, int cellsz, 
	   int cell2sz, int cell4sz, int cell8sz, int tbodysz, 
	   int cofmdatasz, Key_t (*GetKey)(const void *), 
	   float (*GetCost)(const void *),
	   void (*CofmFromDaugh)(hcell *, hcell **, sortresult_t *),
	   void *(*CellFromCofm)(void *cofm),
	   int (*CellSz)(void *p))
{
    tp->body_sz = bodysz;
    tp->tbody_sz = tbodysz;
    tp->cell_sz = cellsz;
    tp->cell2_sz = cell2sz;
    tp->cell4_sz = cell4sz;
    tp->cell8_sz = cell8sz;
    tp->cofmdata_sz = cofmdatasz;
    tp->ndim = ndim;
    tp->GetKey = GetKey;
    tp->GetCost = GetCost;
    tp->hash_mask = HASH_MASK;
    tp->CofmFromDaugh = (CofmFromDaugh) ? CofmFromDaugh : CofmFromDaughNOOP;
    tp->CellFromCofm = (CellFromCofm) ? CellFromCofm : CellFromCofmNOOP;
    tp->CellSz = CellSz;
}


void
BuildTree(tree_t *tp, sortresult_t *bodies)
{
    tp->bodies = bodies;
    pqsort(tp->bodies, tp->GetCost, tp->GetKey);
    tp->htab = Calloc(HASH_TABLE_SIZE, sizeof(hcellptr));

#define NALLOC(unitsz) (unitsz)? (CHUNKSZ-1)/(unitsz) + 1 : CHUNKSZ/8
    ChnInit(&tp->cellchn, tp->cell_sz, NALLOC(tp->cell_sz), Realloc_f);
    ChnInit(&tp->cell2chn, tp->cell2_sz, NALLOC(tp->cell2_sz), Realloc_f);
    ChnInit(&tp->cell4chn, tp->cell4_sz, NALLOC(tp->cell4_sz), Realloc_f);
    ChnInit(&tp->cell8chn, tp->cell8_sz, NALLOC(tp->cell8_sz), Realloc_f);
    ChnInit(&tp->tbodychn, tp->tbody_sz, NALLOC(tp->tbody_sz), Realloc_f);
    ChnInit(&tp->cofmchn, tp->cofmdata_sz, NALLOC(tp->cofmdata_sz), Realloc_f);
    ChnInit(&tp->hcellchn, sizeof(hcell), NALLOC(sizeof(hcell)), Realloc_f);
#undef NALLOC

    make_tree(tp);
    Msgf(("Made Tree, nobj=%d\n", tp->bodies->nobj));
}

void
FreeTree(tree_t *tp)
{
    Free(tp->htab);
    tp->htab = NULL;
    ChnTerminate(&tp->cellchn);
    ChnTerminate(&tp->cell2chn);
    ChnTerminate(&tp->cell4chn);
    ChnTerminate(&tp->cell8chn);
    ChnTerminate(&tp->tbodychn);
    ChnTerminate(&tp->cofmchn);
    ChnTerminate(&tp->hcellchn);
}

static void
LoadSharedNode(tree_t *tp, Key_t key, hcell_type type, void *ptr)
{
    /* load a new node in the tree and fill in empty parent cells */
    /* until we hit something that's already in the tree. */
    int ndim = tp->ndim;
    int nsub1 = (1<<ndim)-1;
    int lobits;
    hcell *hp, *parent;

    hp = Enter(tp, key, ptr, type);
    Msgf(("LoadSharedNode: %s\n", hcellPrint(hp)));
    lobits = KeyAndInt(key, nsub1);
    key = KeyRshift(key, ndim);
    while ((parent = Find(tp, key)) == NULL) {
	parent = Enter(tp, key, NULL, 0);
	Set_Sub_Flag(parent, 1<<lobits);
	parent->type |= SHARED;
	Msgf(("Loaded parent: %s\n", hcellPrint(parent)));
	lobits = KeyAndInt(key, nsub1);
	key = KeyRshift(key, ndim);
    }
    Set_Sub_Flag(parent, 1<<lobits);
}

void Finish(tree_t *tp, Key_t k)
{
    hcell *daughters[MAXNSUB];
    hcell *hp;
    int nsub = 1<<(tp->ndim);
    unsigned int sub_flags, i;

    hp = Find(tp, k);
    /* Msgf(("TF: %s\n", hcellPrint(hp))); */
    sub_flags = Sub_Flags(hp);
    if( sub_flags == 0 )
	return;

    k = KeyLshift(k, tp->ndim);
    for(i=0; i<nsub; i++, sub_flags>>=1){
	if( sub_flags&1 ){
	    daughters[i] = Find(tp, KeyOrInt(k, i));
	}else{
	    daughters[i] = NULL;
	}
    }
    hp->ptr = ChnAlloc(&tp->cofmchn);
    tp->CofmFromDaugh(hp, daughters, tp->bodies);
    for (i = 0; i < nsub; i++) {
	if (daughters[i] && Sub_Flags(daughters[i])) {
	    void *cp = tp->CellFromCofm(daughters[i]->ptr);
	    ChnFree(&tp->cofmchn, daughters[i]->ptr);
	    daughters[i]->ptr = cp;
	}
    }
}

void exch_bounds(int ndim, Key_t hikey, Key_t lokey, Key_t *hiboundp, Key_t *loboundp){
    int nproc, procnum, up, down;
    int shift;

    procnum = MPMY_Procnum();
    nproc = MPMY_Nproc();
#if GRAYDECOMP
    up = Gcup(procnum, nproc);
    down = Gcdown(procnum, nproc);
#else
    up = (procnum==nproc-1)? -1 : procnum+1;
    down = (procnum==0)? -1 : procnum-1;
#endif
    shift = ((KEYBITS-1)/ndim)*ndim;

#if GRAYDECOMP
    if( parity(procnum) )
#else
    if( procnum&1 )
#endif
    {
	if( up >= 0 ){
	    Msgf(("exch up to %d\n", up));
	    MPMY_Shift(up, hiboundp, sizeof(Key_t), &hikey, sizeof(Key_t), NULL);
	}else{
	    /* Set hibound to 0177777... */
	    Key_t tmp;
	    tmp = KeyLshift(KeyInt(1), shift);
	    *hiboundp = KeyOr(tmp, KeySub(tmp, KeyInt(1)));
	}
	if( down >= 0 ){
	    Msgf(("exch down to %d\n", down));
	    MPMY_Shift(down, loboundp, sizeof(Key_t), &lokey, sizeof(Key_t), NULL);
	}else{
	    /* Set lobound to 010000... */
	    *loboundp = KeyLshift(KeyInt(1), shift);
	}
    }else{
	if( down >= 0 ){
	    Msgf(("exch down to %d\n", down));
	    MPMY_Shift(down, loboundp, sizeof(Key_t), &lokey, sizeof(Key_t), NULL);
	}else{
	    /* Set lobound to 010000... */
	    *loboundp = KeyLshift(KeyInt(1), shift);
	}
	if( up >= 0 ){
	    Msgf(("exch up to %d\n", up));
	    MPMY_Shift(up, hiboundp, sizeof(Key_t), &hikey, sizeof(Key_t), NULL);
	}else{
	    Key_t tmp;
	    /* Set hibound to 0177777... */
	    tmp = KeyLshift(KeyInt(1), shift);
	    *hiboundp = KeyOr(tmp, KeySub(tmp, KeyInt(1)));
	}
    }
}

static void
make_tree(tree_t *tp)
{
    void *bp;
    int i, nobj, ndim;
    int rshiftbits, nsub1;
    int rshift_lwm;
    void *oldptr;
    Key_t bkey, lastbkey, lastckey;
    hcellptr parent, new;
    int body_sz;
    Key_t (*getkey)(const void *);
    Key_t hikey, lokey;
    Key_t hiboundcell, loboundcell;
    Key_t ckey;
    int sub, sub_last;
    int loopagain;
    int iam0 = 0;
    int iamlast = 0;

    nobj = tp->bodies->nobj;
    bp = tp->bodies->data;
    body_sz = tp->body_sz;
    getkey = tp->GetKey;
    ndim = tp->ndim;
    rshift_lwm = rshiftbits = ndim * ((KEYBITS-1)/ndim);
    nsub1 = (1<<ndim)-1;
    
    lokey = getkey(bp);
    hikey = getkey((char *)bp + (nobj-1)*body_sz);
	  
    exch_bounds(ndim, hikey, lokey, &hibound, &lobound);
#if GRAYDECOMP
    if (Gcdown(MPMY_Procnum(), MPMY_Nproc()) != -1) {
#else
    if (MPMY_Procnum() != 0) {
#endif
	lastbkey = lobound;
    } else {
	iam0 = 1;
	lastbkey = KeyInt(0);
    }
#if GRAYDECOMP
    if (Gcup(MPMY_Procnum(), MPMY_Nproc()) == -1) {
#else
    if (MPMY_Procnum() == MPMY_Nproc()-1) {
#endif
	iamlast = 1;
    }
    assert(KeyLE(lobound,lokey));
    assert(KeyGE(hibound,hikey));
    if( KeyEQ(lobound, lokey) )
	Warning("Unexplored territory:  lobound==lokey: %s\n", PrintKey(lokey));
    if( KeyEQ(hibound, hikey) )
	Warning("Unexplored territory:  hibound==hikey: %s\n", PrintKey(hikey));

    lastckey = KeyInt(1);
    /* Create the root. */
    Enter(tp, KeyInt(1), NULL, 0);
    Msgf(("load lev=%d, bkey=%s\n", rshiftbits/ndim, PrintKey(lastbkey)));
    Msgf((" bounds: %s ", PrintKey(lobound)));
    Msgf(("%s\n", PrintKey(hibound)));

    StartTimer(&MakeTreeTm);
    /* This loop goes one step too far to load a pseudo-body at the */
    /* position of the hibound.  I find this unaesthetic...  */
    for (i = 0; i <= nobj; bp = (char *)bp + body_sz, i++) {
	if (i < nobj) {
	    bkey = getkey(bp);
	    /* Msgf(("Computed key=%s\n", PrintKey(bkey))); */
	} else {
	    bp = NULL;
	    bkey = hibound;
	}
	    
	/* Msgf(("Loading %s, rshiftbits: %d\n", PrintKey(bkey), rshiftbits)); */
	/* First pop and finish all the cells that we are certain */
	/* are done, because this body is outside them. */
	ckey = KeyRshift(bkey, rshiftbits);
	loopagain = 0;
	while (KeyNEQ(ckey, lastckey) || (iamlast && i == nobj)) {
	    loboundcell = KeyRshift(lobound, rshiftbits);
	    hiboundcell = KeyRshift(hibound, rshiftbits);
	    /* Msgf(("FinishCheck: %10s ", PrintKey(ckey))); */
	    /* Msgf(("%10s ", PrintKey(lastckey))); */
	    /* Msgf(("%10s ", PrintKey(loboundcell))); */
	    /* Msgf(("%10s\n", PrintKey(hiboundcell))); */
	    if (loopagain && 
		(iam0 || KeyGT(lastckey, loboundcell)) && 
		(iamlast || KeyLT(lastckey, hiboundcell))) {
		Finish(tp, lastckey);
	    }
	    if (iamlast && KeyEQ(lastckey, KeyInt(1))) break;
	    loopagain = 1;
	    rshiftbits += ndim;
	    ckey = KeyRshift(bkey, rshiftbits);
	    lastckey = KeyRshift(lastbkey, rshiftbits);
	}

	if (iamlast && i == nobj) break;

	/* Now push enough extra cells to separate the new body */
	/* from lastbody. */
	parent = Find(tp, lastckey);
	oldptr = parent->ptr;
	/* Msgf(("Start to push cells with parent=%s\n", hcellPrint(parent))); */
	    
	for(;;){
	    rshiftbits -= ndim;
	    if( rshiftbits < 0 ){
		Warning("Negative rshiftbits, i=%d!\n", i);
		Msg_do("bkey: %s\n", PrintKey(bkey));
		Msg_do("getkey(bp-1): %s\n", PrintKey(getkey((char *)bp-body_sz)));
		Msg_do("getkey(bp-2): %s\n", PrintKey(getkey((char*)bp-2*body_sz)));
		rshiftbits = 0;
		parent = Find(tp, KeyRshift(bkey, ndim));
		oldptr = NULL;
	    }
	    lastckey = KeyRshift(lastbkey, rshiftbits);
	    ckey = KeyRshift(bkey, rshiftbits);
	    if( KeyNEQ(ckey, lastckey) )
		break;
	    sub = KeyAndInt(ckey, nsub1);
	    Set_Sub_Flag(parent, 1<<sub);
	    parent->ptr = NULL;
	    /* Msgf(("Parent: %s\n", hcellPrint(parent))); */

	    if( rshiftbits == 0 ){
		unsigned int emptysub, parentsub;
		parentsub = Sub_Flags(parent);
		for(emptysub=0; emptysub<=nsub1; emptysub++){
		    if( (parentsub & (1<<emptysub))==0 )
			break;
		}
		Warning("Identical keys: %s, ",
			PrintKey(bkey));
		Msg_do("parent = %s, using slot %d\n",
		       hcellPrint(parent), emptysub);
		if( emptysub > nsub1 )
		    Error("Totally out of bits!\n");
		ckey = KeyOrInt(KeyAndNotInt(bkey, nsub1) , emptysub);
		break;
	    }

	    parent = Enter(tp, ckey, NULL, 0);
	}

	if (oldptr) {
	    sub_last = KeyAndInt(lastckey, nsub1);
	    Set_Sub_Flag(parent, (1<<sub_last));
	    parent->ptr = NULL;
	    new = Enter(tp, lastckey, oldptr, 0);
	    /* Msgf(("Created new copy of lastbody: %s\n", hcellPrint(new))); */
	    /* Msgf(("New Parent: %s\n", hcellPrint(parent))); */
	}

	if (bp) {
	    /* Always, except for last pseudo-body */
	    sub = KeyAndInt(ckey, nsub1);
	    Set_Sub_Flag(parent, 1<<sub);
	    new = Enter(tp, ckey, bp, 0);
	    /* Msgf(("Created new body: %s\n", hcellPrint(new))); */
	    /* Msgf(("Body's Parent: %s\n", hcellPrint(parent))); */
	}
	lastbkey = bkey;
	lastckey = ckey;
	if (rshiftbits < rshift_lwm)
	    rshift_lwm = rshiftbits;
    }
    
    Msg("rshift_lwm", ("rshift_lwm = %d (%d levels remaining)\n", 
	   rshift_lwm, rshift_lwm/ndim));
    /* Now clean up all the cells that reach outside the proc domain. */
    StopTimer(&MakeTreeTm);
    DoSharedCells(tp);
}

static Stk brstk;

static int
FindBranches(tree_t *tp, hcell *hp)
{
    int sz;
    int proc;

    if (hp->ptr) {
	sz = Sub_Flags(hp) ? tp->cofmdata_sz : tp->tbody_sz;
	memcpy(StkPush(&brstk, sz), hp->ptr, sz);
	StkPushType(&brstk, hp->key, Key_t);
	proc = GetSource(hp->type);
	if (proc == -1) proc = MPMY_Procnum();
	StkPushType(&brstk, 
		    DATAHERE|NONLOCAL|PutSource2(proc,hp->type), 
		    hcell_type);
	Msgf(("Found a branch at %s, StkSz now %ld\n", hcellPrint(hp), StkSz(&brstk)));
	return 0;
    } else {
	Msgf(("Continue past %s\n", hcellPrint(hp)));
	return 1;
    }
}

static void
KeyBoundary(tree_t *tp, Key_t cell, Key_t *minp, Key_t *maxp)
{
    /* Find the minimum and maximum possible body-keys that can */
    /* inhabit cell */
    Key_t min, max, stop;
    
    stop = KeyLshift(KeyInt(1),  tp->ndim * ((KEYBITS-1)/tp->ndim));
    max = min = cell;
    while (KeyLT(min, stop)) {
	max = KeyLshift(max, tp->ndim);
	min = KeyLshift(min, tp->ndim);
	max = KeyOrInt(max, (1<<tp->ndim) - 1);
    }
    *minp = min;
    *maxp = max;
}

static int
CofmPre(tree_t *tp,hcell *hp)
{
    if (hp->ptr == NULL) {
	return 1;
    } else {
	return 0;
    }
}

static void
CofmPost(tree_t *tp, hcell *hp, hcell *daughters[])
{
    int i;
    int nsub = 1<<(tp->ndim);
    Key_t mink, maxk;
    
    KeyBoundary(tp, hp->key, &mink, &maxk);
    if (KeyGE(mink, lobound) && KeyLE(maxk, hibound)) {
	if (hp->ptr == NULL) hp->ptr = ChnAlloc(&tp->cofmchn);
	hp->type |= SHARED;
	Msg("sharedcells", ("CoFDaugh: %s\n", hcellPrint(hp)));
	tp->CofmFromDaugh(hp, daughters, tp->bodies);
	for (i = 0; i < nsub; i++) {
	    if (daughters[i] && Sub_Flags(daughters[i])) {
		void *cp = tp->CellFromCofm(daughters[i]->ptr);
		ChnFree(&tp->cofmchn, daughters[i]->ptr);
		daughters[i]->ptr = cp;
	    }
	}
    }
}

static void
DoSharedCells(tree_t *tp)
{
    typedef struct {Key_t lo; Key_t hi; int nbytes;} sbuf;
    int procnum = MPMY_Procnum();
    int doc, ret;
    MPMY_Status stat;

    StartTimer(&SharedCellsTm);
    /* Find the 'branches' */
    hcell *root = Find(tp, KeyInt(1));

    doc = ilog2(MPMY_Nproc());
    int ispowerof2 = (MPMY_Nproc() == (1 << doc));
    if (!ispowerof2) doc++;
    for (int chan = 0; chan < doc; chan++) {
	void *allbranches = NULL;
	sbuf in = {};
	sbuf out = {.lo = lobound, .hi = hibound};
	int sendproc = procnum ^ (1 << chan);
	Msg("sharedcells", ("sendproc is %d\n", sendproc));

	if (sendproc < MPMY_Nproc()) {
	    StkInitEz(&brstk);
	    Traverse(tp, root, FindBranches, NULL);
	    out.nbytes = StkSz(&brstk);
	    Msg("sharedcells", ("Found %d bytes of branches for channel %d\n", out.nbytes, chan));
	
	    StartTimer(&SharedCellsWaitTm);
	    MPMY_Shift(sendproc, &in, sizeof(sbuf), &out, sizeof(sbuf), &stat);
	    ret = MPMY_Count(&stat);
	    if (ret != sizeof(sbuf))
		Error("Shift failed, expected %ld got %d\n", sizeof(sbuf), ret);
	    StopTimer(&SharedCellsWaitTm);

	    if (KeyLT(in.lo, lobound)) lobound = in.lo;
	    if (KeyGT(in.hi, hibound)) hibound = in.hi;
	    Msg("sharedcells", ("bounds after channel %d: %s ", chan, PrintKey(lobound)));
	    Msg("sharedcells", ("%s\n", PrintKey(hibound)));
	    Msg("sharedcells", ("Receiving %d bytes of branches from channel %d\n", in.nbytes, chan));

	    StartTimer(&SharedCellsCommTm);
	    allbranches = Malloc(in.nbytes);
	    MPMY_Shift(sendproc, allbranches, in.nbytes, StkBase(&brstk), StkSz(&brstk), &stat);
	    ret = MPMY_Count(&stat);
	    if (ret != in.nbytes)
		Error("Shift failed, expected %d got %d\n", in.nbytes, ret);
	    StopTimer(&SharedCellsCommTm);
	    StkTerminate(&brstk);
	}

	/* Propagate data for non-power-of-two */
	StartTimer(&SharedCellsCommTm);
	int valproc = (MPMY_Nproc() - 1) & ~(1 << chan); /* last proc with valid data */
	for (int subchan = 0; subchan < chan; subchan++) {
	    int subproc = procnum ^ (1 << subchan);
	    if (subproc >= MPMY_Nproc()) continue;
	    Msg("sharedcells", ("valproc %d subproc %d\n", valproc, subproc));
	    if ((subproc > valproc && procnum < valproc) || (procnum > valproc && subproc < valproc)
		|| (procnum > valproc && subproc == valproc) || (subproc > valproc && procnum == valproc)) {
		if (procnum < subproc) {
		    Msg("sharedcells", ("subsend to %d\n", subproc));
		    MPMY_send(&in, sizeof(sbuf), subproc, 111+chan);
		    MPMY_send(allbranches, in.nbytes, subproc, 211+chan);
		} else {
		    out = in;
		    Msg("sharedcells", ("subrecv from %d\n", subproc));
		    MPMY_recvn(&in, sizeof(sbuf), subproc, 111+chan);
		    if (KeyLT(in.lo, lobound)) lobound = in.lo;
		    if (KeyGT(in.hi, hibound)) hibound = in.hi;
		    Msg("sharedcells", ("bounds after subchannel %d: %s ", subchan, PrintKey(lobound)));
		    Msg("sharedcells", ("%s\n", PrintKey(hibound)));
		    allbranches = Realloc(allbranches, in.nbytes+out.nbytes);
		    Msg("sharedcells", ("Receiving %d bytes of branches from subchannel %d\n", in.nbytes, subchan));
		    MPMY_recvn(allbranches+out.nbytes, in.nbytes, subproc, 211+chan);
		    in.nbytes += out.nbytes;
		}
	    }
	    valproc |= (1 << subchan);
	}
	StopTimer(&SharedCellsCommTm);

	if (in.nbytes > 0) {
	    /* Enter them in the tree, adding empties as necessary. */
	    Msg("sharedcells", ("Entering %d bytes of branches\n", in.nbytes));
	    AddCounter(&SharedCnt, in.nbytes);
	    StkInitWithData(&brstk, in.nbytes, Realloc_f, allbranches, _STK_DEFAULT_ALIGNMENT);
	    while (StkSz(&brstk)){
		hcell_type type = StkPopType(&brstk, hcell_type);
		Key_t key = StkPopType(&brstk, Key_t);
		int sz = Sub_Flags_Type(type) ? tp->cofmdata_sz : tp->tbody_sz;
		void *from = StkPop(&brstk, sz);
		if (GetSource(type) == procnum)
		    continue;
		void *to = Sub_Flags_Type(type) ? ChnAlloc(&tp->cofmchn) : ChnAlloc(&tp->tbodychn);
		memcpy(to, from, sz);
		LoadSharedNode(tp, key, type, to);
	    }
	    StkTerminate(&brstk);	/* Frees allbranches */
	    Traverse(tp, root, CofmPre, CofmPost);
	}
	Msg_flush();
    }
    if (!root->ptr) Error("root->ptr is NULL\n");

    if (Sub_Flags(root)) {
	void *cp = tp->CellFromCofm(root->ptr);
	ChnFree(&tp->cofmchn, root->ptr);
	root->ptr = cp;
    }
    Msg("sharedcells", ("DoSharedCells done: %s\n", hcellPrint(root)));
    StopTimer(&SharedCellsTm);
}

void Traverse(tree_t *tp, hcellptr pp,
	      int (*pref)(tree_t *, hcellptr), 
	      void (*postf)(tree_t *, hcellptr, hcellptr *)){
    if( pp == NULL )
	return;
    if( !pref || pref(tp, pp) ){
	hcellptr daughters[MAXNSUB];
	unsigned int nsub ;
	unsigned int childnum;
	unsigned int sub_flags;
	Key_t key;
    
	key = KeyLshift(pp->key, tp->ndim);
	nsub = 1 << tp->ndim;
	for (sub_flags=Sub_Flags(pp), childnum=0; 
	     childnum < nsub; 
	     childnum++, sub_flags>>=1) {
	    if (sub_flags & 1) {
		daughters[childnum] = Find(tp, KeyOrInt(key, childnum));
		if (daughters[childnum] == NULL) 
		    Error("Bad sub_flags for slot %d of %s\n", childnum, PrintKey(pp->key));
	    } else {
		daughters[childnum] = NULL;
	    }
	}
	/* Take this out of the loop above to improve memory latency in Find() */
	for (childnum = 0; childnum < nsub; childnum++) {
	    if (daughters[childnum]) Traverse(tp, daughters[childnum], pref, postf);
	}
	if (postf)
	    postf(tp, pp, daughters);
    }
}

/* Decoding the bits in a type can be a real headache! */
/* Use sprintf to a stat buffer to avoid the problem of FILE types */
char
*PrintType(hcell_type type)
{
    static char str[64];

    sprintf(str, "Nonlocal:%1d, DataHere:%1d,  KidsHere:%1d"
	    "Req: %1d, Shared:%1d, Subflags: %#x, Source: %d\n",
	    (type&NONLOCAL)>0, (type&DATAHERE)>0, (type&KIDSHERE),
	    (type&REQUESTED)>0, (type&SHARED)>0,
	    (type)&((1<<MAXNSUB) - 1),
	    GetSource(type));
	    
    return str;
}

char *hcellPrint(hcellptr p){
    char tc[5];
    static char str[128];
    hcell_type type;

    if( p == NULL ){
	sprintf(str, "(null)");
	return str;
    }
    type = p->type;

    if( type & NONLOCAL )
	tc[0] = 'N';
    else
	tc[0] = 'L';

    if( type & SHARED )
	tc[1] = 'S';
    else 
	tc[1] = ' ';

    if( type & KIDSHERE )
	tc[2] = 'K';
    else if( type & REQUESTED )
	tc[2] = 'R';
    else
	tc[2] = ' ';
    
    if( type & DATAHERE )
	tc[3] = 'D';
    else
	tc[3] = ' ';
    tc[4] = '\0';
    
    sprintf(str, "%10s %4s %3d 0x%02x %#lx",
	   PrintKey(p->key),
	   tc,
	   GetSource(p->type), Sub_Flags(p), (unsigned long)p->ptr);
    return str;
}

static char *(*PrintBody_s)(/* const cell * */);
static char *(*PrintCell_s)(/* const body * */);

static int prePrint(tree_t *tp, hcellptr p)
{
    if( p )
	Msg_do("%s %s\n", hcellPrint(p), 
	       Sub_Flags(p) ? PrintCell_s(p->ptr) : PrintBody_s(p->ptr));
    return (p!=0);
}

void
PrintTree(tree_t *tp,
	  char *(*PrintBody)(/* const cell * */),
	  char *(*PrintCell)(/* const body * */))
{
    Msg_do("PrintTree\n");
    PrintBody_s = PrintBody;
    PrintCell_s = PrintCell;
    Traverse(tp, Find(tp, KeyInt(1)), prePrint, NULL);
    Msg_flush();
}
