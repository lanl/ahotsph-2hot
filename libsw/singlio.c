#include <stdarg.h>
#include <stdio.h>
#include "protos.h"
#include "mpmy.h"
#include "Msgs.h"
#include "singlio.h"

static int singl_auto_flush = 1;

int singlAutoflush(int new){
    int ret = singl_auto_flush;
    singl_auto_flush = new;
    return ret;
}

int singlPrintf(const char *fmt, ...){
    va_list ap;
    int ret;

    if(MPMY_Procnum() != 0 )
	return 0;
    va_start(ap, fmt);
    ret = vfprintf(stdout, fmt, ap);
    va_end(ap);
    if( singl_auto_flush )
	fflush(stdout);
    return ret;
}

void singlFflush(void)
{
    if( MPMY_Procnum() != 0 )
	return;
    fflush(stdout);
}
