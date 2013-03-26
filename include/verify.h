/* Defines Verify and VerifyX, which verify the truth of the given 
   expression.  Unlike assert, they guarantee that the expression be evaluated
   exactly once.  Thus, you might say:

   Verify(fp=fopen(name, "w"))
   Verify((nwrit=fwrite(p, sz,ni, fp)) >= 0);
   VerifyX((nwrit=fwrite(p, sz, ni, fp)) >= 0, Shout("errno=%d, nwrit=%d\n",
       errno, nwrit));
   Verify0(stat(path, , &buf));
   Notice that VerifyX allows you to add "Xtra" information in the event of
   a failure.
*/
#undef Verify
#undef VerifyX
/* Is there ever really a good reason to shut this off?? */
/* You can with -DNVERIFY. */
# ifndef NVERIFY
#include "error.h"

#ifndef NO_STRINGIFICATION
/* One can write a really slick version that uses GNU varargs macros, but */
/* unfortunately, it would break every other pre-processor.  It isn't even */
/* possible to #ifdef __GNUC__ it because the pre-processor complains */
/* about an incorrect arg count.  It might be possible to write it as a */
/* varargs function, but what's the point? */
#define VerifyX( expr, xtra) \
   (expr) ? (void)0 : (xtra, Error("%s failed\n" , #expr))
#define VerifySX( expr, xtra) \
   (expr) ? (void)0 : (xtra, SinglError("%s failed\n" , #expr))
#else
/* Not only do we not have Stringification, but we assume that we have */
/* the brain-damaged macro substitution into string constants. */
#define VerifyX( expr, xtra) \
   (expr) ? (void)0 : (xtra, Error("%s failed\n" , "expr"))
#define VerifySX( expr, xtra) \
   (expr) ? (void)0 : (xtra, SinglError("%s failed\n" , "expr"))
#endif /* NO_STRINGIFICATION */
# else
# define VerifyX( expr, xtra) ((expr),(void)0)
# endif
#define Verify( expr ) VerifyX(expr, (void)0)
#define VerifyS( expr ) VerifySX(expr, (void)0)
#define Verify0( expr ) Verify(!(expr))
#define VerifyS0( expr ) VerifyS(!(expr))
