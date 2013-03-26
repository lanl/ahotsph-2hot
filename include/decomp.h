#ifndef _DecompDOTh
#define _DecompDOTh
#include "pqsort.h"
#include "timers.h"

#ifdef __cplusplus
extern "C"{
#endif

extern Timer_t DecompTm;
extern Timer_t DecompWaitTm;
extern Timer_t DecompCommTm;
void SetupDecomp(sortresult_t *decompp, float (*weight)(const void *), 
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
