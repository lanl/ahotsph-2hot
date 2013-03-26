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

Timer_t SDFreadTm;

/* These ought to be arguments to SDFread, but that breaks too many
   things that call SDFread.  The varargs makes creating a wrapper a pain. */
char *SDFread_datafile = "datafile";
char *SDFread_hdrfile = "hdrfile";
char *SDFread_npart = "npart";

SDF *SDFread(SDF *csdfp, void **btabp, int *gnobjp, int *nobjp, 
	    int stride,
	    /* char *name, offset_t offset, int *confirm */...)
{
    va_list ap;
    char name[256];
    int start;
    SDF *sdfp;
    int gnobj, nobj;
    void *btab;
    int Nfiles, procs_per_file, myfile, mysection;
    char hdrname[256];
    void *addrs[MAXNAMES];
    char *names[MAXNAMES];
    int strides[MAXNAMES];
    int nobjs[MAXNAMES];
    int64_t starts[MAXNAMES];
    int *confirm;
    int nnames;
    int sz = 0;

    EnableTimer(&SDFreadTm, "SDFread");
    StartTimer(&SDFreadTm);
    
    if( !SDFread_datafile || !SDFhasname(SDFread_datafile, csdfp) ){
	sdfp = csdfp;
	SinglWarning("SDFread: Looking for data in 'control' file\n");
	Nfiles = 1;
    }else{
	sdfp = NULL;
	Nfiles = SDFnrecs(SDFread_datafile, csdfp);
	if( Nfiles > MPMY_Nproc() || Nfiles < 0){
	    SinglError("Sorry, bad Nfiles (%d)!\n", Nfiles);
	}
    }
       
    /* Pick out which file in the control file will be ours. */
    /* and which "section" of the file. */
    procs_per_file = MPMY_Nproc()/Nfiles;
    Verify(procs_per_file*Nfiles == MPMY_Nproc());
    myfile = MPMY_Procnum()/procs_per_file;
    mysection = MPMY_Procnum()%procs_per_file;
    
    if( sdfp == NULL ){
	VerifySX(0==SDFseekrdvecs(csdfp, 
				 SDFread_datafile, myfile, 1, name, sizeof(name),
				 NULL),
		SinglShout("%s", SDFerrstring));
	if( SDFread_hdrfile )
	    SDFgetstringOrDefault(csdfp, SDFread_hdrfile, hdrname, sizeof(hdrname), "");
	else
	    hdrname[0] = '\0';

	/* This was moved from above where it used name unitialized */
	if (hdrname[0] && SDFissdf(name) ) {
	    SinglWarning("Superfluous headerfile %s ignored\n", hdrname);
	    hdrname[0] = '\0';
	}
	VerifySX(sdfp = SDFopen(hdrname, name),SinglShout("%s", SDFerrstring));
    }else{
	strncpy(name, "<SDF file>", sizeof(name));
    }

    if( SDFbyteorder(sdfp) == 0 ){
	int swap;
	/* The data/hdr file itself doesn't specify a byte order. */
	SDFgetintOrDefault(csdfp, "swapbytes", &swap, 0);
	if( swap )
	    SDFswap(sdfp);
	/* If there's no byteorder specified, then it won't swap */
    }
    
    if( SDFread_npart && SDFgetint(sdfp, SDFread_npart, &gnobj) ){
	/* Hopefully calling va_start and va_end in here won't disturb */
	/* the real loop over arguments below... */
	va_start(ap, stride);
	names[0] = va_arg(ap, char *);
	gnobj = SDFnrecs(names[0], sdfp);
	va_end(ap);
	if( MPMY_Procnum() == 0 ){
	    SinglShout("%s does not have an \"%s\".\n", name, SDFread_npart);
	    SinglShout("Guessing %s=%d from SDFnrecs(., %s)\n", 
		       SDFread_npart, gnobj, names[0]);
	}
    }
    
    NobjInitial(gnobj, procs_per_file, mysection, &nobj, &start);
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
	sz += SDFtype_sizes[SDFtype(names[nnames], sdfp)];
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
    singlPrintf("read %ld x %d at %.0f MB/s\n", *gnobjp, sz, (*gnobjp/(100.*1000.))*(sz/ReadTimer(&SDFreadTm)));
    DisableTimer(&SDFreadTm);
    return sdfp;
}

SDF *SDFread64(SDF *csdfp, void **btabp, int64_t *gnobjp, int *nobjp, 
	    int stride,
	    /* char *name, offset_t offset, int *confirm */...)
{
    va_list ap;
    char name[256];
    int64_t start;
    SDF *sdfp;
    int64_t gnobj;
    int nobj;
    void *btab;
    int Nfiles, procs_per_file, myfile, mysection;
    char hdrname[256];
    void *addrs[MAXNAMES];
    char *names[MAXNAMES];
    int strides[MAXNAMES];
    int nobjs[MAXNAMES];
    int64_t starts[MAXNAMES];
    int *confirm;
    int nnames;
    int sz = 0;

    EnableTimer(&SDFreadTm, "SDFread");
    StartTimer(&SDFreadTm);
    
    if( !SDFread_datafile || !SDFhasname(SDFread_datafile, csdfp) ){
	sdfp = csdfp;
	SinglWarning("SDFread: Looking for data in 'control' file\n");
	Nfiles = 1;
    }else{
	sdfp = NULL;
	Nfiles = SDFnrecs(SDFread_datafile, csdfp);
	if( Nfiles > MPMY_Nproc() || Nfiles < 0){
	    SinglError("Sorry, bad Nfiles (%d)!\n", Nfiles);
	}
    }
       
    /* Pick out which file in the control file will be ours. */
    /* and which "section" of the file. */
    procs_per_file = MPMY_Nproc()/Nfiles;
    Verify(procs_per_file*Nfiles == MPMY_Nproc());
    myfile = MPMY_Procnum()/procs_per_file;
    mysection = MPMY_Procnum()%procs_per_file;
    
    if( sdfp == NULL ){
	VerifySX(0==SDFseekrdvecs(csdfp, 
				 SDFread_datafile, myfile, 1, name, sizeof(name),
				 NULL),
		SinglShout("%s", SDFerrstring));
	if( SDFread_hdrfile )
	    SDFgetstringOrDefault(csdfp, SDFread_hdrfile, hdrname, sizeof(hdrname), "");
	else
	    hdrname[0] = '\0';

	/* This was moved from above where it used name unitialized */
	if (hdrname[0] && SDFissdf(name) ) {
	    SinglWarning("Superfluous headerfile %s ignored\n", hdrname);
	    hdrname[0] = '\0';
	}
	VerifySX(sdfp = SDFopen(hdrname, name),SinglShout("%s", SDFerrstring));
    }else{
	strncpy(name, "<SDF file>", sizeof(name));
    }

    if( SDFbyteorder(sdfp) == 0 ){
	int swap;
	/* The data/hdr file itself doesn't specify a byte order. */
	SDFgetintOrDefault(csdfp, "swapbytes", &swap, 0);
	if( swap )
	    SDFswap(sdfp);
	/* If there's no byteorder specified, then it won't swap */
    }
    
    if( SDFread_npart && SDFgetint64(sdfp, SDFread_npart, &gnobj) ){
	/* Hopefully calling va_start and va_end in here won't disturb */
	/* the real loop over arguments below... */
	va_start(ap, stride);
	names[0] = va_arg(ap, char *);
	gnobj = SDFnrecs(names[0], sdfp);
	va_end(ap);
	if( MPMY_Procnum() == 0 ){
	    SinglShout("%s does not have an \"%s\".\n", name, SDFread_npart);
#if __WORDSIZE==64
	    SinglShout("Guessing %s=%ld from SDFnrecs(., %s)\n", 
		       SDFread_npart, gnobj, names[0]);
#else
	    SinglShout("Guessing %s=%lld from SDFnrecs(., %s)\n", 
		       SDFread_npart, gnobj, names[0]);
#endif
	}
    }
    
    NobjInitial64(gnobj, procs_per_file, mysection, &nobj, &start);
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
	sz += SDFtype_sizes[SDFtype(names[nnames], sdfp)];
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
    singlPrintf("read %ld x %d at %.0f MB/s\n", *gnobjp, sz, (*gnobjp/(1000.*1000.))*(sz/ReadTimer(&SDFreadTm)));
    DisableTimer(&SDFreadTm);
    return sdfp;
}

