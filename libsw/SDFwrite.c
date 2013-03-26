/* This subroutine writes a restricted class of SDF files.
   The files contain an arbitrary set of ascii scalars, (specified in the
   variable length arg-list) and an array of binary struct records.
   This is good enough for our purposes at the moment.  More sophisticated
   interfaces will probably be needed as time goes on.
*/
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>		/* for open */
#include <unistd.h>		/* for close */
#include "SDF.h"
#include "SDFwrite.h"
#include "Malloc.h"
#include "mpmy.h"
#include "mpmy_io.h"
#include "Msgs.h"
#include "error.h"
#include "protos.h"
#include "timers.h"
#include "singlio.h"

Timer_t SDFwriteTm;
static char *header_buf;
static int header_size;
static int header_len;


/* We would like to write some headers separately from the data. */
/* We can let SDFwritehdr set this variable, and SDFwrite_alist */
/* will avoid writing the header if wrote_header is true */
static int wrote_header = 0;

/* How much to increase header buffer size when realloced */
#define BUF_INC 4096

/* How big is our line buffer */
#define LINE_LEN 512

/* Align the data segment on this size boundary. */
/* It MUST be less than LINE_LEN */
#define DATAALIGN 32

static void
SDFwrite_alist(const char *filename, int gnobj, int nobj, 
	 const void *btab, int bsize, const char *bodydesc, va_list alist);

static void
SDFwrite_alist64(const char *filename, int mode, int64_t gnobj, int64_t nobj, 
	 const void *btab, int bsize, const char *bodydesc, va_list alist);

static void
outstr(const char *str)
     /* This is an obstack, but who's counting? */
{
    int len;

    len = strlen(str);
    if (header_size - header_len <= len) { /* <= deals with terminal null */
        header_size += BUF_INC + len;
        header_buf = Realloc(header_buf, header_size);
    }
    strcpy(header_buf + header_len, str);
    header_len += len;
}

void
SDFwrite(const char *filename, int gnobj, int nobj, 
	 const void *btab, int bsize, const char *bodydesc, ...){
    va_list alist;

    EnableTimer(&SDFwriteTm, "SDFwrite");
    StartTimer(&SDFwriteTm);
    va_start(alist, bodydesc);
    SDFwrite_alist(filename, gnobj, nobj, btab, bsize, bodydesc, alist);
    va_end(alist);
    StopTimer(&SDFwriteTm);
    OutputTimer(&SDFwriteTm, singlPrintf); /* global sync and set timer->max */
    if (SDFwriteTm.max != 0.0) 
      singlPrintf("write speed %.0f MB/s\n", 
		  (gnobj/(1000.0*1000.0))*(bsize/SDFwriteTm.max));
    DisableTimer(&SDFwriteTm);	/* suppress printing again in OutputTimers */
}

void
SDFwrite64(const char *filename, int64_t gnobj, int64_t nobj, 
	 const void *btab, int bsize, const char *bodydesc, ...){
    va_list alist;
    int mode = MPMY_WRONLY|MPMY_CREAT|MPMY_TRUNC|MPMY_MULTI;

    EnableTimer(&SDFwriteTm, "SDFwrite");
    StartTimer(&SDFwriteTm);
    va_start(alist, bodydesc);
    SDFwrite_alist64(filename, mode, gnobj, nobj, btab, bsize, bodydesc, alist);
    va_end(alist);
    StopTimer(&SDFwriteTm);
    OutputTimer(&SDFwriteTm, singlPrintf); /* global sync and set timer->max */
    if (SDFwriteTm.max != 0.0) 
      singlPrintf("write speed %.0f MB/s\n", 
		  (gnobj/(1000.0*1000.0))*(bsize/SDFwriteTm.max));
    DisableTimer(&SDFwriteTm);	/* suppress printing again in OutputTimers */
}

void
SDFappend64(const char *filename, int64_t gnobj, int64_t nobj, 
	 const void *btab, int bsize, const char *bodydesc, ...){
    va_list alist;
    int mode = MPMY_WRONLY|MPMY_MULTI;

    EnableTimer(&SDFwriteTm, "SDFwrite");
    StartTimer(&SDFwriteTm);
    va_start(alist, bodydesc);
    SDFwrite_alist64(filename, mode, gnobj, nobj, btab, bsize, bodydesc, alist);
    va_end(alist);
    wrote_header = 1;
    StopTimer(&SDFwriteTm);
    OutputTimer(&SDFwriteTm, singlPrintf); /* global sync and set timer->max */
    if (SDFwriteTm.max != 0.0) 
      singlPrintf("write speed %.0f MB/s\n", 
		  (gnobj/(1000.0*1000.0))*(bsize/SDFwriteTm.max));
    DisableTimer(&SDFwriteTm);	/* suppress printing again in OutputTimers */
}

void
SDFwritehdr(const char *filename, const char *bodydesc, ...){
    va_list alist;

    va_start(alist, bodydesc);
    SDFwrite_alist(filename, 0, 0, NULL, 0, bodydesc, alist);
    va_end(alist);
    wrote_header = 1;
}

void
SDFunsetwroteheader(void)
{
  wrote_header = 0;
}

void
SDFsetwroteheader(void)
{
  wrote_header = 1;
}

static void
SDFwrite_alist(const char *filename, int gnobj, int nobj, 
	 const void *btab, int bsize, const char *bodydesc, va_list alist)
{
    MPMYFile *myfd;
    int i, pad;
    char line[LINE_LEN];
    int ival;
    double dval;
    char *sval;
    char *name;
    char *buf;
    int len;
    int mode;
    int ok, allok, retried;

    Msgf(("In Wtdata\n"));
    header_len = 0;

    header_size = BUF_INC;
    header_buf = Malloc(header_size);

    if (MPMY_Procnum() == 0 && wrote_header == 0) {
	outstr ("# SDF\n");
	sprintf(line, "parameter byteorder = 0x%x;\n", 
		SDFcpubyteorder()); outstr(line); 
	while( (name = va_arg(alist, char *)) ){
	    Msgf(("name(%lx)=%s\n", (unsigned long int)name, name));
	    switch( va_arg(alist, enum SDF_type_enum) ){
	      case SDF_INT:
		ival = va_arg(alist, int);
		sprintf(line, "int %s = %d;\n", name, ival); outstr(line);
		break;
	      case SDF_FLOAT:
		dval = va_arg(alist, double);
		sprintf(line, "float %s = %.8g;\n", name, dval); outstr(line);
		break;
	      case SDF_DOUBLE:
		dval = va_arg(alist, double);
		sprintf(line, "double %s = %.16g;\n", name, dval); 
		outstr(line);
		break;
	      case SDF_STRING:
		sval = va_arg(alist, char *);
		sprintf(line, "char %s[] = \"%s\";\n", name, sval);
		outstr(line);
		break;
	      default:
		Shout("Unexpected type in wtdata\n");
		break;
	    }
	}
	if( bodydesc ){
	    outstr(bodydesc);
	    if( gnobj > 0 )
	      sprintf(line, "[%d];\n", gnobj);
	    else
	      sprintf(line, "[];\n");
	    outstr(line);
	}
	outstr("#\f\n");
	outstr ("# SDF-EOH ");
	/* This little bit of magic will cause the first word of data */
	/* to be aligned.  This isn't required by anything, but it makes */
	/* it a lot easier to use really primitive tools like od. */
	pad = (header_len+1)%DATAALIGN;	/* the +1 is to account for the '\n' */
	if( pad )
	  pad = DATAALIGN - pad;
	for(i=0; i<pad; i++){
	    line[i] = ' ';
	}
	line[pad] = '\n';
	line[pad+1] = '\0';
	outstr(line);
	/* Avoid separate write for header due to paragon limitations */
	/* It's only memory, after all */
	len = header_len+bsize*nobj;
	buf = header_buf = Realloc(header_buf, len);
	memcpy(buf+header_len, btab, bsize*nobj);
    } else {
	len = bsize*nobj;
	buf = btab;
    }

    if (wrote_header == 0) {
	mode = MPMY_WRONLY|MPMY_CREAT|MPMY_TRUNC|MPMY_MULTI;
    } else {
	mode = MPMY_WRONLY|MPMY_APPEND|MPMY_MULTI;
    }
    retried = 0;
 retry:
    myfd = MPMY_Fopen(filename, mode);
    if( myfd == NULL ){
	SeriousWarning("MPMY_Fopen(%s, 0x%x) returns NULL, errno=%d\n",
		       filename, mode, errno);
	goto outahere;
    }

    i = MPMY_Fwrite(buf, 1, len, myfd);
    if (i != len){
	SeriousWarning("MPMY_Fwrite(btab, len=%d) only wrote %d, errno=%d\n", 
	      len, i, errno);
	SeriousWarning("\"%s\" is probably corrupt!\n", filename);
    }
    ok = (i==len);
    /* Should there be an MPMY_LAND and MPMY_LOR ? */
    allok = 0;
    MPMY_Combine(&ok, &allok, 1, MPMY_INT, MPMY_BAND);
    Msgf(("ok=%d, allok=%d\n", ok, allok));
    if( !allok ){
	int retryable;
#ifdef ETIMEDOUT  /* This seems to be a solaris thing */
	retryable = !retried && (ok || errno==ETIMEDOUT);
#else
	retryable = 0;
#endif
	Msgf(("retryable (local) = %d\n", retryable));
	MPMY_Combine(&retryable, &retryable, 1, MPMY_INT, MPMY_BAND);
	Msgf(("retryable (global) = %d\n", retryable));
	if( retryable ){
	    SinglShout("Fingers crossed, we are going to retry!\n");
	    retried = 1;		/* only retry once! */
	    MPMY_Fclose(myfd);
#ifdef ETIMEDOUT
	    /* The failure mode we are trying to recover from is an
	       NFS timeout which might go away in a few seconds.  We
	       trust that if ETIMEDOUT exists, then sleep does too...  */
	    SinglShout("sleep(30), maybe the timeout will go away!\n");
	    sleep(30);
#endif
	    goto retry;
	}
    }
    MPMY_Fclose(myfd);
 outahere:
    Free(header_buf);
    header_size = header_len = 0;
    header_buf = NULL;
    wrote_header = 0;
    Msgf(("SDFwrite2 done\n"));
}

static void
SDFwrite_alist64(const char *filename, int mode, int64_t gnobj, int64_t nobj, 
	 const void *btab, int bsize, const char *bodydesc, va_list alist)
{
    MPMYFile *myfd;
    int i, pad;
    char line[LINE_LEN];
    int ival;
    int64_t i64val;
    double dval;
    char *sval;
    char *name;
    char *buf;
    size_t len;
    int ok, allok, retried;

    Msgf(("In Wtdata\n"));
    header_len = 0;

    header_size = BUF_INC;
    header_buf = Malloc(header_size);

    if (MPMY_Procnum() == 0 && wrote_header == 0) {
	outstr ("# SDF\n");
	sprintf(line, "parameter byteorder = 0x%x;\n", 
		SDFcpubyteorder()); outstr(line); 
	while( (name = va_arg(alist, char *)) ){
	    Msgf(("name(%lx)=%s\n", (unsigned long int)name, name));
	    switch( va_arg(alist, enum SDF_type_enum) ){
	      case SDF_INT:
		ival = va_arg(alist, int);
		sprintf(line, "int %s = %d;\n", name, ival); outstr(line);
		break;
	      case SDF_INT64:
		i64val = va_arg(alist, int64_t);
		sprintf(line, "int64_t %s = %ld;\n", name, i64val); outstr(line);
		break;
	      case SDF_FLOAT:
		dval = va_arg(alist, double);
		sprintf(line, "float %s = %.8g;\n", name, dval); outstr(line);
		break;
	      case SDF_DOUBLE:
		dval = va_arg(alist, double);
		sprintf(line, "double %s = %.16g;\n", name, dval); 
		outstr(line);
		break;
	      case SDF_STRING:
		sval = va_arg(alist, char *);
		sprintf(line, "char %s[] = \"%s\";\n", name, sval);
		outstr(line);
		break;
	      default:
		Shout("Unexpected type in wtdata\n");
		break;
	    }
	}
	if( bodydesc ){
	    outstr(bodydesc);
	    if( gnobj > 0 ) {
#if __WORDSIZE==64
		sprintf(line, "[%ld];\n", gnobj);
#else
		sprintf(line, "[%lld];\n", gnobj);
#endif
	    } else {
		sprintf(line, "[];\n");
	    }
	    outstr(line);
	}
	outstr("#\f\n");
	outstr ("# SDF-EOH ");
	/* This little bit of magic will cause the first word of data */
	/* to be aligned.  This isn't required by anything, but it makes */
	/* it a lot easier to use really primitive tools like od. */
	pad = (header_len+1)%DATAALIGN;	/* the +1 is to account for the '\n' */
	if( pad )
	  pad = DATAALIGN - pad;
	for(i=0; i<pad; i++){
	    line[i] = ' ';
	}
	line[pad] = '\n';
	line[pad+1] = '\0';
	outstr(line);
	/* Avoid separate write for header due to paragon limitations */
	/* It's only memory, after all */
	len = header_len+(size_t)bsize*nobj;
	buf = header_buf = Realloc(header_buf, len);
	memcpy(buf+header_len, btab, (size_t)bsize*nobj);
    } else {
	len = (size_t)bsize*nobj;
	buf = btab;
    }

    retried = 0;
 retry:
    myfd = MPMY_Fopen(filename, mode);
    if( myfd == NULL ){
	SeriousWarning("MPMY_Fopen(%s, 0x%x) returns NULL, errno=%d\n",
		       filename, mode, errno);
	goto outahere;
    }

    if (!(mode & MPMY_TRUNC)) MPMY_Fseek(myfd, 0L, MPMY_SEEK_END);
    i64val = MPMY_Fwrite(buf, 1, len, myfd);
    if (i64val != len){
	SeriousWarning("MPMY_Fwrite(btab, len=%ld) only wrote %ld, errno=%d\n", 
	      len, i64val, errno);
	SeriousWarning("\"%s\" is probably corrupt!\n", filename);
    }
    ok = (i64val==len);
    /* Should there be an MPMY_LAND and MPMY_LOR ? */
    allok = 0;
    MPMY_Combine(&ok, &allok, 1, MPMY_INT, MPMY_BAND);
    Msgf(("ok=%d, allok=%d\n", ok, allok));
    if( !allok ){
	int retryable;
#ifdef ETIMEDOUT  /* This seems to be a solaris thing */
	retryable = !retried && (ok || errno==ETIMEDOUT);
#else
	retryable = 0;
#endif
	Msgf(("retryable (local) = %d\n", retryable));
	MPMY_Combine(&retryable, &retryable, 1, MPMY_INT, MPMY_BAND);
	Msgf(("retryable (global) = %d\n", retryable));
	if( retryable ){
	    SinglShout("Fingers crossed, we are going to retry!\n");
	    retried = 1;		/* only retry once! */
	    MPMY_Fclose(myfd);
#ifdef ETIMEDOUT
	    /* The failure mode we are trying to recover from is an
	       NFS timeout which might go away in a few seconds.  We
	       trust that if ETIMEDOUT exists, then sleep does too...  */
	    SinglShout("sleep(30), maybe the timeout will go away!\n");
	    sleep(30);
#endif
	    goto retry;
	}
    }
    MPMY_Fclose(myfd);
 outahere:
    Free(header_buf);
    header_size = header_len = 0;
    header_buf = NULL;
    wrote_header = 0;
    Msgf(("SDFwrite2 done\n"));
}
