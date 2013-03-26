/* Some i/o routines that are likely to be common... */

#ifndef HAVE_MPMY_FPRINTF

int MPMY_Fprintf(MPMYFile *fp, const char *fmt, ...){
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = MPMY_Vfprintf(fp, fmt, ap);
    va_end(ap);
    return ret;
}
#endif

#ifndef HAVE_MPMY_VFPRINTF
#include <stdarg.h>
#include <stdio.h>

static char buf[1024];

int MPMY_Vfprintf(MPMYFile *fp, const char *fmt, va_list args){
    /* What a pain.  Sometimes sprintf returns a char* and sometimes
       it returns an int.  There's no good way to tell, but this
       should do.*/
    long ret = (long)vsprintf(buf, fmt, args);

    /* Broken versions of sprintf return their first arg... */
    if( ret == (long)buf )
	ret = strlen(buf);

    if( ret < 0 ){
	return ret;
    }
    if( ret >= sizeof(buf) ){
	/* This is serious.  We've probably scribbled over memory.
	   Maybe we should just bail out now?
	   */
	static int recursion;
	if( recursion++ == 0 )
	    Error("MPMY_Vfprintf overflow.  Data corruption likely!\n");
	recursion--;
    }

    if( MPMY_Fwrite(buf, 1, ret, fp) != ret ){
	return -1;
    }
    return ret;
}
#endif

#ifndef HAVE_MPMY_FLUSH

/* This is here just for completeness...  The higher levels all do
   their I/O using unbuffered primitives (read/write/open/close), so
   we don't have to do anything special to flush output */
int MPMY_Fflush(MPMYFile *fp){
    return 0;
}
#endif
