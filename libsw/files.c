/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

/* Some common routines for dealing with files. */
#include <fcntl.h>
#include <unistd.h>

#include "Malloc.h"
#include "mpmy.h"
#include "protos.h"

int fexists(const char *name) {
    int fd, ret;

    ret = 0;
    if (MPMY_Procnum() == 0) {
        /* We could call stat, but then we'd have to deal with the */
        /* complications of different flavors of struct stat on different */
        /* machines...Yuck. */
        if ((fd = open(name, O_RDONLY)) >= 0) {
            close(fd);
            ret = 1;
        }
    }
    MPMY_Combine(&ret, &ret, 1, MPMY_INT, MPMY_SUM);
    return ret;
}

int fexists_and_unlink(const char *name) {
    int fd, ret;

    ret = 0;
    if (MPMY_Procnum() == 0) {
        /* We could call stat, but then we'd have to deal with the */
        /* complications of different flavors of struct stat on different */
        /* machines...Yuck. */
        if ((fd = open(name, O_RDONLY)) >= 0) {
            close(fd);
            ret = 1;
        }
        unlink(name);
    }
    MPMY_Combine(&ret, &ret, 1, MPMY_INT, MPMY_SUM);
    return ret;
}

int ForceCheckpoint(void) {
    return fexists_and_unlink("_ForceCheckpoint_") || fexists("_ForceStop_");
}

int ForceOutput(void) { return fexists_and_unlink("_ForceOutput_"); }

int ForceStop(void) { return fexists_and_unlink("_ForceStop_"); }

int ForceDie(void) { return fexists_and_unlink("_ForceDie_"); }
