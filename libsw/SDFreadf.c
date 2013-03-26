#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#include <stdarg.h>
#include "mpmy.h"
#include "SDF.h"
#include "Assert.h"
#include "Malloc.h"
#include "Msgs.h"
#include "error.h"
#include "verify.h"
#include "SDFread.h"
#include "gc.h"
#include "singlio.h"
#include "timers.h"

#define MAXNAMES 64

extern Timer_t SDFreadTm;

SDF *SDFreadf(char *hdr, char *name, void **btabp, int *gnobjp, int *nobjp, 
	    int stride,
	    /* char *name, offset_t offset, int *confirm */...)
{
    va_list ap;
    int start;
    SDF *sdfp;
    int gnobj, nobj;
    void *btab;
    void *addrs[MAXNAMES];
    char *names[MAXNAMES];
    int strides[MAXNAMES];
    int nobjs[MAXNAMES];
    int64_t starts[MAXNAMES];
    int *confirm;
    int nnames;

    EnableTimer(&SDFreadTm, "SDFread");
    StartTimer(&SDFreadTm);

    VerifySX(sdfp = SDFopen(hdr, name),SinglShout("%s", SDFerrstring));
    
    if( SDFgetint(sdfp, "npart", &gnobj) ){
	/* Hopefully calling va_start and va_end in here won't disturb */
	/* the real loop over arguments below... */
	va_start(ap, stride);
	names[0] = va_arg(ap, char *);
	gnobj = SDFnrecs(names[0], sdfp);
	va_end(ap);
	if( MPMY_Procnum() == 0 ){
	    SinglShout("%s does not have an \"npart\".\n", name);
	    SinglShout("Guessing npart=%d from SDFnrecs(., %s)\n", 
		       gnobj, names[0]);
	}
    }
    
    NobjInitial(gnobj, MPMY_Nproc(), MPMY_Procnum(), &nobj, &start);
    btab = Calloc(nobj, stride);
    Msgf(("Proc %d starting at %d in file, reading %d of %d\n",
	  MPMY_Procnum(), start, nobj, gnobj));

    nnames = 0;
    va_start(ap, stride);
    while(( names[nnames] = va_arg(ap, char *)) != NULL ){
	assert(nnames < MAXNAMES);
	addrs[nnames] = va_arg(ap, int) + (char *)btab;
	confirm = va_arg(ap, int *);
	if( !SDFhasname(names[nnames], sdfp) ){
	    *confirm = 0;
	    Msgf(("SDF file does not have %s\n", names[nnames]));
	    continue;
	}else{
	    *confirm = 1;
	}
	starts[nnames] = start;
	nobjs[nnames] = nobj;
	strides[nnames] = stride;
	nnames++;
    }
    va_end(ap);
    
    VerifyX(0==SDFseekrdvecsarr(sdfp, nnames,
			   names, starts, nobjs, addrs, strides),
	    Shout("%s", SDFerrstring));
    
    *nobjp = nobj;
    MPMY_Combine(nobjp, gnobjp, 1, MPMY_INT, MPMY_SUM);
    Msgf(("nobj=%d, gnobj=%d\n", *nobjp, *gnobjp));

    *btabp = btab;
    StopTimer(&SDFreadTm);
    OutputTimer(&SDFreadTm, singlPrintf); /* global sync and sets timer->max */
    singlPrintf("read speed %.0f Mb/s\n", (*gnobjp/(1024.*1024.))*(stride/SDFreadTm.max));
    DisableTimer(&SDFreadTm);
    return sdfp;
}

SDF *SDFreadf64(char *hdr, char *name, void **btabp, int64_t *gnobjp, int *nobjp, 
	    int stride,
	    /* char *name, offset_t offset, int *confirm */...)
{
    va_list ap;
    int64_t start;
    SDF *sdfp;
    int64_t gnobj;
    int nobj;
    void *btab;
    void *addrs[MAXNAMES];
    char *names[MAXNAMES];
    int strides[MAXNAMES];
    int nobjs[MAXNAMES];
    int64_t starts[MAXNAMES];
    int *confirm;
    int nnames;

    EnableTimer(&SDFreadTm, "SDFread");
    StartTimer(&SDFreadTm);

    VerifySX(sdfp = SDFopen(hdr, name),SinglShout("%s", SDFerrstring));
    
    if( SDFgetint64(sdfp, "npart", &gnobj) ){
	/* Hopefully calling va_start and va_end in here won't disturb */
	/* the real loop over arguments below... */
	va_start(ap, stride);
	names[0] = va_arg(ap, char *);
	gnobj = SDFnrecs(names[0], sdfp);
	va_end(ap);
	if( MPMY_Procnum() == 0 ){
	    SinglShout("%s does not have an \"npart\".\n", name);
	    SinglShout("Guessing npart=%ld from SDFnrecs(., %s)\n", 
		       gnobj, names[0]);
	}
    }
    
    NobjInitial64(gnobj, MPMY_Nproc(), MPMY_Procnum(), &nobj, &start);
    btab = Calloc(nobj, stride);
    Msgf(("Proc %d starting at %ld in file, reading %d of %ld\n",
	  MPMY_Procnum(), start, nobj, gnobj));

    nnames = 0;
    va_start(ap, stride);
    while(( names[nnames] = va_arg(ap, char *)) != NULL ){
	assert(nnames < MAXNAMES);
	addrs[nnames] = va_arg(ap, int) + (char *)btab;
	confirm = va_arg(ap, int *);
	if( !SDFhasname(names[nnames], sdfp) ){
	    *confirm = 0;
	    Msgf(("SDF file does not have %s\n", names[nnames]));
	    continue;
	}else{
	    *confirm = 1;
	}
	starts[nnames] = start;
	nobjs[nnames] = nobj;
	strides[nnames] = stride;
	nnames++;
    }
    va_end(ap);
    
    VerifyX(0==SDFseekrdvecsarr(sdfp, nnames,
			   names, starts, nobjs, addrs, strides),
	    Shout("%s", SDFerrstring));
    
    *gnobjp = *nobjp = nobj;
    MPMY_Combine(gnobjp, gnobjp, 1, MPMY_INT64, MPMY_SUM);
    Msgf(("nobj=%d, gnobj=%ld\n", *nobjp, *gnobjp));

    *btabp = btab;
    StopTimer(&SDFreadTm);
    OutputTimer(&SDFreadTm, singlPrintf); /* global sync and sets timer->max */
    singlPrintf("read speed %.0f Mb/s\n", (*gnobjp/(1024.*1024.))*(stride/SDFreadTm.max));
    DisableTimer(&SDFreadTm);
    return sdfp;
}

