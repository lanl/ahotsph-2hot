/*
    SDF Library for reading Self-Describing Files
    Copyright (C) 1991,1992  John K. Salmon

    Terms and conditions are specified in the file "copyright.h",
    and more precisely in the file COPYING.LIB which you should have
    received with this library.
*/
/* Implement the user-visible functions that make up the */
/* SDF package. */

/* Using alloca reduces memory fragmentation, and allows the halo */
/* finder to work on larger datasets */
#define USE_ALLOCA

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef USE_ALLOCA
#include <alloca.h>
#endif
#include "Msgs.h"
#include "stdio.h" /* for sprintf, etc.  No 'real' stdio */
#include "SDF-private.h"
#include "obstack.h"
#include "byteswap.h"
#include "Malloc.h"
#include "protos.h"
#include "mpmy.h"
#include "mpmy_io.h"

/* max_bufsz is the maximum read-buffer that we will attempt to
   malloc.  The code in rdvecs is capable of requesting arbitrarily
   large blocks and scaling back (in powers of 2) until malloc returns
   non-NULL.  That doesn't seem like a good idea on virtual systems,
   though, so the maximum first attempt is dynamically settable with
   SDFsetmaxbufsz.  Setting it to zero will remove the limit. This is
   the initial value.  */
#define DEFAULT_MAX_BUFSZ (256LL*1024*1024)

static void buildhash(SDF *hdr);
static vec_descrip_t *lookup(const char *name, SDF *hdr);
static int compar_func(const void *p1, const void *p2);
static vec_descrip_t **compar_vds;
static unsigned char junk[4] = {0x12, 0x34, 0x56, 0x78};
static unsigned int *cpubyteorder = (unsigned int *)&junk[0];
static int max_bufsz = DEFAULT_MAX_BUFSZ;

char SDFerrstring[256];
extern int SDFyyparse(void);

/* This is a non-guaranteed way to test if a file is SDF or not. */
/* It really only tests the initial comment!  This is not fail-safe! */
int SDFissdf(const char *name){
    MPMYFile *fp;
    int result = 1;
    int c;
    char buf[64];
    int bufcnt = 0;

    if( MPMY_Initialized() == 0 ){
	MPMY_Init(0, NULL);
    }
    /* Assume we do the 'right' thing with "-" */
    /* but what IS the right thing?? (see below) */
    fp = MPMY_Fopen(name, MPMY_RDONLY | MPMY_SINGL | MPMY_IOZERO);
    if( fp == NULL )
	return 0;

    buf[bufcnt] = 0;		/* In case read returns nothing */
    MPMY_Fread(buf, 1, 64, fp);
    if( (c=buf[bufcnt++]) != '#' ){ result=0; goto done;}
    while( isspace(c = buf[bufcnt++]) && c != '\n' );
    if( c == '\n' ) { result=0; goto done; }
    if( c !=  'S' ) { result=0; goto done; }
    if( buf[bufcnt++] != 'D' ) { result=0; goto done; }
    if( buf[bufcnt++] != 'F' ) { result=0; goto done; }
  done:
    MPMY_Fclose(fp);

    return result;
}    

SDF *SDFopen(const char *hdrfname, const char *datafname)
{
    SDF *hdr;
    int parseerr;

    /* Is this a good idea?  Does it promote sloppy programming?  Or does
       it allow a program that has no interest in MPMY to behave as
       expected?  Or does it belong in MPMY_Fopen instead? */
    if( MPMY_Initialized() == 0 ){
	MPMY_Init(0, NULL);
    }

    if( datafname == NULL ){
	sprintf(SDFerrstring, "SDFopen: NULL data file %s\n", datafname);
	return NULL;
    }	

    hdr = Calloc(1, sizeof(SDF));
    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFopen: could not calloc space for hdr\n");
	return NULL;
    }
    /* Handle the case of only one name in the arg list. */
    /* Don't worry about which one it is.  Is this right? */
    if( hdrfname == NULL || hdrfname[0] == '\0' )
	hdrfname = datafname;
    if( datafname == NULL || datafname[0] == '\0' )
	datafname = hdrfname;
	
    Msgf(("SDFopen:  Calling SDFyyprepare(ptr, hdr=%s, data=%s)\n", hdrfname, datafname));
    if( SDFyyprepare(hdr, hdrfname, datafname) < 0 ){
	/* Hopefully, errstring was already set... */
	Free(hdr);
	return NULL;
    }

    parseerr= SDFyyparse();
    
    if(parseerr){
	/* Try to free up whatever's been allocated inside the hdr */
	SDFclose(hdr);
	return NULL;
    }else{
	Msgf(("SDFopen:  SDFyyparse completed ok\n"));
	/* buildhash belongs in SDF_finishparse, but then I'd need to add 
	   a bunch more externals... Yuck. */
	buildhash(hdr);
	return hdr;
    }
}

int SDFclose(SDF *hdr)
{
    int i;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFclose: not an SDF hdr\n");
	return -1;
    }
    /* The check here is necessary in case we are trying to bail */
    /* out from inside SDFopen on a parse error.  Cleaning up the */
    /* mess after a parse error still needs work! */
    if( hdr->vecs ){
	for(i=0; i<hdr->nvecs; i++){
	    Free(hdr->vecs[i].name);
	}
    }
    obstack_free(&hdr->blks_obs, NULL);
    obstack_free(&hdr->vecs_obs, NULL);
    obstack_free(&hdr->data_obs, NULL);
    if( hdr->vec_names )
	Free(hdr->vec_names);
    if( hdr->datafp )
	MPMY_Fclose(hdr->datafp);
    if( hdr->hashtbl )
	Free(hdr->hashtbl);
    Free(hdr);
    return 0;
}

int SDFnvecs(SDF *hdr)
{
    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFnvecs: not an SDF hdr\n");
	return -1;
    }
    return hdr->nvecs;
}

int SDFseekable(SDF *hdr)
{
    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFseekable: not an SDF hdr\n");
	return -1;
    }
    return (MPMY_Fseek(hdr->datafp, 0, MPMY_SEEK_CUR) >= 0);
}

int SDFhasname(const char *name, SDF *hdr)
{
    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFhasname: not an SDF hdr\n");
	return -1;
    }
    return (lookup(name, hdr)) ? 1 : 0;
}

char **SDFvecnames(SDF *hdr)
{
    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFvecnames: not an SDF hdr\n");
	return NULL;
    }
    return hdr->vec_names;
}

int64_t SDFnrecs(const char *name, SDF *hdr)
{
    vec_descrip_t *desc;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFnrecs: not an SDF hdr\n");
	return -1;
    }
    desc = lookup(name, hdr);
    if(desc)
	return hdr->blks[desc->blk_num].Nrec;
    else
	return 0;
}

int SDFarrcnt(const char *name, SDF *hdr)
{
    vec_descrip_t *desc;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFarrcnt: not an SDF hdr\n");
	return -1;
    }
    desc = lookup(name, hdr);
    if(desc)
	return desc->arrcnt;
    else
	return 0;
}

enum SDF_type_enum SDFtype(const char *name, SDF *hdr)
{
    vec_descrip_t *desc;
    
    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFtype: not an SDF hdr\n");
	return SDF_NOTYPE;
    }
    desc = lookup(name, hdr);
    if(desc)
	return desc->type;
    else
	return SDF_NOTYPE;
}

int SDFtell(const char *name, SDF *hdr)
{
    vec_descrip_t *desc;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFtell: not an SDF hdr\n");
	return -1;
    }
    desc = lookup(name, hdr);
    if(desc)
	return desc->nread;
    else
	return -1;
}

int SDFseek(const char *name, int64_t offset, int whence, SDF *hdr)
{
    vec_descrip_t *desc;
    int64_t nrec;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFseek: not an SDF hdr\n");
	return -1;
    }
    desc = lookup(name, hdr);
    if(desc == NULL)
	return -1;

    switch(whence){
    case SDF_SEEK_SET:
	desc->nread = offset;
	break;
    case SDF_SEEK_CUR:
	desc->nread += offset;
	break;
    case SDF_SEEK_END:
	nrec = hdr->blks[desc->blk_num].Nrec ;
	if(nrec == 0)
	    return -1;
	desc->nread = nrec + offset;
    }
    return 0;
}

/* SDFfileoffset and SDFfilestride are for those who think they */
/* can outsmart SDF*rdvecs*.  They tell you the offset of the FIRST */
/* element of NAME in the file, and the stride between successive */
/* elements.  They do not pay any attention to the current seek pointer. */
/* We do not provide SDFfilefp because we don't want you to mess with */
/* our file pointer.  Open the file yourself if you're so smart... */
int64_t SDFfileoffset(const char *name, SDF *hdr){
    vec_descrip_t *desc;
    blk_descrip_t *blk;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFfileoffset: not an SDF hdr\n");
	return -1;
    }
    if( hdr->begin_file_offset < 0 ){
	sprintf(SDFerrstring, "SDFfileoffset: unseekable file\n");
	return -1;
    }

    desc = lookup(name, hdr);
    if(desc == NULL)
	return -1;
    blk = &hdr->blks[desc->blk_num];
    if( blk->inmem ){
	sprintf(SDFerrstring, "SDFfileoffset: %s not in data segment\n", name);
	return -1;
    }
    return blk->begin_offset + desc->blk_off + hdr->begin_file_offset;
}

int64_t SDFfilestride(const char *name, SDF *hdr){
    vec_descrip_t *desc;
    blk_descrip_t *blk;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFfilestride: not an SDF hdr\n");
	return -1;
    }
    desc = lookup(name, hdr);
    if(desc == NULL)
	return -1;
    blk = &hdr->blks[desc->blk_num];
    if( blk->inmem ){
	sprintf(SDFerrstring, "SDFfilestride: %s not in data segment\n", name);
	return -1;
    }
    return blk->reclen;
}

unsigned int SDFcpubyteorder(void){
    return *cpubyteorder;
}

unsigned int SDFbyteorder(SDF *hdr){
    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFbyteorder: not an SDF hdr\n");
	return -1;
    }
    /* This is either 0 or the value set by a 'parameter byteorder=' stmt */
    return hdr->byteorder;
}

int SDFswap(SDF *hdr){
    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFswap: not an SDF hdr\n");
	return -1;
    }
    hdr->swapping = 1;
    return 0;
}

int SDFnoswap(SDF *hdr){
    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFnoswap: not an SDF hdr\n");
	return -1;
    }
    hdr->swapping = 0;
    return 0;
}

int SDFisswapping(SDF *hdr){
    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFisswapping: not an SDF hdr\n");
	return -1;
    }
    return hdr->swapping;
}

int SDFsetmaxbufsz(int new){
    int ret = max_bufsz;
    max_bufsz = new;
    return ret;
}

int SDFrdvecs(SDF *hdr, ...
	     /* char *name, int n, void *address, int stride,  ... */ )
{
    va_list ap;
    int ret;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFrdvecs: not an SDF hdr\n");
	return -1;
    }
    va_start(ap, hdr);
    ret = SDFrdvecsv(hdr, ap);
    va_end(ap);
    return ret;
}

int SDFrdvecsv(SDF *hdr, va_list ap)
{
    int *narray;
    char **namearray;
    void **addressarray;
    int *stridearray;
    struct obstack nobs;
    struct obstack nameobs;
    struct obstack addressobs;
    struct obstack strideobs;
    int nrequests;
    char *name;
    int stride, n;
    void *address;
    int ret;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFrdvecsv: not an SDF hdr\n");
	return -1;
    }
    obstack_begin(&nameobs, 32*sizeof(char *));
    obstack_begin(&strideobs, 32*sizeof(int));
    obstack_begin(&nobs, 32*sizeof(int));
    obstack_begin(&addressobs, 32*sizeof(void *));

    nrequests = 0;
    while( (name = va_arg(ap, char *)) ){
	nrequests++;
	obstack_ptr_grow(&nameobs, name);
	n = va_arg(ap, int);
	obstack_int_grow(&nobs, n);
	address = va_arg(ap, void *);
	obstack_ptr_grow(&addressobs, address);
	stride = va_arg(ap, int);
	obstack_int_grow(&strideobs, stride);
    }
    obstack_int_grow(&nobs, 0);
    obstack_int_grow(&strideobs, 0);
    obstack_ptr_grow(&nameobs, NULL);
    obstack_ptr_grow(&addressobs, NULL);

    narray = (int *)obstack_finish(&nobs);
    namearray = (char **)obstack_finish(&nameobs);
    addressarray = (void **)obstack_finish(&addressobs);
    stridearray = (int *)obstack_finish(&strideobs);
    ret = SDFrdvecsarr(hdr, nrequests, 
		     namearray, narray, addressarray, stridearray);
    obstack_free(&nobs, NULL);
    obstack_free(&nameobs, NULL);
    obstack_free(&addressobs, NULL);
    obstack_free(&strideobs, NULL);
    return ret;
}
    
int SDFrdvecsarr(SDF *hdr, int nreq, 
	  char **names, int *ns, void **addresses, int *strides)
{
    int64_t *starts;
    int i;
    int ret;
    vec_descrip_t *descrip;

    starts = Malloc(nreq*sizeof(int64_t));
    if( starts == NULL && nreq>0 ){
	sprintf(SDFerrstring, "Could not malloc tmp space in SDFrdvecsarr\n");
	return -1;
    }

    for(i=0; i<nreq; i++){
	if( (descrip = lookup(names[i], hdr)) == NULL ){
	    ret = -1;
	    sprintf(SDFerrstring, "SDFrdvecsarr: \"%s\" name not found\n",
		    names[i]);
	    goto outahere;
	}
	starts[i] = descrip->nread;
    }
    if( (ret = SDFseekrdvecsarr(hdr, nreq, names, 
			       starts, ns, addresses, strides)) ){
	goto outahere;
    }
    /* At this point, we have successfully looked up all */
    /* the names twice (at least). */
    for(i=0; i<nreq; i++){
	descrip = lookup(names[i], hdr);
	/* assert(descrip); */
	descrip->nread += ns[i];
    }	
 outahere:
    Free(starts);
    return ret;
}

int SDFseekrdvecs(SDF *hdr, ...
	     /* char *name, int start, int n, void *addr, int stride,  ... */ )
{
    va_list ap;
    int ret;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFseekrdvecs: not an SDF hdr\n");
	return -1;
    }
    va_start(ap, hdr);
    ret = SDFseekrdvecsv(hdr, ap);
    va_end(ap);
    return ret;
}

int SDFseekrdvecsv(SDF *hdr, va_list ap)
{
    int *narray;
    int64_t *startarray;
    char **namearray;
    void **addressarray;
    int *stridearray;
    struct obstack nobs;
    struct obstack nameobs;
    struct obstack startobs;
    struct obstack addressobs;
    struct obstack strideobs;
    int nrequests;
    char *name;
    int64_t start;
    int64_t zero64 = 0;
    int stride, n;
    void *address;
    int ret;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFseekrdvecsv: not an SDF hdr\n");
	return -1;
    }
    obstack_begin(&nameobs, 32*sizeof(char *));
    obstack_begin(&startobs, 32*sizeof(int64_t));
    obstack_begin(&strideobs, 32*sizeof(int));
    obstack_begin(&nobs, 32*sizeof(int));
    obstack_begin(&addressobs, 32*sizeof(void *));

    nrequests = 0;
    while( (name = va_arg(ap, char *)) ){
	nrequests++;
	obstack_ptr_grow(&nameobs, name);
	start = va_arg(ap, int64_t);
	obstack_grow(&startobs, &start, sizeof(int64_t));
	n = va_arg(ap, int);
	obstack_int_grow(&nobs, n);
	address = va_arg(ap, void *);
	obstack_ptr_grow(&addressobs, address);
	stride = va_arg(ap, int);
	obstack_int_grow(&strideobs, stride);
    }
    obstack_int_grow(&nobs, 0);
    obstack_grow(&startobs, &zero64, sizeof(int64_t));
    obstack_int_grow(&strideobs, 0);
    obstack_ptr_grow(&nameobs, NULL);
    obstack_ptr_grow(&addressobs, NULL);

    narray = (int *)obstack_finish(&nobs);
    namearray = (char **)obstack_finish(&nameobs);
    startarray = (int64_t *)obstack_finish(&startobs);
    addressarray = (void **)obstack_finish(&addressobs);
    stridearray = (int *)obstack_finish(&strideobs);
    ret = SDFseekrdvecsarr(hdr, nrequests, 
		     namearray, startarray, narray, addressarray, stridearray);
    obstack_free(&nobs, NULL);
    obstack_free(&nameobs, NULL);
    obstack_free(&startobs, NULL);
    obstack_free(&addressobs, NULL);
    obstack_free(&strideobs, NULL);
    return ret;
}
    
static 
void no_problem(const char *fmt, ...){
    /* Pass this to the MallocHandler function to tell Malloc that we
       know what we're doing, and we really can recover from malloc
       returning null! */
    return;
}

int SDFseekrdvecsarr(SDF *hdr, int nreq, 
	  char **names, int64_t *starts, int *ns, void **addresses, int *strides)
{
    int nn;
    off_t fileoffset;
    int i;
    int64_t nrec;
    int vecnum, first;
    int next_blknum;
    int last_blknum;
    int64_t sz_needed;
    int *sort_index;
    vec_descrip_t *descrip, **vd_array;
    char *fromptr, *toptr;
    char *recptr, *buf;
    int64_t buf_sz;
    int64_t last_rec, first_rec, new_last_rec;
    blk_descrip_t *blk;
    int stride, sz;
    int ret;
    int64_t nread, rec_cnt, rec_left, ncopy, ntry;
    int64_t *nleft, *seekto;
    char **toptr_arr;
    int64_t whole_sz;
    Error_t oldmallochandler;

    if( hdr == NULL ){
	sprintf(SDFerrstring, "SDFseekrdvecsarr: not an SDF hdr\n");
	return -1;
    }
    buf = NULL;
    buf_sz = 0;
#ifdef USE_ALLOCA
    sort_index = alloca(nreq*sizeof(int));
    vd_array = alloca(nreq*sizeof(vec_descrip_t *));
    nleft = alloca(nreq*sizeof(int64_t));
    toptr_arr = alloca(nreq*sizeof(char *));
    seekto = alloca(nreq*sizeof(int64_t));
#else
    sort_index = Malloc(nreq*sizeof(int));
    vd_array = Malloc(nreq*sizeof(vec_descrip_t *));
    nleft = Malloc(nreq*sizeof(int64_t));
    toptr_arr = Malloc(nreq*sizeof(char *));
    seekto = Malloc(nreq*sizeof(int64_t));
#endif

    if( nreq>0 && (sort_index == NULL 
       || vd_array == NULL || nleft == NULL || toptr_arr == NULL 
       || seekto == NULL) ){
	sprintf(SDFerrstring, 
		"SDFseekrdvecsarr: Malloc failed\n"
		"sort_index(%lu) = 0x%lx, vd_array(%lu) = 0x%lx\n",
		(unsigned long)nreq*sizeof(int), (unsigned long)sort_index, 
		(unsigned long)nreq*sizeof(vec_descrip_t), 
		(unsigned long)vd_array);
	ret = -1;
	goto outahere;
    }

    for(i=0; i<nreq; i++){
	sort_index[i] = i;
	if( (descrip = lookup(names[i], hdr)) != NULL ){
	    vd_array[i] = descrip;
	}else{
	    ret = -1;
	    goto outahere;
	}
	/* Check to make sure we don't go past the end of a record */
	nrec = hdr->blks[vd_array[i]->blk_num].Nrec;
	if(nrec > 0 && starts[i] + ns[i] > nrec){
	    ret = -1;
	    Msg_do("nrec %ld i %d starts[i] %ld ns[i] %d\n", 
		   nrec, i, starts[i], ns[i]);
	    sprintf(SDFerrstring, 
		    "SDFseekrdvecsarr: attempt to read past end of vector \"%s\"",
		    names[i]);
	    goto outahere;
	}
    }

    compar_vds = vd_array;
    qsort(sort_index, nreq, sizeof(sort_index[0]), compar_func);
    /* Now the descrip_list is sorted so that we can do all the */
    /* vectors in the same block at once. */

    vecnum = 0;
    while( vecnum < nreq ){
	descrip = vd_array[sort_index[vecnum]];
	first = vecnum;
	next_blknum = descrip->blk_num;
	blk = &hdr->blks[next_blknum];
	first_rec = starts[sort_index[vecnum]];
	last_rec = first_rec;
	do{
	    new_last_rec = ns[sort_index[vecnum]] + starts[sort_index[vecnum]];
	    if(new_last_rec > last_rec)
		last_rec = new_last_rec;
	    last_blknum = next_blknum;
	    if(++vecnum == nreq)
		break;
	    descrip = vd_array[sort_index[vecnum]];
	    next_blknum = descrip->blk_num;
	}while( last_blknum==next_blknum );

	/* We are going to do all the vectors between first and vecnum */
	/* all at once.   They all point at the same block. */
	rec_left = rec_cnt = last_rec - first_rec;
	for(i=first; i<vecnum; i++){
	    nleft[i] = ns[sort_index[i]];
	    toptr_arr[i] = addresses[sort_index[i]];
	    seekto[i] = starts[sort_index[i]];
	}
	if(blk->inmem){
	    recptr = ((char *)hdr->data) + blk->begin_offset + blk->reclen*first_rec;
	    fileoffset = 0;	/* not necessary, but it quiets a warning */
	}else{
	    whole_sz = blk->reclen * rec_left;
	    if( max_bufsz > 0 && whole_sz > max_bufsz ){
		sz_needed = (max_bufsz/blk->reclen)*blk->reclen;
		if( sz_needed < blk->reclen )
		    sz_needed = blk->reclen;
	    }else{
		sz_needed = whole_sz;
	    }
	    if( sz_needed > buf_sz ){
		oldmallochandler = MallocHandler(no_problem);
		if( buf )
		    Free(buf);
		buf_sz = sz_needed;
		buf = Malloc(buf_sz);
		while( buf == NULL && buf_sz >= blk->reclen ){
		    buf_sz >>= 1;
		    buf = Malloc(buf_sz);
		}
		MallocHandler(oldmallochandler);
		if( buf == NULL ){
		    sprintf(SDFerrstring, "SDFrdvecsarr: no space!\n");
		    ret = -1;
		    goto outahere;
		}
	    }
	    recptr = buf;
	    /* Repaired by msw Mon Jul 11 14:35:19 PDT 1994 */
	    /* Repaired again by johns Tue Jun 27 14:55:42 EST 1995 */
	    fileoffset = blk->begin_offset + (off_t)blk->reclen * first_rec 
		+ hdr->begin_file_offset;
	    Msgf(("blk->reclen %d, first_rec %ld\n", blk->reclen, first_rec));
	    Msgf(("fileoffset is %ld\n", fileoffset));
	    Msgf(("buf_sz is %ld\n", buf_sz));
	}

	while( rec_left > 0 ){
	    if(blk->inmem){
		nread = rec_left;
		rec_left = 0;
	    }else{
		if( blk->reclen > buf_sz ){
		    sprintf(SDFerrstring, "SDFrdvecsarr:  not enough for one record!\n");
		    ret = -1;
		    goto outahere;
		}
		whole_sz = blk->reclen * rec_left;
		if( whole_sz > buf_sz ){
		    ntry = (buf_sz-blk->reclen)/blk->reclen + 1;
		}else{
		    ntry = rec_left;
		}

		/* SDFlib2 model has one file pointer, thus use SEEK_SET */
		if((nread = MPMY_Fseekrd(hdr->datafp, fileoffset, 
				 MPMY_SEEK_SET, buf, blk->reclen, ntry))
		   != ntry ){
		    ret = -1;
		    sprintf(SDFerrstring,
			    "SDFrdvecsarr: fseekrd(offset=%ld, SEEK_CUR, reclen=%d, ntry=%ld) only got %ld, errno=%d\n", 
			    fileoffset, blk->reclen, ntry, nread, errno);
		    goto outahere;
		}
		rec_left -= nread;
		fileoffset += blk->reclen * (off_t)nread;
	    }

	    for(i=first; i<vecnum; i++){
		descrip = vd_array[sort_index[i]];
		if( seekto[i] > (first_rec + nread) || nleft[i] == 0 )
		    continue;
		fromptr = recptr + (seekto[i] - first_rec)*blk->reclen 
		    + descrip->blk_off;
		ncopy = nread - (seekto[i] - first_rec);
		if( ncopy > nleft[i] ){
		    ncopy = nleft[i];
		}
		toptr = toptr_arr[i];
		stride = strides[sort_index[i]];
		sz = SDFtype_sizes[descrip->type];
		if( stride == 0 )
		    stride = sz*descrip->arrcnt;
		if( sz*descrip->arrcnt > stride ){
		    /* This could have been checked earlier!!! */
		    sprintf(SDFerrstring, "stride for %s not long enough to step over successive elements.  arrcnt=%d, unit_sz=%d, stride=%d\n",
			    descrip->name, descrip->arrcnt, sz, stride);
		    ret = -1;
		    goto outahere;
		}
		for(nn=0; nn< ncopy; nn++){
		    if(!blk->inmem && hdr->swapping){
			if(Byteswap( sz, descrip->arrcnt, fromptr, toptr)){
			    sprintf(SDFerrstring, "SDFswapn(%d, %d, 0x%lx, 0x%lx) Failed",
				    sz, descrip->arrcnt, 
				    (unsigned long)fromptr, 
				    (unsigned long)toptr);
			    /* There's probably more cleaning up to do! */
			    ret = -1;
			    goto outahere;
			}
		    }else{
			(void)memcpy( toptr, fromptr, sz * descrip->arrcnt);
		    }
		    fromptr += blk->reclen;
		    toptr += stride;
		}
		seekto[i] += ncopy;
		nleft[i] -= ncopy;
		toptr_arr[i] = toptr;
	    }
	    first_rec += nread;
	}
    }
    ret = 0;
 outahere:
    /* alloca would be simpler! */
    /* The tests are not necessary according to ANSI... */
#ifndef USE_ALLOCA
    if( seekto ) Free(seekto);
    if( toptr_arr ) Free(toptr_arr);
    if( nleft )    Free(nleft);
    if( vd_array ) Free(vd_array);
    if( sort_index ) Free(sort_index);
#endif
    if( buf ) Free(buf);
    return ret;
}

static int compar_func(const void *p1, const void *p2){
    int i1 = *(int *)p1;
    int i2 = *(int *)p2;
    vec_descrip_t *d1 = compar_vds[i1];
    vec_descrip_t *d2 = compar_vds[i2];

    if(d1->blk_num < d2->blk_num)
	return -1;
    else if(d1->blk_num > d2->blk_num)
	return 1;
    else if(d1->nread < d2->nread)
	return -1;
    else if(d1->nread > d2->nread)
	return 1;
    else if(d1->blk_off < d2->blk_off)
	return -1;
    else if(d1->blk_off > d2->blk_off)
	return 1;
    else
	return 0;
}

/* a few prime numbers chosen entirely at random... */
#define NCOEF (sizeof(hashcoef)/sizeof(*hashcoef))
static int hashcoef[] = { 17, 37, 3, 97, 57, 23, 151, 7, 41 };

/* 
   This completely bogus hash function computes a linear combination of
   characters in the word.  
*/
static int SDFhash(const char *word, SDF *hdr){
    int i;
    const char *p;
    int sum = 0;

    p = word;
    i = 0;
    while(*p){
	sum += hashcoef[i] * *p;
	p++;
	if( ++i == NCOEF )
	    i = 0;
    }
    Msgf(("hash(%s) = %d%%%d = %d\n", word, sum, hdr->hashsz, sum%hdr->hashsz));
    return sum % hdr->hashsz;
}

static int isprime(int n){
    int d;
    /* this only needs to work for small O(few hundred) values of n */
    /* first make sure it's not even */
    if( (n&1) == 0 )
	return 0;
    /* Extremely naive.  Now check all odd potential divisors up to sqrt(n) */
    for(d = 3; d*d<=n ; d+=2){
	if( n%d == 0 )
	    return 0;
    }
    return 1;
}

static int nextprime(int n){
    Msgf(("nextprime(%d) = ", n));
    if( (n&1) == 0 )
	n++;
    while(!isprime(n)) n+=2;
    Msgf(("%d\n", n));
    return n;
}

static void buildhash(SDF *hdr){
    int sz;
    int i, h;

    sz = nextprime(5*hdr->nvecs);
    Msgf(("hash table of size %d\n", sz));
    hdr->hashsz = sz;
    hdr->hashtbl = Calloc(sz, sizeof(*hdr->hashtbl));

    for(i=0; i<hdr->nvecs; i++){
	h = SDFhash(hdr->vecs[i].name, hdr);
	while( hdr->hashtbl[h] != NULL ){
	    Msgf(("build:  hcollision between %s and %s in name-space\n",
		  hdr->hashtbl[h]->name, hdr->vecs[i].name));
	    h++;
	    if(h==sz)		/* wrap */
		h = 0;
	}
	hdr->hashtbl[h] = &hdr->vecs[i];
    }
}

static vec_descrip_t *lookup(const char *name, SDF *hdr)
{
    int i, istart;
    vec_descrip_t *possible;

    i = istart = SDFhash(name, hdr); 
    do{
	possible = hdr->hashtbl[i];
	if( possible == NULL ){
	    break;
	}
	if( strcmp(possible->name, name) == 0 )
	    return possible;

	Msgf(("lookup: hcollision i=%d, name=%s, tblname=%s\n", 
	      i, name, possible->name));
	i++;
	if( i==hdr->hashsz )	/* wrap */
	    i = 0;
	/* It ought to be imposible to fall off the bottom of this loop */
    }while(i!= istart);

    sprintf(SDFerrstring, "No such name, \"%s\" in file.", name);
    return NULL;
}
