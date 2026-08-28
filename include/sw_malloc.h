/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef _MallocDOTh
#define _MallocDOTh
#include <stddef.h>

#if defined(RENAME_MALLOC) || (!defined(USE_SYSTEM_MALLOC) && !defined(REPLACE_MALLOC))
/* There are really three possibilities:
 a) the system malloc works perfectly and we aren't interested in the
    debugging features of libsw/malloc.c:  use -DUSE_SYSTEM_MALLOC.
    You will also need to do this if brk and/or sbrk are broken on your
    machine.
 b) we want libsw/malloc.c, but other system "utilities" (e.g., crt0.o)
    rely on or configure undocumented features of the system malloc.
    Therefore we can't simply drop in a replacement for
    malloc/calloc, etc.:  use -DRENAME_MALLOC (or nothing. This is the default)
 c) it's ok to just drop in a replacement for the system malloc:
    use -DREPLACE_MALLOC.

    Note that it is probably only necessary to modify CFLAGS in
    libsw/Make.$(ARCH) because "user-level" memory allocation is done through
    Malloc.[ch] anyway.
*/
#undef malloc
#undef calloc
#undef realloc
#undef free
#define malloc sw_malloc
#define calloc sw_calloc
#define realloc sw_realloc
#define free sw_free
#endif /* RENAME_MALLOC */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
extern int malloc_debug(int);
extern int malloc_verify(void);
extern void malloc_print(void);
extern char malloc_errstring[];
extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void *realloc(void *, size_t);
extern void free(void *);
extern size_t malloc_avail(void);
extern size_t malloc_used(void);
extern size_t malloc_heapsz(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
