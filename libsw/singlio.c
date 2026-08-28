/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include "singlio.h"

#include <stdarg.h>
#include <stdio.h>

#include "Msgs.h"
#include "mpmy.h"
#include "protos.h"

static int singl_auto_flush = 1;

int singlAutoflush(int new) {
    int ret = singl_auto_flush;
    singl_auto_flush = new;
    return ret;
}

int singlPrintf(const char *fmt, ...) {
    va_list ap;
    int ret;

    if (MPMY_Procnum() != 0)
        return 0;
    va_start(ap, fmt);
    ret = vfprintf(stdout, fmt, ap);
    va_end(ap);
    if (singl_auto_flush)
        fflush(stdout);
    return ret;
}

void singlFflush(void) {
    if (MPMY_Procnum() != 0)
        return;
    fflush(stdout);
}
