#ifndef MPMY_timeDOTh
#define MPMY_timeDOTh

/* some simple system-dependent routines to facilitate timing */

#define MPMY_CPU_TIME 1
#define MPMY_WC_TIME 2

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
void *MPMY_CreateTimer(int type);
int MPMY_StartTimer(void *);
    int MPMY_CopyTimer(void *, void *);
int MPMY_StopTimer(void *);
int MPMY_ClearTimer(void *);
double MPMY_ReadTimer(void *);
int MPMY_DestroyTimer(void *);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
