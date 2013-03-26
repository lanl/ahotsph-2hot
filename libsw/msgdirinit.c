#define NO_MSGS
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "Malloc.h"
#include "mpmy_abnormal.h"
#include "protos.h"
#include "error.h"
#include "Msgs.h"
#include "mpmy.h"
#include "files.h"
#include "mpmy_io.h"

#ifdef __DELTA__
void ivfprintf(FILE *stream, const char *fmt, va_list args);
void ifflush(FILE *stream);
#endif

void MsgdirInit(const char *name)
{
    char *junk;
    void *dbgfp;
    char *lastslash;
    char *dirname;

#ifdef __PARAGON__
#define PARAGON_MAX_OPEN 128
    if (MPMY_Nproc() > PARAGON_MAX_OPEN) {
	SinglWarning("Can't open more than %d files on paragon because of NORMA-IPC\n", 
		     PARAGON_MAX_OPEN);
	return;
    }
#endif

#ifdef sun
    if (MPMY_Nproc() > 32) {
	SinglWarning("Can't open %d files on cm5 because of fd limit\n", 
		     MPMY_Nproc());
	return;
    }
#endif

    junk = Malloc(strlen(name)+1); /* enough? */
    lastslash = strrchr(name, '/');
    if( lastslash == NULL || lastslash == &name[0] ){
	/* Either it's in "." or in "/" */
	/* In either case we don't need to do a mkdir */
	dirname = NULL;
    }else{
	strncpy(junk, name, lastslash-name);
	junk[lastslash-name] = '\0';
	dirname = junk;
    }

    /* We only try to create the lowest level of the name.  No
       recursion here... Nosiree. */
    if( dirname && !fexists(dirname) ){
	if( MPMY_Mkdir(dirname, 0777) ){
	    SinglWarning("Mkdir(%s) failed\n", dirname);
	    goto outahere;
	}
    }

    /* Now the directory is sure to exist. */
    dbgfp = fopen(name, "w");
    if( dbgfp == NULL ){
	Error("Could not fopen %s, errno=%d\n", name, errno);
    }

#ifdef __DELTA__
    Msg_addfile(dbgfp, (Msgvfprintf_t)ivfprintf, (Msgfflush_t)ifflush);
#else
    Msg_addfile(dbgfp, (Msgvfprintf_t)vfprintf, (Msgfflush_t)fflush);
#endif
 outahere:
    Free(junk);
}

