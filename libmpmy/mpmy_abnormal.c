/* Yet another attempt to rationalize the abnormal termination of
   parallel programs.

   Issues to consider:
     - Do core files exist, and are they useful.
     - Do we need to chdir before dumping a core file.
     - Exactly how do we go about dumping a core file.
     - Do we want to do anything else before quitting.
     - Sometimes we get the most information when we just return
       from a signal handler (delta).
     - Parts will need to be over-ridden by mpmy_<paros>.c, but
       duplication of code should be minimized.
     - include files, values of signals, etc. are all wildly system
       specific.
*/

#include <signal.h>
#include <stdlib.h>
#include "singlio.h"
#include "Msgs.h"
#include "mpmy_abnormal.h"

#ifndef MPMY_ABORT
void MPMY_Abort(void){
    MPMY_RaiseAbnormal(SIGABRT);
    MPMY_SystemAbort();
}
#endif

#ifdef _AIX
    /* dje says AIX dumps core fine, but the debuggers are confused by
       the signal handlers. Another way of achieving this might be
       to set absiglist and MPMY_HAVE_ABSIGLIST in the mpmy_paros.c file. */
#define NO_SIGNALS
#endif

#if !defined( HAVE_ABSIG_LIST ) && !defined(NO_SIGNALS)
static int absiglist[]={
#if 0 && defined(SIGABRT)			/* ANSI */
/* DO NOT TRAP SIGABRT!  Just leave it completely alone.  We studiously
   avoid calling abort in our code, so we should just let SIGABRT do its
   normal, unmodified thing. */
SIGABRT, 
#endif
#ifdef SIGFPE			/* ANSI */
SIGFPE, 
#endif
#ifdef SIGIILL			/* ANSI */
SIGILL,
#endif
#ifdef SIGIOT			/* ANSI */
SIGIOT,
#endif
#ifdef SIGSEGV			/* ANSI */
SIGSEGV,
#endif
#ifdef SIGQUIT
SIGQUIT,
#endif
#ifdef SIGTRAP
SIGTRAP,
#endif
#ifdef SIGEMT
SIGEMT,
#endif
#ifdef SIGKILL
SIGKILL,			/* for form's sake */
#endif
#ifdef SIGBUS
SIGBUS,
#endif
#ifdef SIGSYS
SIGSYS,
#endif
#ifdef SIGPIPE
SIGPIPE,
#endif
/* Do we need to worry about a trailing comma?? */
};
#endif /* HAVE_ABSIGLIST */

#ifndef HAVE_SETUP_ABSIGS
void _MPMY_setup_absigs(void){
    int i;
#if !defined(NO_SIGNALS)
    for(i=0; i< sizeof(absiglist)/sizeof(*absiglist); i++){
	signal(absiglist[i], MPMY_RaiseAbnormal);
    }
#endif
}
#endif

#ifndef HAVE_ABNORMAL
#define MAXUSERFUNCS 64

static int nuserfuncs = 0;
static Abhndlr userfuncs[MAXUSERFUNCS];
/* Lots of functions are suitable for use as userfuncs to be called in
   the event of abnormal termination.  These might include:
   malloc_print();
   PrintMemfile();
   PrintMPMYDiags();
   Msg_flush();
   MPMY_abchdir();  (with MPMY_abchdir_arg set beforehand)
   MPMY_SystemAbort();
   MPMY_SystemExit(); (with MPMY_exit_arg set beforehand)
   MPMY_Abannounce();

   They are called in the reverse chronological order, so later functions
   might 'override' earlier ones.  This isn't perfect, but it's better than
   the monolithic handler we had before.
*/

void MPMY_OnAbnormal(Abhndlr hndlr){
    if(nuserfuncs < MAXUSERFUNCS)
	userfuncs[nuserfuncs++] = hndlr;
}

int MPMY_Abnormal_signum;
int MPMY_stop_abnormal_processing;
void MPMY_RaiseAbnormal(int sig){
    int i;

    MPMY_Abnormal_signum = sig;
    MPMY_stop_abnormal_processing = 0;
    /* Just cycle through the 'user' functions. */
    for(i=nuserfuncs-1; i>=0 && !MPMY_stop_abnormal_processing; i--){
	(*userfuncs[i])();
    }
    MPMY_Abnormal_signum = 0;
}
#endif

#ifndef HAVE_SYSTEM_ABORT
void MPMY_SystemAbort(void){
    /* This should be unnecessary, since we didn't trap SIGABRT above. */
    signal(SIGABRT, SIG_DFL);
    abort();
}
#endif

#ifndef HAVE_SYSTEM_EXIT
int MPMY_exit_arg = 0;
void MPMY_SystemExit(void){
    exit(MPMY_exit_arg);
}
#endif

#ifndef HAVE_MPMY_CHDIR
/* This is pretty generic, but if the system can't even link against
   mkdir and chdir, then it will be necessary to override this with
   a noop in the PAROS-specific file. */
#include <unistd.h>
#ifndef __INTEL_SSD__ /* Intel's headers aren't safe for multiple inclusion! */
#include <sys/stat.h>
#endif
#include <errno.h>
char MPMY_Abchdir_arg[128];

/* Suitable for use as a userfunc */
void MPMY_Abchdir(void){
    if( strlen(MPMY_Abchdir_arg) ){
	errno=0;
	if( mkdir(MPMY_Abchdir_arg, 0777) && errno != EEXIST ){
	    Msg_do("Can't mkdir(\"%s\"): %d\n", MPMY_Abchdir_arg, errno);
	}
	errno = 0;
	if( chdir(MPMY_Abchdir_arg) ){
	    Msg_do("Can't chdir(\"%s\"): %d\n", MPMY_Abchdir_arg, errno);
	}else{
	    Msg_do("New directory: \"%s\"\n", MPMY_Abchdir_arg);
	}
    }
}
#endif

#ifndef HAVE_ABANNOUNCE
void MPMY_Abannounce(void){
    static int announced;
    /* Don't repeat yourself! */
    if (!announced){
	Msg_do("MPMY_ABNORMAL_SIGNUM: %d\n", MPMY_Abnormal_signum);
	MPMY_Diagnostic(Msg_do);
    }
    Msg_flush();
    announced = 1;
}
#endif

#ifndef HAVE_PRINTMPMY_DIAGS
void PrintMPMYDiags(void){
    MPMY_Diagnostic(Msg_do);
}
#endif

#ifndef HAVE_MPMY_TIMEOUT
/* Aieeee!!!  Some systems can link alarm, but when the execute it,
   they die (cm5).  Other systems can't even link it (sunmos, ap1000).
   Others will crash mysteriously if they use alarm (lsv).  This isn't
   really a PAROS thing, and it's not really ARCH either.  Aargh. */
#ifdef CANT_USE_ALARM
#define HAVE_MPMY_TIMEOUT
void
MPMY_TimeoutSet(int n){
    SinglWarning("Can't use alarm().  Timeout not set!\n");
}

void
MPMY_TimeoutReset(int n){
    return;
}

void 
MPMY_TimeoutCancel(void){
    return;
}

#else /* CANT_USE_ALARM */

static int nt;
static void
alrm_hndlr(int sig)
{
    /* Should this be Shout followed by MPMY_RaiseAbnormal()?? */
    Error("Exceeded timeout (%d sec)\n", nt);
}

void
MPMY_TimeoutSet(int n)
{
    void (*prev_sig)(int);
    prev_sig = signal(SIGALRM, alrm_hndlr);
    if( prev_sig == SIG_DFL || prev_sig == NULL || prev_sig == SIG_IGN ){
	singlPrintf("Setting timeout to %d seconds.\n", n);
	alarm(n);
	nt = n;
    }else{
	singlPrintf("There is already a handler for SIGALRM at %p\n", 
		    prev_sig);
	singlPrintf("NOT setting timeout!\n");
	signal(SIGALRM, prev_sig);
    }
}

void
MPMY_TimeoutReset(int n)
{
    alarm(n);
    nt = n;
}

void
MPMY_TimeoutCancel(void)
{
    alarm(0);
    signal(SIGALRM, SIG_DFL);
}
#endif /* CANT_USE_ALARM */
#endif /* HAVE_MPMY_TIMEOUT */
