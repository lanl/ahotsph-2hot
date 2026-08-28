/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef _FILESdotH
#define _FILESdotH

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
int fexists(const char *name);
int fexists_and_unlink(const char *name);
/* These are advisory routines.  They don't actually do anything */
/* They just check for files named "_ForceOutput_" and "_ForceStop_" */
int ForceCheckpoint(void);
int ForceOutput(void);
int ForceStop(void);
int ForceDie(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
