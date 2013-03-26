#ifndef _newCommDOTh
#define _newCommDOTh

#include "tree.h"
#include "timers.h"

#define NLHISTLEN 16		/* a logarithmic histogram of msg lens */
extern Counter_t NLBytesCnt;
extern Counter_t NLHist[NLHISTLEN];

void NLHistEnable(void);
void NLSetup(tree_t *tp, int pktsz);
void NLTerminate(tree_t *tp);
void NLRequest(hcell *);
void NLPoll(void);
void NLPollTillDone(void);

#endif
