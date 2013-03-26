#ifndef _MPMYAbnormalDOTh_
#define _MPMYAbnormalDOTh_
#include "gccextensions.h"

/* Abhndlrs are void functions of void.  Use them as arguments to 
   MPMY_OnAbnormal. */
typedef void (*Abhndlr)(void);

#ifdef __cplusplus
extern "C"{
#endif

/* MPMY_Abort will execute MPMY_RaiseAbnormal(SIGABRT) and then call
   MPMY_SystemAbort.  Don't be surprised if the OnAbnormal functions
   themselves call MPMY_SystemAbort first, though. */
void MPMY_Abort(void) __NORETURN__;

extern int MPMY_Abnormal_signum;
extern int MPMY_stop_abnormal_processing;
void MPMY_RaiseAbnormal(int sig); /* fake a signal of type 'sig' */

/* Push a handler to the stack that will be executed on abnormal termination.
   Some useful arguments to consider are:
   malloc_print();
   PrintMemfile();
   Msg_flush();
   MPMY_abchdir();  (with MPMY_abchdir_arg set beforehand)
   MPMY_SystemAbort();
   MPMY_SystemExit(); (with MPMY_exit_arg set beforehand)

   They are called in the reverse chronological order from the order that
   they were requested by MPMY_OnAbnormal, so later functions
   might 'override' earlier ones.  This isn't perfect, but it's better than
   the monolithic handler we had before.  The handler can
   find out which signal is being handled by looking at 
     int MPMY_current_signal ;

   As a final twist, it is possible to bail out of the stack and just
   return from the handler immediately by setting:
     int MPMY_stop_abnormal_processing;
   to non-zero.
*/
void MPMY_OnAbnormal(Abhndlr hndlr);

/* Really, truly, abort().  Now! */
void MPMY_SystemAbort(void) __NORETURN__;

/* Really, truly, exit(MPMY_exit_arg).  Now! */
extern int MPMY_exit_arg;
void MPMY_SystemExit(void) __NORETURN__;	/* An Abhndlr */

/* Do a mkdir/chdir to the directory named by MPMY_abchdir_arg. 
   This can be extremely useful before dumping core... */
extern char MPMY_Abchdir_arg[];
void MPMY_Abchdir(void);	/* An Abhndlr */

/* Announce (Shout) that we are handling a signal.  Try not to repeat
 yourself if it's been said already... */
void MPMY_Abannounce(void);

/* Arrange to crash and burn if n seconds elapse before Reset is called */
void MPMY_TimeoutSet(int n);
void MPMY_TimeoutReset(int n);
void MPMY_TimeoutCancel(void);

/* Not for public use. */
void _MPMY_setup_absigs(void);
#ifdef __cplusplus
}
#endif
#endif /* _MPMYAbnormalDOTh_ */
