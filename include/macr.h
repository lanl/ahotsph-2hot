#ifndef MacrDOTh
#define MacrDOTh

/*
 * Copyright 1991 Michael S. Warren and John K. Salmon.  All Rights Reserved.
 */

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
