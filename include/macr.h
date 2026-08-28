/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef MacrDOTh
#define MacrDOTh

#include <stdio.h>

#include "error.h"

/*
 * A bunch of cover macros, which fail (informatively) on errors
 *  Fopen does some special handling of '-' also.
 */
#define Fopen(fp, file, mode)                    \
    if (file[0] == '-' && file[1] == '\0') {     \
        if (mode[0] == 'w')                      \
            fp = stdout;                         \
        else                                     \
            fp = stdin;                          \
    } else if ((fp = fopen(file, mode)) == NULL) \
    Error("fopen: fname \"%s\"\n", file)

#define Fwrite(ptr, size, nitems, stream)            \
    if (fwrite(ptr, size, nitems, stream) != nitems) \
    Error("fwrite: sz=%ld, nitem=%ld\n", (long)size, (long)nitems)

#define Fread(ptr, size, nitems, stream)            \
    if (fread(ptr, size, nitems, stream) != nitems) \
    Error("fread: sz=%ld, nitem=%ld\n", (long)size, (long)nitems)

#define Fseek(stream, offset, ptrname)  \
    if (fseek(stream, offset, ptrname)) \
    Error("fseek: off=%ld\n", (long)offset)

#define Fclose(stream)  \
    if (fclose(stream)) \
    Error("fclose\n")

#define Fflush(stream)  \
    if (fflush(stream)) \
    Error("fflush\n")

#endif
