/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef _newCommDOTh
#define _newCommDOTh

#include "timers.h"
#include "tree.h"

#define NLHISTLEN 16 /* a logarithmic histogram of msg lens */
extern Counter_t NLBytesCnt;
extern Counter_t NLHist[NLHISTLEN];

void NLHistEnable(void);
void NLSetup(tree_t *tp, int pktsz);
void NLTerminate(tree_t *tp);
void NLRequest(hcell *);
void NLPoll(void);
void NLPollTillDone(void);

#endif
