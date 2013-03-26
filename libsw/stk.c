#define STKdotC
#include "Assert.h"
#include "stk.h"
#include "error.h"
/* Everything else is inlined in stk.h */

/* Any non-inlined definitions can go here. */

void StkInit(struct stk *s, size_t initial_sz, 
	void *(*realloc_like)(void *, size_t), unsigned int alignment){
    s->growby = initial_sz;
    s->realloc_like = realloc_like;
    s->ptr = s->bottom = realloc_like(NULL, initial_sz);
    s->top = s->bottom + initial_sz;
    if( alignment == 0 ) 
      alignment = _STK_DEFAULT_ALIGNMENT;
    assert( (alignment & (alignment-1)) == 0 );
    s->align_mask = alignment - 1;
}

void StkInitWithData(struct stk *s, size_t initial_sz,
		     void *(*realloc_like)(void *, size_t), void *data, 
		     unsigned int alignment){
    s->growby = initial_sz;
    s->realloc_like = realloc_like;
    s->bottom = data;
    s->top = s->ptr = s->bottom + initial_sz;
    if( alignment == 0 ) 
      alignment = _STK_DEFAULT_ALIGNMENT;
    assert( (alignment & (alignment-1)) == 0 );
    s->align_mask = alignment - 1;
}

void StkCopy(struct stk *to, const struct stk *from){
    size_t initial_sz;

    to->growby = from->growby;
    to->realloc_like = from->realloc_like;
    initial_sz = StkSz(from);
    to->bottom = to->realloc_like(NULL, initial_sz);
    to->top = to->ptr = to->bottom + initial_sz;
    memcpy(to->bottom, from->bottom, initial_sz);
}

void StkTerminate(struct stk *s){
    (*s->realloc_like)(s->bottom, 0); /* free */
    s->ptr = s->bottom = s->top = NULL;
}

void StkGrow(Stk *s, int nbytes){
    size_t newsz = (s->ptr - s->bottom) + nbytes + s->growby;
    char *newbottom = (*s->realloc_like)(s->bottom, newsz);
    if( newbottom == NULL ){
	Error("Can't realloc to sz=%ld in StkGrow\n", newsz);
    }
    s->ptr = (s->ptr - s->bottom) + newbottom;
    s->top = newbottom + newsz;
    s->bottom = newbottom;
}

void *StkCrunch(struct stk *s){
    size_t newsz = s->ptr - s->bottom;
    char *newbottom = (*s->realloc_like)(s->bottom, newsz);
    s->ptr = s->top = newsz + newbottom;
    s->bottom = newbottom;
    return newbottom;
}
