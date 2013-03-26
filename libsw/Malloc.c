/*
 * Copyright 1991 Michael S. Warren and John K. Salmon.  All Rights Reserved.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include "Msgs.h"
#include "Malloc.h"
#include "malloc.h"

#define WARNSIZEINITIAL (1024L*1024*1024*2)
static size_t WarnSize = WARNSIZEINITIAL;

#include "error.h"

static void Error_and_mprint(const char *fmt, ...){
    va_list alist;
    malloc_print();
    va_start(alist, fmt);
    vError(fmt, alist);
    va_end(alist);
}

/* Try to do this without the typedef Error_t!!! */
static Error_t reporter = Error_and_mprint;

Error_t MallocHandler(Error_t new){
     Error_t ret = reporter;
     reporter = new;
     return ret;
}

void
xFree(void *ptr, const char *file, int lineno)
{
    Msgf(("%s(%d): f(%#lx)\n", file, lineno, (unsigned long)ptr));
    if( ptr != (void *)0 )
	free(ptr);
}

void *
xMalloc(size_t size, const char *file, int lineno)
{
    void *ptr;

    Msgf(("%s(%d): m(%lu)=", file, lineno, (unsigned long)size));
    if (size > WarnSize){
	Shout("Large Malloc Warning, size %ld\n", (long)size);
	WarnSize *=2;
    }
    if (size == 0) {
	Msgf(("0x0"));
	return (void *)0;
    }
    ptr = malloc(size);
    if (ptr == (void *)0 && reporter) {
	reporter("%s(%d) Malloc(%ld) failed\n", file, lineno, (long)size);
    }
    Msgf(("%#lx\n", (unsigned long)ptr));
    return(ptr);
}


void *
xRealloc(void *ptr, size_t size, const char *file, int lineno)
{
    void *p1 = ptr;

    Msgf(("%s(%d): r(%#lx,%lu)=", file, lineno, (unsigned long)ptr, 
	  (unsigned long)size));
    if (size > WarnSize){
	Shout("Large Realloc Warning, size %ld\n", (long)size);
	WarnSize *= 2;
    }

    if( ptr == (void *)0 ){
	Msgf(("-> malloc\n"));
	return Malloc(size);
    }
    if( size == 0 ){
	Msgf(("-> free\n"));
	Free(ptr);
	return (void *)0;
    }
    ptr = realloc(ptr, size);
    if (ptr == (void *)0 && reporter) {
	reporter("%s(%d): Realloc(%p, %ld) failed\n", file, lineno, p1, (long)size);
    }
    Msgf(("%#lx\n", (unsigned long)ptr));
    return(ptr);
}

void *
xCalloc(size_t n, size_t s, const char *file, int lineno)
{
    void *ptr;

    Msgf(("%s(%d): c(%lu,%lu)=", file, lineno, 
	  (unsigned long)n, (unsigned long)s));
    if (n*s > WarnSize){
	Shout("Large Calloc Warning, size %ld\n", (long)n*s);
	WarnSize *= 2;
    }
    if( n==0 || s==0 ){
	Msgf(("0x0\n"));
	return (void *)0;
    }
    ptr = calloc(n,s);
    if (ptr == (void *)0 && reporter) {
	reporter("%s(%d): Calloc(%ld) failed\n", file, lineno, (long)n);
    }
    Msgf(("%#lx\n", (unsigned long)ptr));
    return(ptr);
}

/* Use these, for example, when you pass a ptr-to-function arg */
/* to another function.  They call the macro version, which prints */
/* a message.  If you don't intend to ever use the old K&R /lib/cpp */
/* pre-processor, then these are superfluous.  You could pass, e.g. */
/* foo(Realloc), and you could call these functions, e.g., Realloc */
/* and everything would be fine...   */

void *Realloc_f(void *ptr, size_t size){
    return Realloc(ptr, size);
}

void *Malloc_f(size_t n){
    return Malloc(n);
}

void *Calloc_f(size_t n, size_t s){
    return Calloc(n, s);
}

void Free_f(void *p){
    Free(p);
}
/* void MallocOnNULLReturn(void (*printf_like)(const char *fmt, ...)) */
