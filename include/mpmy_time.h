/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef MPMY_timeDOTh
#define MPMY_timeDOTh

/* some simple system-dependent routines to facilitate timing */

#define MPMY_CPU_TIME 1
#define MPMY_WC_TIME 2

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
void *MPMY_CreateTimer(int type);
int MPMY_StartTimer(void *);
int MPMY_CopyTimer(void *, void *);
int MPMY_StopTimer(void *);
int MPMY_AccumTimer(void *, double dt);
int MPMY_ClearTimer(void *);
double MPMY_ReadTimer(void *);
int MPMY_DestroyTimer(void *);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
