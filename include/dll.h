#ifndef _DLLdotH_
#define _DLLdotH_

#include "chn.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct Dll_elmt_s{
    struct Dll_elmt_s *up, *down;
    void *stuff[1];		/* Will be alloc'ed to something else! */
    /* DON'T PUT ANYTHING HERE!!! It will be silently mangleded */
} Dll_elmt;

typedef struct {
    Dll_elmt Sup, Inf;
    Chn *chn;
    int length;			/* why not? */
} Dll ;

/* Create a Chain suitable for passing to DllCreate */
void DllCreateChn(Chn *chn, int sz, int n);
/* Create a new Dll */
void DllCreate(Dll *dll, Chn *chn);
/* Terminate a Dll */
void DllTerminate(Dll *dll);
/* Return a new element just above 'DOWN' */
Dll_elmt *DllInsertAbove(Dll *dll, Dll_elmt *down);
/* Return a new element just below 'UP' */
Dll_elmt *DllInsertBelow(Dll *dll, Dll_elmt *up);
/* Delete OLD.  Return nothing. */
void DllDelete(Dll *dll, Dll_elmt *old);
/* Delete OLD.  Return the entry that used to be above it. */
Dll_elmt *DllDeleteUp(Dll *dll, Dll_elmt *old);
/* Delete OLD.  Return the entry that used to be below it. */
Dll_elmt *DllDeleteDown(Dll *dll, Dll_elmt *old);
/* Extract the 'mover' and place it immediately above 'down'.
   Like DllDelete, followed by DllInsertAbove, but preserve the
   data in the object. */
void DllMoveAbove(Dll *dll, Dll_elmt *mover, Dll_elmt *down);
/* Extract the 'mover' and place it immediately below 'up'.
   Like DllDelete, followed by DllInsertBelow, but preserve the
   data in the object. */
void DllMoveBelow(Dll *dll, Dll_elmt *mover, Dll_elmt *up);

/* These would require __inline__ to be done properly. */
/* Insert a new element at the bottom, equivalent to:
   DllInsertAbove(dll, DllInf(dll)); */
Dll_elmt *DllInsertAtBottom(Dll *dll);
/* Insert a new element at the top, equivalent to:
   DllInsertBelow(dll, DllSup(dll)); */
Dll_elmt *DllInsertAtTop(Dll *dll);
/* Move to bottom */
void DllMoveToBottom(Dll *dll, Dll_elmt *mover);
/* Move to top */
void DllMoveToTop(Dll *dll, Dll_elmt *mover);

/* Should we bother with __inline__.  These are simple enough that #define 
 is sufficient. */

/* How many elements? */
int DllLength(Dll *dll);
#define DllLength(dll) ((dll)->length)
/* One past the topmost 'user' element */
Dll_elmt *DllSup(Dll *dll);
#define DllSup(dll) (&((dll)->Sup))
/* One below the lowest 'user' element */
Dll_elmt *DllInf(Dll *dll);
#define DllInf(dll) (&((dll)->Inf))
/* The highest 'user' element */
Dll_elmt *DllTop(Dll *dll);
#define DllTop(dll) ((dll)->Sup.down)
/* The lowest 'user' element */
Dll_elmt *DllBottom(Dll *dll);
#define DllBottom(dll) ((dll)->Inf.up)
/* The 'user' data */
void *DllData(Dll_elmt *elmt);
#define DllData(elmt) ((void *)((elmt)->stuff))
/* The next elements, both up and down */
Dll_elmt *DllUp(Dll_elmt *elmt);
#define DllUp(elmt) ((elmt)->up)
Dll_elmt *DllDown(Dll_elmt *elmt);
#define DllDown(elmt) ((elmt)->down)

#ifdef __cplusplus
}
#endif

#endif /* _DLLdotH_ */
