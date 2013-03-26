#ifndef STKdotH
#define STKdotH

#include <stddef.h>
#include <string.h>
#include "Malloc.h"

/* A quick and dirty stack implementation.  Less functionality than */
/* the RMS obstack.  Is it faster?  */

/* We could lift the code from obstack to determine proper alignment,
   but I think it's easier to just -Define it in Make.$ARCH */

/* Use char * so we can do arithmetic without annoying casts. */
typedef struct stk{
    char *bottom;
    char *ptr;
    char *top;
    int growby;
    void *(*realloc_like)(void *, size_t);
    unsigned int align_mask;
} Stk;

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
/* StkInitWithData starts with the given DATA ptr */
void StkInitWithData(struct stk *s, size_t initial_sz,
		     void *(*realloc_like)(void *, size_t), void *data,
		     unsigned int alignment);

/* StkCopy makes a completely new stack, initialized with the data in FROM */

void StkCopy(struct stk *to, const struct stk *from);
/* StkInit initializes a stack. */
void StkInit(struct stk *s, size_t initial_sz, 
	     void *(*realloc_like)(void *, size_t), unsigned int alignment);

/* StkGrow adds at least nbytes to the available space in the stack. */
/* FOR INTERNAL USE ONLY */
void StkGrow(Stk *s, int nbytes);

/* StkTerminate frees the space and forget about it forever */
void StkTerminate(struct stk *s);

#if !(__STDC_VERSION__ >= 199901L)
/* StkInitEz chooses reasonable defaults. */
extern void StkInitEz(struct stk *s);

/* StkPush returns a pointer to room for nbytes at the top of the */
/* stack.  It's up to you to put something there. */
extern void *StkPush(struct stk *s, int nbytes);
/* The 'Align' version makes sure that the stack remains aligned.
   Use if WITH AND ONLY WITH StkPopAlign */
extern void *StkPushAlign(struct stk *s, int nbytes);

/* StkPushData does a push and a memcpy */
extern void *StkPushData(struct stk *s, void *p, int nbytes);

/* StkPop returns a pointer to the beginning of the nbytes at the */
/* top of the stack.  The data under those bytes is guaranteed to */
/* remain intact ONLY UNTIL THE NEXT StkPush, i.e., the data has been */
/* popped. */
extern void *StkPop(struct stk *s, int nbytes);
/* The 'Align' version makes sure that the stack remains aligned.  Use
   it IF AND ONLY IF with StkPushAlign was used.  NOTE:  you must use
   the StkPushAlign on the push BEFORE the one that requires alignment! */
extern void *StkPopAlign(struct stk *s, int nbytes);

/* StkPeek returns the same thing as StkPop, but it doesn't "pop" anything. */
/* The top of the stack is found by StkPeek(s, 0); */
extern void *StkPeek(const struct stk *s, int nbytes);

/* StkBase returns the current base of the stack.  Beware, it can move */
/* around!  It is only guaranteed to stay in place until the next StkPush */
extern void *StkBase(const struct stk *s);

/* StkSz is the size of the stack. */
extern size_t StkSz(const struct stk *s);

/* StkTop is easily constructed from the others, but we provide it anyway. */
/* It returns a pointer just past the end of the current stack. */
extern void *StkTop(const struct stk *s);

/* StkClear is equivalent to StkPop(s, StkSz(s)), but it returns void */
extern void StkClear(struct stk *s);

/* StkCrunch realloc's the stack so it doesn't use any more space */
/* than necessary.  Use it if you know you've reached the high-water-mark */
extern void *StkCrunch(struct stk *s);

/* Discover the alignment of a given stack.  I.e., how would it  */
extern int StkAlign(const struct stk *s, unsigned int nbytes);

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* Here's a convenient define for popping a known type. */
#define StkPopType(s, t) (*(t *)StkPop(s, sizeof(t)))

#define StkPushType(s, val, t) (*(t *)StkPush(s, sizeof(t)) = val)

/* StkAlign(n) tells you how many bytes will really be pushed if you */
/* do a StkPush(n) */
#ifdef STK_FORCE_ALIGNMENT
#define _STK_DEFAULT_ALIGNMENT STK_FORCE_ALIGNMENT
#else
#define _STK_DEFAULT_ALIGNMENT 1
#endif

/* Here are all the inlined definitions.  Non-inlined functions */
/* can go in stk.c */

#if (defined(__GNUC__) || defined(__ICC__)) || defined(STKdotC)

#undef INLINE
#if (__STDC_VERSION__ >= 199901L) && !defined (STKdotC)
#define INLINE inline
#else
#if (defined (__GNUC__) || defined(__ICC__)) && !defined (STKdotC)
#define INLINE extern __inline__
#else
#define INLINE
#endif
#endif

INLINE int StkAlign(const struct stk *s, unsigned int nbytes){
  return (nbytes + s->align_mask ) & ~s->align_mask;
}

INLINE void StkInitEz(struct stk *s){
    StkInit(s, 1024, Realloc_f, _STK_DEFAULT_ALIGNMENT);
}

INLINE void *StkPush(struct stk *s, int nbytes){
    char *ret;
    nbytes = StkAlign(s, nbytes);
    
    if( s->ptr + nbytes > s->top ){
	StkGrow(s, nbytes);
    }
    ret = s->ptr;
    s->ptr += nbytes;
    return ret;
}

INLINE void *StkPushData(struct stk *s, void *p, int nbytes){
    return memcpy(StkPush(s, nbytes), p, nbytes);
}

INLINE void *StkPop(struct stk *s, int nbytes){
     nbytes = StkAlign(s, nbytes);
#ifdef NO_CHECK
    return s->ptr -= nbytes;
#else
    s->ptr -= nbytes;
    if( s->ptr < s->bottom ){
	s->ptr += nbytes;	/* undo the damage */
	return NULL;
    }else
	return s->ptr;
#endif
}

INLINE void *StkPeek(const struct stk *s, int nbytes){
    return s->ptr - nbytes;
}

INLINE void StkClear(struct stk *s){
    s->ptr = s->bottom;
}

INLINE void *StkBase(const struct stk *s){
    return s->bottom;
}

INLINE size_t StkSz(const struct stk *s){
    return s->ptr - s->bottom;
}

INLINE void *StkTop(const struct stk *s){
    /* NOTE that StkTop is NOT s->top !! */
    return s->ptr;
}

#undef INLINE
#endif /* GNUC || STKdotC */

#endif /* STKdotH */
