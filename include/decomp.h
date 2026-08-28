/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef _DecompDOTh
#define _DecompDOTh
#include "pqsort.h"
#include "timers.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Timer_t DecompTm;
extern Timer_t DecompWaitTm;
extern Timer_t DecompCommTm;
void SetupDecomp(sortresult_t *decompp,
                 float (*weight)(const void *),
                 Key_t (*getkey)(const void *));
int DestDecomp(void *p);
void FinishDecomp(void);
void *SaveDecomp19(void);
void SetDecomp19(void *ptr);
void ClearDecomp19(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
