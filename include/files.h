#ifndef _FILESdotH
#define _FILESdotH

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
int fexists(const char *name);
int fexists_and_unlink(const char *name);
/* These are advisory routines.  They don't actually do anything */
/* They just check for files named "_ForceOutput_" and "_ForceStop_" */
int ForceCheckpoint(void);
int ForceOutput(void);
int ForceStop(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

