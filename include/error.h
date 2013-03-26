#ifndef _ErrorDOTh
#define _ErrorDOTh

#include <stdarg.h>
#include "gccextensions.h"

/* Define an Error_t to describe error-like functions. */
typedef void (*Error_t)(const char *, ...);

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
void SWError(const char *, ...) 
/* noreturn only works in 2.5 or higher */
#if (__GNUC_MINOR__>=5 && __GNUC__==2)||__GNUC__>2 
     __attribute__ ((format (printf, 1, 2),noreturn));
#else
     __attribute__ ((format (printf, 1, 2)));
#endif

void vError(const char *, va_list) 
/* noreturn only works in 2.5 or higher */
#if (__GNUC_MINOR__>=5 && __GNUC__==2)||__GNUC__>2 
     __attribute__ ((format (printf, 1, 0),noreturn));
#else
     ;
#endif

void SinglError(const char *, ...) 
/* noreturn only works in 2.5 or higher */
#if (__GNUC_MINOR__>=5 && __GNUC__==2)||__GNUC__>2 
     __attribute__ ((format (printf, 1, 2),noreturn));
#else
     __attribute__ ((format (printf, 1, 2)));
#endif
void Warning(const char *, ...)
     __attribute__ ((format (printf, 1, 2)));
void SinglWarning(const char *, ...)
     __attribute__ ((format (printf, 1, 2)));
void SeriousWarning(const char *, ...)
     __attribute__ ((format (printf, 1, 2)));
void Shout(const char *mesg, ...)
     __attribute__ ((format (printf, 1, 2)));
void SinglShout(const char *mesg, ...)
     __attribute__ ((format (printf, 1, 2)));
#ifdef __cplusplus
}
#endif /* __cplusplus */

#if __GNUC__>1 /* Actually, only 2.4 or higher?  */
/* We have varargs macros! */
#define Error(format, args...) \
    (SWError)("%s (%d) in %s :\n" format, __FILE__, __LINE__, __FUNCTION__, ##args)
#define SinglError(format, args...) \
    (SinglError)("%s (%d) in %s :\n" format, __FILE__, __LINE__, __FUNCTION__, ##args)
#define Warning(format, args...) \
    (Warning)("%s (%d) in %s :\n" format, __FILE__, __LINE__, __FUNCTION__, ##args)
#define SinglWarning(format, args...) \
    (SinglWarning)("%s (%d) in %s :\n" format, __FILE__, __LINE__, __FUNCTION__, ##args)
#define SeriousWarning(format, args...) \
    (SeriousWarning)("%s (%d) in %s :\n" format, __FILE__, __LINE__, __FUNCTION__, ##args)

#else /* No wacky GNUC varargs stuff...*/
/* This prevents namespace collisions when linking SDF into perl5! (really!) */
#define Error SWError

#endif

#endif /* _ErrorDOTh */
