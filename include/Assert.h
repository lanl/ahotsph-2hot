/* A drop-in replacement for assert.h, but it calls "error" which */
/* deposits a message in the msgbuf, the terminal, your home answering */
/* machine, and in skywriting at Malibu beach. */
#undef assert
# ifndef NDEBUG
/* Error may be #defined */
#include "error.h"

#ifndef NO_STRINGIFICATION
# define assert(ex)	((void)((ex)?0 :\
	 ((SWError)("Assertion (%s) failed: file \"%s\", line %d\n",\
		      #ex, __FILE__, __LINE__),0)))
#else
/* Not only do we not have Stringification, but we assume that we have */
/* the brain-damaged macro substitution into "..." */
/* Note that this breaks if the substituted string has " " in it!, e.g. 
   assert(SDFhasname("x", sdfp));
*/
# define assert(ex)	((void)((ex)?0 : ((SWError)("Assertion (%s) failed: file \"%s\", line %d\n", "ex", __FILE__, __LINE__), 0)))
#endif /* NO_STRINGIFICATION */
# else
# define assert(ex) ((void)0)
# endif
