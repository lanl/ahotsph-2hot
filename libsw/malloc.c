/* Combine malloc.c, prt_mem.c and _malloc.h into one file. */
/* Then remove abort() in favor of Error().  - johns, 12/24/92*/

/* Throw out the whole thing if USE_SYSTEM_MALLOC is defined. */
#ifdef USE_SYSTEM_MALLOC
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include "Msgs.h"
#ifndef __INSIGHT__
int malloc_debug(int i){ return -1; }
int malloc_verify(void){return 0;}
void malloc_print(void){Msg_do("Can't print malloc structures for system malloc\n");}
#endif
size_t malloc_avail(void){return -1;}

size_t
malloc_heapsz(void)
{
    char fname[128];
    char line[128];
    FILE *fp;
    int ret;
    size_t size = 0;

    sprintf(fname, "/proc/%d/status", getpid());
    if ((fp = fopen(fname, "r")) == NULL) return 0;
    while (fgets(line, sizeof(line), fp)) {
	ret = sscanf(line, "VmPeak: %ld kB", &size);
	if (ret == 1) break;
    }
    fclose(fp);
    return size*1024;
}

size_t
malloc_used(void)
{
    char fname[128];
    char line[128];
    FILE *fp;
    int ret;
    size_t size = 0;

    sprintf(fname, "/proc/%d/status", getpid());
    if ((fp = fopen(fname, "r")) == NULL) return 0;
    while (fgets(line, sizeof(line), fp)) {
	ret = sscanf(line, "VmRSS: %ld kB", &size);
	if (ret == 1) break;
    }
    fclose(fp);
    return size*1024;
}



/* Do not define malloc, calloc, realloc or free!!! */

#else  /* USE_SYSTEM_MALLOC */

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include "malloc.h"
#include "protos.h"
#include "Msgs.h"
#include "error.h"

#define ALIGN 8

/* Knuth's_c must be >= (sizeof(fheader_t) + sizeof(aheader_t)) */
/* See p. 438 of Knuth, and the soln to exercise 12. */
/* When equal to the above amount, */
/* the control data in a free block created */
/* at the end of a not-quite-filled block completely fills the block. */
/* Since an aheader_t is smaller than an fheader_t, you can still */
/* get it if you ask for <= (sizeof(fheader_t) - sizeof(aheader_t)) */

#define KNUTHS_C (sizeof(fheader_t) + sizeof(aheader_t))
#define FREE '-'
#define INUSE '+'

/* The CHUNKSIZE is the amount of memory allocated */
/* by each call to sbrk.  It should be reasonably large, */
/* but not so large that it is likely to fail. */
#ifdef __NCUBE1__
#define CHUNKSIZE (16*1024)
#else
#define CHUNKSIZE (256*1024*1024)
#endif

/* MAXCHUNKS is the maximum number of chunks */
/* that may be obtained.  The value is used */
/* exclusively for the size of an array with */
/* information describing each chunk.  With a */
/* little cleverness, we could chain the chunks */
/* together, but it hardly seems worth it since */
/* the only purpose of this information is to */
/* allow debugging/printing of diagnostics.  */
/* Now that chunks get coallesced when sbrk has */
/* not been called by the user, there is even less */
/* reason for MAXCHUNKS to be large... */
#define MAXCHUNKS 8

/* If any of these typedefs are changed, be sure that */
/* sizeof(header_t) and sizeof(trailer_t) both are multiples */
/* of ALIGN */

typedef int tag_t;

/* An aheader is the header at the beginning of an */
/* allocated block.  It must be identical to an fheader, */
/* except that allocated fields don't need linking info */
/* WARNING:  If this struct is changed, it must also be */
/* changed in ../stdio/prt_mem.c!!!! */
typedef struct aheader{
    tag_t tag;
    size_t size;
} aheader_t ;

typedef struct fheader{
    tag_t tag;			/* The tag is a single bit */
    size_t size;		/* it should be merged into size */
    struct fheader *link;
    struct fheader *prev;
} fheader_t;
    
typedef struct trailer{
    tag_t tag;
    size_t size;
} trailer_t;


/* Each piece of memory is preceded by a header, and followed */
/* by a trailer.  If the piece is a free block, the tag fields */
/* in the header and trailer are FREE, otherwise, they are INUSE */
/* For free blocks, the size field in the trailer is valid, but it */
/* is not necessarily valid for allocated blocks. */
/* The point of all this is to make freeing an object take */
/* constant time.  One can examine the trailer that immediately */
/* precedes the to-be-freed object, and determine whether the */
/* block immediately below should be coalesced.  Similarly for */
/* the block immediately above. */

/* The avail fheader_t is special.  Its link field */
/* points to the front of the free list.  Its prev field */
/* points to the end of the free list, and its space field */
/* contains the total available space in the free list, so */
/* mall_avail runs quickly.  Unfortunately, mall_avail can't*/
/* give any info about fragmentation.  It also doesn't */
/* call getrlimit, although it might be useful to ask the */
/* OS exactly how much farther we will be able to extend */
/* memory. */

/* Export */
/* In addition to malloc, calloc, realloc, free */
char malloc_errstring[256];
int malloc_verify(void);
void malloc_print(void);
int malloc_debug(int);

/* The avail fheader_t is special.  Its link field */
/* points to the front of the free list.  Its prev field */
/* points to the end of the free list, and its space field */
/* contains the total available space in the free list, so */
/* mall_avail runs quickly.  Unfortunately, mall_avail can't*/
/* give any info about fragmentation. */

static fheader_t avail = {
    FREE,			/* tag */
    0,				/* size */
    &avail,			/* link */
    &avail			/* prev */
};

/* The last entry in the chunk_tbl is special */
struct _chunk_desc{
    size_t size;
    void *begin;
};

static fheader_t *rover = &avail;
static size_t chunksize = CHUNKSIZE;
static int debug_lev = 1;
static int debug_verify;
static int debug_abort_nomem;
static int debug_test;
static fheader_t *top_of_mem, *bottom_of_mem;
static int n_free_blocks;
static struct _chunk_desc _chunk_tbl[MAXCHUNKS + 1];
static int _nchunks;

static int extend_mem(size_t n);
static int verify_blk(fheader_t *p);

/* We use these system-dependent functions. */
extern void *sbrk(int incr);
extern int brk(void *addr);
static void *getmaxbrk(void);

void
*calloc(size_t nmemb, size_t size)
{
    void *p;

    /* What are we supposed to do here???  Should we round up the size */
    /* so all elements are aligned?   I don't think so, but I'm not */
    /* confident. */
/* 
    if(size > 1 && size % ALIGN)
	size += ALIGN - size%ALIGN;
*/

    p = malloc(size*nmemb);
    if(p)
	(void)memset(p, 0, size*nmemb);
    return p;
}

void
*malloc(size_t size)
/* Implement Knuth's Algorithm A, modified for doubly linked */
/* lists, and using the A4' step to avoid small blocks. */
/* It may be found on p. 598 in the soln. to problem 12. */
{
    size_t N, k;
    int looped;
    fheader_t *p, *p1;
    aheader_t *L;
    trailer_t *trl;
    size_t askfor;
    int got;

    if(debug_verify && malloc_verify()){
	Error("bad malloc structures: %s\n", malloc_errstring);
    }

    if(size == 0)
	return (void *)0;

    if(size%ALIGN)
	size += ALIGN - size%ALIGN;

    /* If we allocate a block that's too small, we will have */
    /* BIG problems when we try to free it!! */
    if(size + sizeof(aheader_t) < sizeof(fheader_t))
	N = sizeof(fheader_t) + sizeof(trailer_t);
    else
	N = size + sizeof(aheader_t) + sizeof(trailer_t);
    p = rover;
    looped = 0;
    while(p->size < N || p == &avail){
	if(p == &avail){
	    if(looped){
		askfor = (N<chunksize) ? chunksize : N;
		got = extend_mem(askfor);
		/* If we couldn't get a full-size */
		/* chunk, remember that, and don't */
		/* ask again. */
		if(got < (int)askfor){
		    /* We have exhausted all our memory. */
		    chunksize = 0;
		}
		/* The cast is important here, so the */
		/* comparison is signed */
		if(got < (int)N){
		    /* We couldn't get what we needed */
		    if(debug_abort_nomem) {
			malloc_print();
			Error("Out of memory in malloc\n");
		    }
		    return (void *)0;
		}
		/* extend_mem will link in a block immediately */
		/* after avail, so we will find it in the */
		/* next time around the outside loop */
	    }else{
		looped = 1;
		p = avail.link;	/* ????????? */
		continue;
	    }
	}
	p = p->link;
    }

    if(debug_test && verify_blk(p)){
	Error("Bad block (%#lx) in malloc: %s\n", 
	      (unsigned long)p, malloc_errstring);
	return (void *)0;
    }
    rover = p->link;
    k = p->size - N;
#if 0
    /* This code allocates the new block at the top of */
    /* the free block.  This has very bad consequences */
    /* for fragmentation when sbrk is called many times */
    /* to get new chunks.  Thus, the improved code below. */
    if(k < KNUTHS_C){
	avail.size -= p->size;
	p->prev->link = rover;
	rover->prev = p->prev;
	L = (aheader_t *)p;
	n_free_blocks--;
    }else{
	/* Here, we split the large block */
	avail.size -= N;
	p->size = k;
	L = (aheader_t *)((char *)p + k);
	trl = (trailer_t *)L - 1;
	trl->size = k;
	trl->tag = FREE;
	L->size = N;
    }
    L->tag = INUSE;
    trl = (trailer_t *)((char *)L + L->size) - 1;
    trl->tag = INUSE;
    trl->size = L->size;
#else
    L = (aheader_t *)p;
    if(k < KNUTHS_C){
	avail.size -= p->size;
	p->prev->link = rover;
	rover->prev = p->prev;
	n_free_blocks--;
    }else{
	/* Here, we split a large free block into an allocated block */
	/* and a smaller free block. */
	/* Knuth's method is considerably easier, since */
	/* it leaves the free block essentially alone.  There */
	/* is no need to poke around in the free list and fix pointers */
	/* Undoubtedly, the correct thing is to make the linked lists */
	/* out of the trailers instead...  Maybe in some other life */
	avail.size -= N;
	L->size = N;
	p1 = (fheader_t *)((char *)L + N);
	p1->tag = FREE;
	p1->size = k;
	p1->link = p->link;
	p1->prev = p->prev;
	p->prev->link = p1;
	p->link->prev = p1;
	trl = (trailer_t *)((char *)p1 + k) - 1;
	trl->size = k;
	/* the tag is FREE already */
    }
    L->tag = INUSE;
    trl = (trailer_t *)((char *)L + L->size) - 1;
    trl->tag = INUSE;
    trl->size = L->size;
#endif
    return (void *)(L+1);
}

void
*realloc(void *ptr, size_t size)
{
    void *ret;
    aheader_t *p0;
    fheader_t *p, *p1, *p2;
    trailer_t *trl, *trl0;
    size_t N;
    size_t ptrsz;
    int k;

    if(debug_verify && malloc_verify()){
	Error("Bad structures in realloc: %s\n", malloc_errstring);
	return (void *)0;
    }

    if(size == 0){
	if(ptr)
	    free(ptr);
	return (void *)0;
    }

    if(ptr == (void *)0)
	return malloc(size);

    if(size%ALIGN)
	size += ALIGN - size%ALIGN;

    /* If we allocate a block that's too small, we will have */
    /* BIG problems when we try to free it!! */
    if(size + sizeof(aheader_t) < sizeof(fheader_t))
	N = sizeof(fheader_t) + sizeof(trailer_t);
    else
	N = size + sizeof(aheader_t) + sizeof(trailer_t);

    /* This code is very similar to that in free. */
    /* They should probably be combined in a common subroutine */

    p0 = (aheader_t *) ((aheader_t *)ptr - 1);
    /* How much data is actuall stored under ptr??? */
    /* I think this is correct even for 'small' N */
    ptrsz = p0->size - (sizeof(aheader_t) + sizeof(trailer_t));
    p = (fheader_t *)((char *)p0 + p0->size);
    if(debug_test && verify_blk(p)){
	Error("Bad blk (%#lx) in realloc: %s\n", 
	      (unsigned long)p, malloc_errstring);
	return (void *)0;
    }
    if(p->tag == FREE){
	/* The next block is free.  Combine them */
	p1 = p->link;
	p2 = p->prev;
	p1->prev = p2;
	p2->link = p1;
	p0->size += p->size;
	avail.size -= p->size;
	/* p just got removed from the free list. */
	/* Make sure it's not equal to rover */
	if(p == rover)
	    rover = &avail;
	p = (fheader_t *)( (char *)p + p->size );
	trl = (trailer_t *)p - 1;
	trl->tag = INUSE;
	trl->size = p0->size;
	n_free_blocks--;
    }

    trl = (trailer_t *)p0 -1;
#ifdef UNFRAGMENT    
    /* Combine this block and the one before it whenever possible */
    if(trl->tag == FREE && (trl->size + p0->size) > N){
#else
    /* Only combine this block with the one before it if we don't have */
    /* enough space yet. */
    if(trl->tag == FREE && (p0->size < N) && (trl->size + p0->size) > N){
#endif
	/* In the event that the previous blk is free, and it doesn't meet */
	/* our needs, we will coalesce it when the current block */
	/* is freed, postponing any irreversible damage until after */
	/* new space has been successfully found */
	p = (fheader_t *)((char *)p0 - trl->size);
	p1 = p->link;
	p2 = p->prev;
	p1->prev = p2;
	p2->link = p1;
	avail.size -= p->size;
	p->size += p0->size;
	/* p just got removed from the free list. */
	/* Make sure it's not equal to rover */
	if(p == rover)
	    rover = &avail;
	/* Copy the data from p0 to p */
	if( (char *)((aheader_t*)p+1) < (char *)ptr - ptrsz )
	    memcpy(((aheader_t *)p+1), ptr, ptrsz);
	else
	    memmove(((aheader_t *)p+1), ptr, ptrsz);
	p->tag = INUSE;
	p0 = (aheader_t *)p;
	trl0 = ((trailer_t *)((char *)p + p->size)) -1;
	trl0->size = p->size;
	n_free_blocks--;
    }

    if(debug_test && verify_blk((fheader_t *)p0)){
	Error("Bad block (p0) (%#lx) in realloc: %s\n", 
	      (unsigned long)p0, malloc_errstring);
	return (void *)0;
    }
    k = p0->size - N;
    if(k<0){
	ret = malloc(size);
	if(ret){
	    memcpy(ret, ptr, p0->size - sizeof(aheader_t)-sizeof(trailer_t));
	    free(ptr);
	}else if(debug_abort_nomem){
	    Error("Out of mem in realloc\n");
	}
	/* Fortunately, we didn't combine the preceding block */
	/* with the given block unless we were sure that would */
	/* result in enough space.  Thus, we didn't molest the data */
	/* or the list structures in any way that would need to */
	/* be reversed. */
	return ret;
    }else if(k >= KNUTHS_C){
	/* First, shrink this node. */
	p0->size = N;
	p1 = (fheader_t *)((char *)p0 +N);
	trl = (trailer_t *)p1 - 1;
	trl->tag = INUSE;
	trl->size = N;
	/* then create a new free node at the top of this block */
	n_free_blocks++;
	avail.size += k;
	p1->size = k;
	p1->tag = FREE;
	p1->link = avail.link;
	p1->prev = &avail;
	avail.link->prev = p1;
	avail.link = p1;
	trl = (trailer_t *) ((char *)p1 + k) -1;
	trl->tag = FREE;
	trl->size = k;
    }/* else there is nothing to do.  The change is not significant */
    return (void *)((aheader_t *)p0 + 1);
}

void
free(void *ptr)
/* Implement Knuth's Algorithm C, p. 442 */
{
    fheader_t *p, *p0, *p1, *p2;
    trailer_t *trl;

    if(debug_verify && malloc_verify()){
	Error("Bad structures in free: %s\n", malloc_errstring);
	return;
    }

    if(ptr == (void *)0)
	return;

    p0 = (fheader_t *) ((aheader_t *)ptr - 1);
    if(debug_test && verify_blk(p0)){
	Error("Bad block (%#lx) in free: %s\n", 
	      (unsigned long)p0, malloc_errstring);
    }
    trl = (trailer_t *)p0 -1;
    avail.size += p0->size;
    if(trl->tag == FREE){
	/* The preceding block is free.  Combine them */
	p = (fheader_t *)((char *)p0 - trl->size);
	p1 = p->link;
	p2 = p->prev;
	p1->prev = p2;
	p2->link = p1;
	p->size += p0->size;
	/* p just got removed from the free list. */
	/* Make sure it's not equal to rover */
	if(p == rover)
	    rover = &avail;
	p0 = p;
	n_free_blocks--;
    }

    p = (fheader_t *)((char *)p0 + p0->size);
    if(debug_test && verify_blk(p)){
	Error("Bad block (p) (%#lx) in free: %s\n", 
	      (unsigned long)p, malloc_errstring);
    }
    if(p->tag == FREE){
	/* The anteceding block is free.  Combine them */
	p1 = p->link;
	p2 = p->prev;
	p1->prev = p2;
	p2->link = p1;
	p0->size += p->size;
	/* p just got removed from the free list. */
	/* Make sure it's not equal to rover */
	if(p == rover)
	    rover = &avail;
	p = (fheader_t *)( (char *)p + p->size );
	n_free_blocks--;
    }

    /* link the new block into the front of the avail list */
    trl = (trailer_t *)p - 1;
    trl->size = p0->size;
    trl->tag = FREE;
    p0->link = avail.link;
    p0->prev = &avail;
    avail.link->prev = p0;
    avail.link = p0;
    p0->tag = FREE;
    n_free_blocks++;
}


size_t 
malloc_avail(void)
{
    /* The space unallocated in the blocks we have appropriated, */
    /* minus the space left that the OS will give us. */
    /* If the OS won't tell us, we have to return -1, which is */
    /* more likely to mean we don't know than that there is none! */
    void *memlim = getmaxbrk();
    void *curtop = sbrk(0);
    
    if((long)memlim == -1 || (long)curtop == -1)
	return -1;
    return avail.size + ((char *)memlim - (char *)curtop);
}

size_t 
malloc_heapsz(void)
{
    void *curtop = sbrk(0);
    return((long)curtop - (long)_chunk_tbl[0].begin);
}

size_t 
malloc_used(void)
{
    return malloc_heapsz()-avail.size;
}


static int
extend_mem(size_t n)
/* Try to obtain a block of at least size n */
/* from the OS using sbrk.  Return the size */
/* of the block actually obtained. */
{
    trailer_t *trl0;
    aheader_t *hdr0;
    aheader_t *hdr1;
    fheader_t *hdr;
    trailer_t *trl;
    char *old_top;
    void *max_brk, *begin;

    Msgf(("extend_mem(%ld)\n", (long)n));
    /* Check structure sizes */
    if (sizeof(aheader_t) % ALIGN != 0)
      Error("sizeof(aheader_t) (%d) not multiple of ALIGN (%d)\n", 
	    (int)sizeof(aheader_t), ALIGN);
    if (sizeof(aheader_t) != sizeof(trailer_t))
      Error("sizeof(aheader_t) (%d) != sizeof(trailer_t) (%d)\n",
	    (int)sizeof(aheader_t), (int)sizeof(trailer_t));
    /* First, try to get a chunk of memory */
    /* If the system won't give us one of the */
    /* size we ask for, then halve the size */
    /* until we get something */

    /* Technically, sbrk returns a caddr_t, which */
    /* is typedef'ed to be a char *.  It returns */
    /* an error condition, however, by returning the */
    /* integer -1, so the casts are necessary. */
    while (1) {
	if(n < (sizeof(fheader_t) + sizeof(trailer_t))) {
	    Error("no more memory!\n");
	}
	begin = sbrk(n + sizeof(aheader_t) +sizeof(trailer_t));
	if ((long int)begin != -1L) break;
	Msgf(("sbrk(%ld) returns -1\n", 
	      (long)(n+sizeof(aheader_t) + sizeof(trailer_t))));
	n = n/2;
    }
    old_top = (char *)top_of_mem;
    if(bottom_of_mem == 0){
	/* The first time through here, remember the lowest possible */
	/* malloc'ed location */
	bottom_of_mem = (fheader_t *)(begin);
    }
    /* Assume sbrk is strictly increasing, so the end of memory */
    /* is represented by the end of the most recent sbrk call */
    top_of_mem = (fheader_t *)((char*)begin + n+sizeof(aheader_t)+sizeof(trailer_t));

    if(begin == old_top){
	/* put a fake INUSE block at the top */
	Msgf(("Adding extended mem to previous chunk\n"));
	hdr1 = ((aheader_t *)top_of_mem) - 1;
	hdr1->size = sizeof(aheader_t);
	hdr1->tag = INUSE;
	/* Now change the hdr that used to be at the top of memory */
	hdr0 = ((aheader_t *)old_top) - 1;
	hdr0->tag = INUSE;
	hdr0->size = n + sizeof(aheader_t) + sizeof(trailer_t);
	/* Now put a consistent trailer at the end of it */
	trl0 = ((trailer_t *)hdr1) - 1;
	trl0->tag = INUSE;
	trl0->size = hdr0->size;
	/* And now free it to connect it with the free-list */
	/* This might change trl0->size */
	free((void *)(hdr0 + 1));
	/* Finally, record that the chunk has grown */
#if 0				/* wrong? */
	_chunk_tbl[_nchunks-1].size += n + sizeof(trailer_t);
#else
	_chunk_tbl[_nchunks-1].size += n+sizeof(aheader_t)+sizeof(trailer_t);
#endif
	Msgf(("extend_mem returns %lu\n", (unsigned long)trl0->size));
	return trl0->size;
    }

    /* Now we've got a big chunk of memory */
    /* Prepare it by placing INUSE markers at */
    /* both ends, and a header after the INUSE */
    /* marker at the beginning. */
    trl0 = (trailer_t *)begin;
    trl0->tag = INUSE;
    hdr0 = (aheader_t *)((char *)begin + n + sizeof(trailer_t));
    hdr0->tag = INUSE;
    /* WARNING!  These two lines will keep verify_blk happy */
    /* if it is asked to verify the markers at the beginning */
    /* and end of memory.  They RELY ON THE FACT that: */
    /* sizeof(header_t) == sizeof(trailer_t) */
    hdr0->size = sizeof(aheader_t);
    trl0->size = sizeof(trailer_t);
    hdr = (fheader_t *)(trl0 + 1); /* the header of the new free block */
    trl = ((trailer_t *)hdr0) - 1; /* the trailer of the new free block */

    /* Now link the header into the free chain */
    hdr->link = avail.link;
    hdr->prev = &avail;
    avail.link->prev = hdr;
    avail.link = hdr;
    hdr->tag = FREE;
    hdr->size = n;
    trl->tag = FREE;
    trl->size = n;
    avail.size += n;
    n_free_blocks++;

    /* Now record info about this chunk in the table */
    if(_nchunks < MAXCHUNKS){
	_chunk_tbl[_nchunks].size = n;
	_chunk_tbl[_nchunks].begin = (void *)hdr;
	_nchunks++;
    }else{
	/* There's no more room in the table! */
	/* Don't panic, though, just give up on */
	/* recording the sizes of individual chunks for */
	/* posterity. */
	_chunk_tbl[MAXCHUNKS].size += n;
    }

    Msgf(("extend_mem returns %lu\n", (unsigned long)n));
    return n;
}

/* These two are patterned after the diagnostic version of */
/* malloc available on SUNs.  Malloc_debug(lev) sets the debugging */
/* level to its arg.  Values are: */
/* 0 : the default case */
/* 1 : abort if any problem is detected in malloc's data structures */
/*     Level 1 does not actively seek out problems, but if it happens upon */
/*     one, it calls abort.  This is the default level.  It should */
/*     be only marginally slower than level 0. */
/* 2 : Examine the entire data structure on every call of malloc/calloc/ */
/*     realloc/free. This may be VERY slow. */
/* 3 : Same as 1, but also abort when malloc would return NULL. */
/* 4 : Same as 2, but also abort when malloc would return NULL. */
/*     Neither of the last two abort if 0 bytes are requested */
/* malloc_verify() performs a very thorough of the entire malloc */
/*     data structures.  If all is well, it returns 0, otherwise it */
/*     returns 1 */
/* Whenever an error is detected, the external (void *) malloc_bad_block */
/*     is set to point to the bad block.  It points to the value that */
/*     would have been returned by malloc, i.e. sizeof(aheader_t) past */
/*     the address of the header.  If the error is a global one that */
/*     cannot be blamed on a single block, e.g. sizes not adding up, */
/*     malloc_bad_block points to the static avail. */
int
malloc_debug(int level)
{
    int ret;

    if(level > 4 || level < 0)
	return -1;

    ret = debug_lev;
    debug_lev = level;
    switch(debug_lev){
    case 0:
	debug_test = 0;
	debug_verify = 0;
	debug_abort_nomem = 0;
	break;
    case 1:
	debug_test = 1;
	debug_verify = 0;
	debug_abort_nomem = 0;
	break;
    case 2:
	debug_test = 1;
	debug_verify = 1;
	debug_abort_nomem = 0;
	break;
    case 3:
	debug_test = 1;
	debug_verify = 0;
	debug_abort_nomem = 1;
	break;
    case 4:
	debug_test = 1;
	debug_verify = 1;
	debug_abort_nomem = 1;
	break;
    }
    return ret;
}

int
malloc_verify(void)
{
    fheader_t *last;
    fheader_t *hdr;
    int i;
    int n_free_blocks1 = 0;
    int n_free_blocks2 = 0;
    size_t sz_free = 0;
   
    /* This is very similar to the loop in prt_mem... */
    /* we loop over all chunks, and all blocks in the chunk, */
    /* calling verify_blk for each one.   We also count the */
    /* number of free blocks, which we compare with a scan down the */
    /* linked list of free blocks starting at avail. */
    for(i=0; i<_nchunks; i++){
	hdr = (fheader_t *)_chunk_tbl[i].begin;
	last = (fheader_t *)((char *)hdr + _chunk_tbl[i].size);
	while(hdr < last){
	    if(verify_blk(hdr)){
		return 1;
	    }
	    if(hdr->tag == FREE){
		n_free_blocks1++;
		sz_free += hdr->size;
	    }
	    /* Avoid infinite loops by counting free blocks */
	    if(n_free_blocks1 > n_free_blocks){
		sprintf(malloc_errstring, "nfree_blocks1(%d) > nfreeblocks(%d)\n",
			n_free_blocks1, n_free_blocks);
		return 1;
	    }
	    hdr = (fheader_t *)((char *)hdr + hdr->size);
	}
	if( hdr != last ){
	    SeriousWarning("Possible too-long chunk:  hdr=%#lx != last=%#lx\n",
			   (unsigned long)hdr, (unsigned long)last);
	}
    }
    
    /* Only make this test if the chunk_tbl contains all the relevant */
    /* information */
    if((_nchunks != MAXCHUNKS) &&
       (sz_free != avail.size || n_free_blocks != n_free_blocks1)){
	sprintf(malloc_errstring, "Sizes don't add up, sz_free=%ld, avail.size=%ld, n_free_blocks=%d, n_free_blocks1=%d!\n",
		(long)sz_free, (long)avail.size, 
		n_free_blocks, n_free_blocks1);
	SeriousWarning("%s", malloc_errstring);
	/* return 1; */
    }

    /* Now scan the linked list of free blocks and make sure */
    /* it is the right length, and that its size adds up too */
    sz_free = 0;
    for(hdr = avail.link; hdr != &avail; hdr = hdr->link){
	n_free_blocks2++;
	sz_free += hdr->size;
	/* avoid infinite loops this way */
	if(n_free_blocks2 > n_free_blocks1){
	    sprintf(malloc_errstring, "Too many links in chain\n");
	    return 1;
	}
    }
    if(sz_free != avail.size || n_free_blocks2 != n_free_blocks){
	sprintf(malloc_errstring, "Sizes or counts don't add up!\n");
	return 1;
    }

    return 0;
}

static int
verify_blk(fheader_t *p)
{
    fheader_t *l;
    trailer_t *tp;
    /* rely on the two variables top_of_mem and bottom_of_mem */
    /* being set before entering here... */
    /* How slow is this?????  Is it unreasonable to call it */
    /* every time through the linked list in malloc??? */
    if(p >= top_of_mem || p < bottom_of_mem){
	sprintf(malloc_errstring, "ptr (%#lx) outside of memory\n", 
		(unsigned long)p);
	return 1;
    }
    
    tp = ((trailer_t *)((char *)p + p->size)) - 1;
    /* make sure the size isn't preposterous */
    if((fheader_t *)tp > top_of_mem){
	sprintf(malloc_errstring, "block goes past (%#lx) top of memory\n", 
		(unsigned long)tp);
	return 1;
    }

    /* Verify that the flag is ok */
    if((p->tag != FREE && p->tag != INUSE)){
	sprintf(malloc_errstring, 
		"Bad magic byte in hdr %#lx\n", (unsigned long)p);
	return 1;
    }
    
    /* Verify that the header and trailer match */
    if(p->tag != tp->tag){
	sprintf(malloc_errstring, 
		"Tags don't match %#lx, %#lx\n", (unsigned long)p, (unsigned long)tp);
	return 1;
    }

    /* Check that the sizes agree in header and trailer */
    if(p->size != tp->size){
	sprintf(malloc_errstring, "Sizes don't match %#lx, %#lx\n", 
		(unsigned long)p, (unsigned long)tp);
	return 1;
    }

    /* Check that the size is reasonable */
    /* Accepting sizes exactly equal to sizeof(trailer_t) */
    /* allows the "dummy" headers at both ends of each chunk */
    /* to pass.  It hardly seems worth the effort of looking through */
    /* the chunk_tbl, to verify that we are actually looking at */
    /* such a block */
    if(p->size < sizeof(fheader_t) + sizeof(trailer_t) &&
       p->size != sizeof(trailer_t)){
	sprintf(malloc_errstring, "Size doesn't make sense p=%#lx\n", 
		(unsigned long)p);
	return 1;
    }

    /* Verify that the forward and backward pointers are reasonable */
    if(p->tag == FREE){
	l = p->link;
	if(l != &avail && (l >= top_of_mem || l < bottom_of_mem)){
	    sprintf(malloc_errstring, "Link ptr (%#lx) of %#lx out of range\n",
		    (unsigned long)l, (unsigned long)p);
	    return 1;
	}
	
	l = p->prev;
	if(l != &avail && (l >= top_of_mem || l < bottom_of_mem)){
	    sprintf(malloc_errstring, "Prev ptr (%#lx) of %#lx out of range\n",
		    (unsigned long)l, (unsigned long)p);
	    return 1;
	}
    }
    
    /* Amazing, all is well. */
    return 0;
}


void
malloc_print(void)
/* Print the map of allocated memory.  */
/* Beware that printf might call malloc if */
/* the buffer for the file, fp, is extensible */
/* This will have dire consequences.  */
/* For the moment, a solution is to guarantee that */
/* fprintf is linked with any paralib program via */
/* the trick in stdio/data.c */
{
    aheader_t *last;
    aheader_t *hdr;
    fheader_t *fhdr;
    int ch, i;
    int nfb;
   
    Msg_do("Malloc_print called.  heapsz: %ld, avail: %ld, used: %ld\n",
	   (long)malloc_heapsz(), (long)malloc_avail(), (long)malloc_used());
    Msg_do( "Memory map:\n");
    for(i=0; i<_nchunks; i++){
	Msg_do( "Chunk %d of size %lu\n", 
		i, (unsigned long)_chunk_tbl[i].size);
	Msg_do( "address size [allocated|free]\n");
	hdr = (aheader_t *)_chunk_tbl[i].begin;
	last = (aheader_t *)((char *)hdr + _chunk_tbl[i].size);
	while(hdr < last){
	    if( verify_blk((fheader_t *)hdr) ){
		Msg_do( "Bad block (%#lx): %s\n", 
		       (unsigned long)hdr, malloc_errstring);
		break;
	    }
	    /* ch is 'f' for free, 'a' for allocated */
	    /* We print out the address that would have been */
	    /* returned by malloc, and the maximum possible */
	    /* size of USER memory in the block. Due to */
	    /* rounding, this may be more than he asked for, */
	    /* but the size of any headers and/or trailers is */
	    /* subtracted.  Note the use of the %p conversion */
	    /* specifier. */
	    ch = (hdr->tag == INUSE)? 'a' : 'f';
	    Msg_do( "%#lx %lu %c\n", (unsigned long)(hdr+1), 
		    (unsigned long)(hdr->size - sizeof(aheader_t) - sizeof(trailer_t)),
		    ch);
	    hdr = (aheader_t *)((char *)hdr + hdr->size);
	}
	Msg_do( "\n");
    }
    if(_nchunks == MAXCHUNKS){
	Msg_do( "There are more chunks with\n");
	Msg_do( "a total size of %lu.  I don't\n", 
		(unsigned long)_chunk_tbl[MAXCHUNKS].size);
	Msg_do( "have detailed info about them though.\n");
    }
    Msg_do("Free list:\n");
    nfb = 0;
    for(fhdr = avail.link; fhdr != &avail; fhdr = fhdr->link){
	nfb++;
	ch = (fhdr->tag == INUSE)? 'a' : 'f';
	if( verify_blk(fhdr) ){
	    Msg_do("Bad block (%#lx): %s\n", (unsigned long)fhdr, malloc_errstring);
	}
	Msg_do( "%#lx %lu %c\n", (unsigned long)((aheader_t *)fhdr+1), 
	       (unsigned long)(fhdr->size - sizeof(aheader_t) - sizeof(trailer_t)),
	       ch);
	/* avoid infinite loops this way */
	if(nfb > n_free_blocks){
	    Msg_do("Too many free blocks.  Possible loop\n");
	    break;
	}
    }
    
}

/* This uses the same discredited idea that we abandoned */
/* in the tree11/sysdep.c:  define a GETMAXBRK_DEFINED symbol */
/* as soon as we find a system-predicate we like.  When we hit the */
/* end, complain if we haven't yet defined GETMAXBRK_DEFINED. */
#undef GETMAXBRK_DEFINED

#if defined(__hpux)
#define GETMAXBRK_DEFINED
#include <ulimit.h>
static void *getmaxbrk(void){
    (void *)ulimit(UL_GETMAXBRK);
}
#endif

#if defined(sun)||defined(__PARAGON__)||defined(linux)
/* is getrlimit a sunos'ism, a bsd'ism or what??? */
#define GETMAXBRK_DEFINED
#include <sys/time.h>
#include <sys/resource.h>

extern char etext;

static void *
getmaxbrk(void)
{
    struct rlimit rl;

    if(getrlimit(RLIMIT_DATA, &rl))
	return (void *)-1;
    if(rl.rlim_max == RLIM_INFINITY){
	return (void *)-1;
    }else{
	return &etext + 2750LL*1024LL*1024LL;
    }
}

#endif /* sun||PARAGON */

#ifdef __DELTA__
#define GETMAXBRK_DEFINED
#define BEGIN 0x10000000
static void *
getmaxbrk(void){
    return (void *)(BEGIN + 12*1024*1024); /* 12Meg */
}
#endif

#ifndef GETMAXBRK_DEFINED
static void *
getmaxbrk(void)
{
    return (void *)-1;
}
#endif /* GETMAXBRK_DEFINED */

#endif /* USE_SYSTEM_MALLOC */

