/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifdef __SRV__
#error This file should not use SRV
#endif

#include "error.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "Msgs.h"
#include "gccextensions.h"
#include "memfile.h"
#include "mpmy.h"
#include "mpmy_abnormal.h"
#include "protos.h"

#undef Error
#undef SinglError
#undef Warning
#undef SinglWarning
#undef SeriousWarning
#undef Shout
#undef SinglShout

static int recursion;

/* We call this SWError because of a namespace conflict when linking SDF
   into perl5.  error.h should do the switcheroo automatically... */
void SWError(const char *mesg, ...) {
    va_list alist;

    if (recursion++)
        MPMY_SystemAbort(); /* errors within errors.  A very bad sign */

    va_start(alist, mesg);
    fprintf(stderr, "ERROR: Proc %d (%s) ", MPMY_Procnum(), MPMY_Physnode());
    vfprintf(stderr, mesg, alist);
    fflush(stderr);
    va_end(alist);
    Msg_do("ERROR: ");
    va_start(alist, mesg);
    Msg_doalist(mesg, alist);
    va_end(alist);
    Msg_flush();
    MPMY_Abort();
}

/* Is this right?  Is it even possible?  Can I pass the same va_list
   to two different subroutines?  I can't use va_start and va_end because
   this isn't a varargs function!  It works in Msgs, so it ought to work
   here too.*/
void vError(const char *mesg, va_list alist) {
    if (recursion++)
        MPMY_SystemAbort(); /* errors within errors.  A very bad sign */

    fprintf(stderr, "ERROR: Proc %d (%s) ", MPMY_Procnum(), MPMY_Physnode());
    vfprintf(stderr, mesg, alist);
    fflush(stderr);

    Msg_do("ERROR: ");
    Msg_doalist(mesg, alist);
    Msg_flush();
    PrintMemfile();
    MPMY_Abort();
}

void SinglError(const char *mesg, ...) {
    va_list alist;

    if (recursion++)
        MPMY_SystemAbort();
    if (MPMY_Procnum() == 0) {
        va_start(alist, mesg);
        fprintf(stderr, "Single ERROR: ");
        vfprintf(stderr, mesg, alist);
        fflush(stderr);
        va_end(alist);
        Msg_do("Single ERROR: ");
        va_start(alist, mesg);
        Msg_doalist(mesg, alist);
        va_end(alist);
        PrintMemfile();
        Msg_flush();
    }
    MPMY_Abort();
}

void Warning(const char *mesg, ...) {
    va_list alist;

    if (recursion++) {
        --recursion;
        return;
    }
    Msg_do("WARNING: ");
    va_start(alist, mesg);
    Msg_doalist(mesg, alist);
    va_end(alist);
    Msg_flush();
    --recursion;
}

void SinglWarning(const char *mesg, ...) {
    va_list alist;

    if (recursion++) {
        --recursion;
        return;
    }
    if (MPMY_Procnum() == 0) {
        /* Since there's only one, it's safe to send it to stderr too... */
        va_start(alist, mesg);
        fprintf(stderr, "WARNING: (single): ");
        vfprintf(stderr, mesg, alist);
        fflush(stderr);
        va_end(alist);
        Msg_do("WARNING (Single mode): ");
        va_start(alist, mesg);
        Msg_doalist(mesg, alist);
        va_end(alist);
        Msg_flush();
    }
    --recursion;
}

/* This one goes to stderr, for all to see immediately */
void SeriousWarning(const char *mesg, ...) {
    va_list alist;

    if (recursion++) {
        --recursion;
        return;
    }
    va_start(alist, mesg);
    /* These should be put together so they are written atomically */
    fprintf(stderr, "WARNING: Proc %d ", MPMY_Procnum());
    vfprintf(stderr, mesg, alist);
    fflush(stderr);
    va_end(alist);
    Msg_do("WARNING: ");
    va_start(alist, mesg);
    Msg_doalist(mesg, alist);
    va_end(alist);
    Msg_flush();
    --recursion;
}

/* No "WARNING", no line numbers, etc... Just the arguments */
void Shout(const char *mesg, ...) {
    va_list alist;

    if (recursion++) {
        --recursion;
        return;
    }
    va_start(alist, mesg);
    vfprintf(stderr, mesg, alist);
    fflush(stderr);
    va_end(alist);
    va_start(alist, mesg);
    Msg_doalist(mesg, alist);
    va_end(alist);
    Msg_flush();
    --recursion;
}

void SinglShout(const char *mesg, ...) {
    va_list alist;

    if (recursion++) {
        --recursion;
        return;
    }
    if (MPMY_Procnum() == 0) {
        va_start(alist, mesg);
        vfprintf(stderr, mesg, alist);
        fflush(stderr);
        va_end(alist);
        va_start(alist, mesg);
        Msg_doalist(mesg, alist);
        va_end(alist);
        Msg_flush();
    }
    --recursion;
}
