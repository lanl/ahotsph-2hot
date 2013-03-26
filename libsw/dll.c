/* DLL: doubly linked lists.  

   Support:
     insertion either before or after a given element (Above, Below).
     deletion.
     traversal in either direction. (Up, Down) 

   Use a vertical metaphor for mnemonics.  You can grow the data
   structure up, down, or outward from the middle.  It makes
   absolutely no difference.

   The implementation uses two 'sentinels'.  The one above the topmost
   element is called 'Sup' and the one below the lowest element is
   called 'Inf'.  These are good places to start and end 'for' loops, e.g.,
   
   for(p=DllTop(dll); p!=DllInf(dll); p = DllDown(dll, p))...

   or
   
   for(p=DllBottom(dll); p!=DllSup(dll); p = DllUp(dll, p))...

   Space for 'user' data of size 'sz' is allocated with each element.
   This is ideal if you want to allocate associate a fixed size object
   with each list element.  If you want more flexibility, there's
   nothing stopping you from making that fixed size object a void*
   that points to something else.
   
*/

#include "chn.h"
#include "dll.h"
#include "Malloc.h"

/* Initializing is a two step process because we have to do some
 funny stuff to get a chain allocating chunks just slightly bigger than
 what the user asks for.   The resulting chain can be ChnTerminated
 at the caller's leisure.  */
void DllCreateChn(Chn *chn, int sz, int n){
    n+=2;
    if( sz > sizeof(int) )
	sz -= sizeof(int);
    ChnInit(chn, sizeof(Dll_elmt)+sz, n, Realloc_f);
}

void DllCreate(Dll *dll, Chn *chn){
    dll->chn = chn;
    dll->Sup.down = &dll->Inf;
    dll->Sup.up = NULL;
    dll->Inf.down = NULL;
    dll->Inf.up = &dll->Sup;
    dll->length = 0;
}

/* Terminate a dll */
void DllTerminate(Dll *dll){
    /* Hmmm.  There's really nothing to do since the user is now
       empowered to free the chain.  We can't even do a FreeAll because
       there might be other stuff (other DLL's?) using the chain. */
}

/* Insert closer to the Top */
Dll_elmt *DllInsertAbove(Dll *dll, Dll_elmt *down){
    Dll_elmt *new = ChnAlloc(dll->chn);
    Dll_elmt *up = down->up;
    
    if( new == NULL ){
	Shout("ChnAlloc returns null in DllInsertAbove\n");
	return NULL;
    }
    dll->length++;
    new->up = up;
    new->down = down;
    down->up = new;
    up->down = new;
    return new;
}

/* Insert closer to the bottom */
Dll_elmt *DllInsertBelow(Dll *dll, Dll_elmt *up){
    Dll_elmt *new = ChnAlloc(dll->chn);
    Dll_elmt *down = up->down;
    
    if( new == NULL ){
	Shout("ChnAlloc returns null in DllInsertBelow\n");
	return NULL;
    }
    dll->length++;
    new->up = up;
    new->down = down;
    down->up = new;
    up->down = new;
    return new;
}

/* These two should be inlined, with __inline__ ... */
/* Insert at the bottom */
Dll_elmt *DllInsertAtBottom(Dll *dll){
    return DllInsertAbove(dll, &(dll->Inf));
}

/* Insert at the top */
Dll_elmt *DllInsertAtTop(Dll *dll){
    return DllInsertBelow(dll, &(dll->Sup));
}

/* These three are VERY similar, but they are useful for traversals iin 
   different directions.  Otherwise the caller needs to save the 'up' or
   'down' element */
/* Delete an entry.  Return nothing. */
void DllDelete(Dll *dll, Dll_elmt *old){
    Dll_elmt *up = old->up;
    Dll_elmt *down = old->down;

    dll->length--;
    up->down = down;
    down->up = up;
    ChnFree(dll->chn, old);
}

/* Delete an entry.  Return the entry that used to be above it. */
Dll_elmt *DllDeleteUp(Dll *dll, Dll_elmt *old){
    Dll_elmt *up = old->up;
    Dll_elmt *down = old->down;

    dll->length--;
    up->down = down;
    down->up = up;
    ChnFree(dll->chn, old);
    return up;
}

/* Delete an entry.  Return the entry that used to be below it. */
Dll_elmt *DllDeleteDown(Dll *dll, Dll_elmt *old){
    Dll_elmt *up = old->up;
    Dll_elmt *down = old->down;

    dll->length--;
    up->down = down;
    down->up = up;
    ChnFree(dll->chn, old);
    return down;
}

/* The next two have two plausible returns: the item directly above or
  below the original position of the mover.  Rather than confuse
  things, I won't return either, and leave it up to the caller to keep
  track of whatever he wants. */

/* Extract the 'mover' and place it immediately below 'up'.
   Like DllDelete, followed by DllInsertBelow, but preserve the
   data in the object. */
void DllMoveBelow(Dll *dll, Dll_elmt *mover, Dll_elmt *up){
  Dll_elmt *down;
  /* Extract the mover */
  mover->down->up = mover->up;
  mover->up->down = mover->down;
  /* Now insert it below up */
  down = up->down;
  mover->up = up;
  mover->down = down;
  down->up = mover;
  up->down = mover;
}

/* Extract the 'mover' and place it immediately above 'down'.
   Like DllDelete, followed by DllInsertAbove, but preserve the
   data in the object. */
void DllMoveAbove(Dll *dll, Dll_elmt *mover, Dll_elmt *down){
  Dll_elmt *up;
  /* Extract the mover */
  mover->down->up = mover->up;
  mover->up->down = mover->down;
  /* Now insert it above down */
  up = down->up;
  mover->up = up;
  mover->down = down;
  down->up = mover;
  up->down = mover;
}

/* These should be inlined... */
/* Move to bottom */
void DllMoveToBottom(Dll *dll, Dll_elmt *mover){
    DllMoveAbove(dll, mover, &(dll->Inf));
}

/* Move to top */
void DllMoveToTop(Dll *dll, Dll_elmt *mover){
    DllMoveBelow(dll, mover, &(dll->Sup));
}

/* These are generally 'inlined' with appropriate #defines in dll.h.
   They're simple enough to allow use of the pre-processor instead of
   __inline__. */
#undef DllLength
int DllLength(Dll *dll){
    return dll->length;
}

#undef DllSup
Dll_elmt *DllSup(Dll *dll){
    return &dll->Sup;
}

#undef DllInf
Dll_elmt *DllInf(Dll *dll){
    return &dll->Inf;
}

/* The highest 'real' element */
#undef DllTop
Dll_elmt *DllTop(Dll *dll){
    return dll->Sup.down;
}

/* The lowest 'real' element */
#undef DllBottom
Dll_elmt *DllBottom(Dll *dll){
    return dll->Inf.up;
}

#undef DllData
void *DllData(Dll_elmt *elmt){
    return &elmt->stuff;
}

#undef DllUp
Dll_elmt *DllUp(Dll_elmt *elmt){
    return elmt->up;
}

#undef DllDown
Dll_elmt *DllDown(Dll_elmt *elmt){
    return elmt->down;
}

    
