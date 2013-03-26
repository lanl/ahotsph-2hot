#ifndef _PollDOTh
#define _PollDOTh

#include "timers.h"

#ifdef __cplusplus
extern "C"{
#endif
void PollSetup(void put(void *buf, int size), int max_size, int tag);
void Poll(int tag);
void PollWait(MPMY_Comm_request req, int tag);
void PollUntilDone(int tag);
extern Timer_t PollWaitTm;

#ifdef __cplusplus
}
#endif

#endif /* _PollDOTh */
