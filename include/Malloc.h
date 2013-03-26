#ifndef _MAlloCDOTh
#define _MAlloCDOTh

#include <stddef.h>
#include "error.h"

#ifdef __cplusplus
extern "C"{
#endif
/* Set the default behavior of the Malloc family when encountering */
/* errors, e.g., NULL returns, etc. */
Error_t MallocHandler(Error_t);

void xFree (void *ptr, const char *file, int lineno);
void *xMalloc (size_t, const char *file, int lineno);
void *xRealloc (void *ptr, size_t, const char *file, int lineno);
void *xCalloc (size_t, size_t, const char *file, int lineno);
/* No extra parens needed ?? */
void Free_f (void *ptr);
void *Malloc_f (size_t);
void *Realloc_f (void *ptr, size_t);
void *Calloc_f (size_t, size_t);
#ifdef __cplusplus
}
#endif
/* Some pre-processors are so stupid that they will convert
  foo(Realloc); 
into
  foo(xRealloc(,,__FILE__, __LINE__));
Thus, we can't use Realloc as function name.
*/
#define Free(p) xFree(p, __FILE__, __LINE__)
#define Malloc(n) xMalloc(n, __FILE__, __LINE__)
#define Calloc(n,s) xCalloc(n, s, __FILE__, __LINE__)
#define Realloc(p, n) xRealloc(p, n, __FILE__, __LINE__)

#endif /* _MAllocDOTh */
