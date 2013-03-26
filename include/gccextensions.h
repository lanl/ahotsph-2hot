#ifndef _GccExtensionsDOTh
#define _GccExtensionsDOTh

/* this is broken in gcc2.4.0 through 2.4.4 */
#if __GNUC__<2 || (__GNUC__== 2 && __GNUC_MINOR__<=4)
#define BROKEN_GCC_FORMAT_ATTRIBUTE
#endif

/* This isn't an entirely perfect way to deal with functions that 
   don't return because sometimes we want more than one __attribute__.
   See, for example, the code in error.h and mpmy_abnormal.h  */
#if (__GNUC_MINOR__>=5 && __GNUC__==2)||__GNUC__>2 
#define __NORETURN__    __attribute__ ((noreturn))
#else
#define __NORETURN__
#endif

#undef __attribute__
#if !defined(__GNUC__) || defined(printf) || defined(scanf)
#define __attribute__(x)
#endif /* __GNUC__ */

/* NoInline can be used to prevent inlining of function calls at the */
/* calling location.  I.e., NoInline(func)(arg) instead of func(arg) */
/* We leave everything alone if we're not optimizing because there */
/* are no inlines in that case anyway.  */
#if defined(__GNUC__) && defined(__OPTIMIZE__)
#define NoInline(f) ({typeof(f) *fp = &f; *fp;})
#else
#define NoInline(f) f
#endif

#endif

