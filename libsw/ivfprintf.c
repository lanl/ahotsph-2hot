#ifdef __SRV__
 # error This file should not be compiled with srv
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifndef FORCE_TYPE
/* mesh.h is not protected against multiple inclusion ! */
#include <mesh.h>
#endif

/* Why isn't this in mesh.h? */
extern void iowait(int);

/* A fast asynchronous disk message primitive */

/* There is circumstantial evidence that the delta dies if we have many */
/* iwrites pending and call ifflush  ???? (maybe fixed - johns ) ????*/

static int id[] = {-1, -1, -1, -1, -1, -1, -1, -1};
#define NID (sizeof(id)/sizeof(id[0]))
static int nid = 0;
static char buf[NID][1024];

void
ivfprintf(FILE *stream, const char *fmt, va_list args)
{
    if (id[nid] != -1){
	iowait(id[nid]);
    }
    vsprintf(buf[nid], fmt, args);
    id[nid] = iwrite(fileno(stream), buf[nid], strlen(buf[nid]));
    if( ++nid == NID )
	nid = 0;
}


void
ifflush(FILE *stream)
{
    int i;
    int ii;
    
    /* stream not used.  Just wait for all the pending i/o */
    for (ii = nid, i = 0; i < NID; i++){
      if (id[ii] != -1) {
	  iowait(id[ii]);
	  id[ii] = -1;
      }
      if( ++ii == NID )
	  ii = 0;
  }
}
