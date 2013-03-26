/* Declarations for the functions defined in Msgs.c */
#ifndef MsgsDOTh
#define MsgsDOTh
#include <stdarg.h>

#include "gccextensions.h"

/* These two typedefs simplify the task of casting function ptrs */
/* to the appropriate type when calling Msg_addfile */
typedef int (*Msgvfprintf_t)(void *, const char *, va_list);
typedef int (*Msgfflush_t)(void *);

#ifdef __cplusplus
extern "C" {
#endif
/* The fflush_like function pointer MUST BE ASYNCHRONOUSLY CALLABLE */
/* If you don't have one, don't worry.  Pass NULL instead. */
int Msg_addfile(void *fp,	/* add fp to the list of msg files */
	     int (*vfprintf_like)(void *, const char *, va_list),
	     int (*fflush_like)(void *));
int Msg_delfile(void *fp);	/* fp should no longer receive msgs */
int Msg_on(const char *);	/* turn on a specific type of msgs */
int Msg_off(const char *);	/* turn off a specific type of msgs */
/* Normally, you should use the Msg_test macro instead. */
int _Msg_test(const char *);	/* is a specific type on or off? */
int Msg_set_enable(int);	/* enable (1) or disable (0) all message  */
				/* return the previous state. */
void Msg_turnon(const char *msg_turn_on); /* an interface to Msg_on */
				/* that calls Msg_on for every file */
				/* in a comma-whitespace delimited list */
				/* of names.  If a name is specified as */
				/* NAME:lo-hi or NAME:procnum, then */
				/* messages are on only in the corresponding */
				/* processors.  If the string is "nomsgs", */
				/* then msgs are totally disabled. */
void MsgdirInit(const char *path); /* tries to do a mkdir and open */
				/* msg files for each process */
int Msg_do(const char *fmt, ...)  /* Unconditionally say something. */
     __attribute__ ((format (printf, 1, 2))) ; /* it's printf-like! */
int Msg_doalist(const char *fmt, va_list)
/* this is broken in gcc2.4.0 through 2.4.4 */
#ifndef BROKEN_GCC_FORMAT_ATTRIBUTE 
     __attribute__ ((format (printf, 1, 0)))
#endif
     ;

int Msg_flush(void);		/* flush all the buffers now. */
int Msg_flushalways(int newval);/* flush buffers after every Msg_do. 
				 return the previous value.*/
#ifdef __cplusplus
}
#endif

/* Don't use these! _Msg_enabled */
extern int _Msg_enabled;

/* Msg is called with an extra set of parentheses, to hide */
/* the variadic arguments, e.g.
   Msg("foo", ("Hello world this is a \"foo\" message\n"));
or
   Msg(__FILE__, ("Hello from file: %s, line: %d\n", __FILE__, __LINE__));

The macro 'Msgf' is a shorthand for the Msg(__FILE__, ...) construction.
The macro 'Msglno' is a shorthand for:
  Msg(name, "%s(%d):" <fmt> , __FILE__, __LINE__, <otherargs>);
The macro 'Msgfunc' is a shorthand for Msg(__FUNCTION__, ...).
  Thus, you can turn on messaging at the function level.
*/

#define Msglno(name, args) Msg(name, ("%s(%d): ", __FILE__, __LINE__)),Msg(name, args)
#define Msgf(args) Msg(__FILE__, args)
#define Msglnof(args) Msglno(__FILE__, args)
#ifdef __FUNCTION__
#define Msgfunc(args) Msg(__FUNCTION__, args)
#else
#define Msgfunc(args)
#endif

#ifndef NO_MSGS
#define Msg_test(name) (_Msg_enabled && _Msg_test(name))
#define Msg(name, args) ((void)((Msg_test(name))? Msg_do args : 0 ))
#else
#define Msg_test(name) (0)
#define Msg(name, args) ((void)0)
#endif


#endif /* MsgsDOTh */
