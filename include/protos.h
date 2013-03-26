/* This is where we keep the code to shut up warnings about implicit */
/* declarations. */

/*
 * Copyright 1991 Michael S. Warren and John K. Salmon.  All Rights Reserved.
 */
#ifndef _ProtosDOTh
#define _ProtosDOTh

/* gcc's fixprotos changed in 2.5 so that it is no longer necessary
   to have protos for all these.  If __GNUC__ and __GNUC_MINOR are undefined,
   ANSI says the pre-processor should treat them as 0, so the test should
   pass and we should see the prototypes - fingers crossed.
*/
/* AIEEEE!!!  The above comment only applied to the short-lived 
   aberation 2.5.4.  It is broken again in 2.5.5.  Unfortunately,
   there is no __GNUC__SUBMINOR__ so I can't switch on it.  The
   following conditional worked for 2.5.4...
#if defined(sun) && !defined(__SUN5__) && __GNUC__<=2 && __GNUC_MINOR__<=4
*/

/*------------------------------------------------------*/
/* ----------------BEGIN--SUNOS------------------------ */
/*------------------------------------------------------*/
#if defined(sun) 
#include <stddef.h>
/* Sunos4.1.3 sometimes seems to come with prototypes...And they're wrong! */
/* Setting ARCH=sun4proto will activate this */
#ifndef _SUNOS4_PROTOTYPES_
extern int bcopy(const void *from, void *to, size_t);
#if !defined(__SUN5__) && !defined (_SUNACC)
/* Sun's stdio doesn't have protos for fflush, printf, what else?? */
/* I can't predict whether FILE is meaningful, so I use void* */
/* Furthermore, I can't give a prototype for sprintf because gcc complains */
/* about a conflict between old-style decl and one with an ellipsis */
#include "gccextensions.h"
#include <stdarg.h>

/* Sigh...  In 2.5.6 and 2.5.7 (at least).  fixincludes gives full
   prototypes for all the non-integer-return functions in std*.h. */
#if 0
extern char *sprintf();
#endif

extern int printf( const char *, ... )
     __attribute__ ((format (printf, 1, 2)));
extern int fprintf(void *, const char *, ...)
     __attribute__((format (printf, 2, 3)));
extern int vfprintf(void *, const char *, va_list)
     __attribute__((format (printf, 2, 0)));
extern int vsprintf(char *, const char *, va_list)
     __attribute__((format (printf, 2, 0)));
extern int scanf(const char *, ...)
     __attribute__((format (scanf, 1, 2)));
extern int sscanf(const char *, const char *, ...)
     __attribute__((format (scanf, 2, 3)));
extern int fflush(void *);
extern int fwrite(const void *, size_t, size_t, void/*FILE*/ *);
extern int fseek(void/*FILE*/ *, long, int);
extern int fread(void *, size_t, size_t, void/*FILE*/ *);
extern int fclose(void *);
extern int raise(int);
extern int _filbuf(void *);
extern int _flsbuf(int, void *);
#ifndef ungetc
extern int ungetc(int, void*);
#endif
extern int setvbuf(void *, void *, int, size_t);
/* We carefully include stdlib.h when we use these, */
/* but Sun has elected to leave them out of stdlib.h...go figure */
extern void *memmove(void *, const void *, size_t);
extern void *memccpy (void *, const void *, int, size_t );
extern void *memchr (const void *, int, size_t );
extern void *memcpy (void *, const void *, size_t );
extern void *memset (void *, int, size_t );
#endif /* __SUN5__ */
#endif /* _SUNOS4_PROTOTYPES_ */
#endif /* sun */
/*------------------------------------------------------*/
/* ------------------END--SUNOS------------------------ */
/*------------------------------------------------------*/

/*------------------------------------------------------*/
/* ------------------BEGIN--INTEL---------------------- */
/*------------------------------------------------------*/
#if defined(__INTEL_SSD__)
#include <stddef.h>
extern int bcopy(const void *from, void *to, size_t);

/* This should be in nx.h or cube.h or mesh.h */
extern void flick(void);

#if defined(__DELTA__) || defined(__GAMMA__)
/* This should be in fcntl.h */
int open(const char *, int flags, ...);
int creat(const char *, int/* mode_t */);

/* These should be in unistd.h */
int close(int);
int unlink(const char *);
int read(int, void *buf, unsigned int);
int write(int, const void *, unsigned int);
/*off_t*/long lseek(int, long/*off_t*/, int);

/* Should be in sys/stat.h */
int mkdir(const char *, int);
#ifdef S_IRGRP
/* this means we already included <sys/stat.h> */
int fstat(int, struct stat *);
#endif
#endif /* __DELTA__ || __GAMMA__ */

#endif /* !__INTEL_SSD__ */
/*------------------------------------------------------*/
/* ------------------END--INTEL------------------------ */
/*------------------------------------------------------*/

/*------------------------------------------------------*/
/* ------------------BEGIN--SOLARIS/STARDENT----------- */
/*------------------------------------------------------*/
#if defined(__STARDENT__) || defined(__SUN5__)
int finite(double);
#endif
/*------------------------------------------------------*/
/* --------------------END--SOLARIS/STARDENT----------- */
/*------------------------------------------------------*/

#endif /* _PrototDOTh */
