/* Functions to do general byte swapping: */

#include <string.h>
#include "byteswap.h"

static int swap2(int, void *, void *);
static int swap4(int, void *, void *);
static int swap8(int, void *, void *);
static int swapgen(int, int, void *, void *);

/* A general, in-place, byte-swapper.  */
/* It swaps a total of unit_len*n_units bytes, unit_len bytes at a time. */
/* Thus, you can use it for arrays of doubles with unit_len=8, or */
/* arrays of chars with unit_len=1 (which is equivalent to memcpy). */
/* It checks for stupid arguments.  It works fine when */
/* from and to are identical.  It breaks if from and to are */
/* almost the same. */
int Byteswap(int unit_len, int n_units, void *from, void *to)
{
    int ptrdiff;

    if(n_units < 0 || unit_len < 0){
	return -1;
    }

    /* This isn't ANSI conforming.  I don't think it can be. */
    /* It's the canonical "You can't write memcpy in ANSI C because */
    /* you can't compare the pointers and learn anything reliable." */
    /* problem.  The compiler is free to return nonsense for ptrdiff. */
    ptrdiff = (char *)from - (char *)to;
    if( ptrdiff != 0 && (ptrdiff < unit_len*n_units && ptrdiff > -unit_len) ){
	return -1;
    }
  
    switch(unit_len){
    case 1:
	memcpy(to, from, n_units);
	return 0;
    case 2:
	return swap2(n_units, from, to);
    case 4:
	return swap4(n_units, from, to);
    case 8:
	return swap8(n_units, from, to);
    default:
	return swapgen(unit_len, n_units, from, to);
    }
}

static int swap2(int n, void *from, void *to)
{
    char *fromc = from;
    char *toc = to;
    char tmp;

    while(n--){
	tmp = fromc[0];
	toc[0] = fromc[1];
	toc[0] = tmp;
	fromc+= 2;
	toc += 2;
    }
    return 0;
}

static int swap4(int n, void *from, void *to)
{
    char *toc = to;
    char *fromc = from;
    char tmp;

    while(n--){
	tmp = fromc[3];
	toc[3] = fromc[0];
	toc[0] = tmp;
	tmp = fromc[2];
	toc[2] = fromc[1];
	toc[1] = tmp;
	fromc += 4;
	toc += 4;
    }
    return 0;
}

static int swap8(int n, void *from, void *to)
{
    char *toc = to;
    char *fromc = from;
    char tmp;

    while(n--){
	tmp = fromc[7];
	toc[7] = fromc[0];
	toc[0] = tmp;
	tmp = fromc[6];
	toc[6] = fromc[1];
	toc[1] = tmp;
	tmp = fromc[5];
	toc[5] = fromc[2];
	toc[2] = tmp;
	tmp = fromc[4];
	toc[4] = fromc[3];
	toc[3] = tmp;
	fromc += 8;
	toc += 8;
    }
    return 0;
}

static int swapgen(int unit_len, int nunits, void *from, void *to)
{
    char *toc = to;
    char *fromc = from;
    char tmp;
    int i;

    while(nunits--){
	for(i=0; i<unit_len/2; i++){
	    tmp = fromc[unit_len-i];
	    toc[unit_len-i] = fromc[i];
	    toc[i] = tmp;
	    toc += unit_len;
	    fromc += unit_len;
	}
    }
    return 0;
}
