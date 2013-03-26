/* little functions for doing gray-code stuff. */

#include <limits.h>
#include <sys/types.h>
#include "gc.h"

/* Both parity and firstbit could be sped up with some lookup tables. */
/* Who cares? */
unsigned int parity(unsigned int num)
{
    unsigned int answer = 0;
    while( num ){
	if( num&1 )
	    answer ^= 1;
	num >>= 1;
    }
    return answer;
}

int hibit(unsigned int num){
    /* return the index of the highest bit in num. */
    /* -1 if num == 0 */
    int bit = -1;

    while(num) {
	bit++;
	num >>= 1;
    }
    return bit;
}

int lobit(unsigned int num){
    /* Return the index of the lowest bit in num */
    /* Return BITS_PER_WORD in if num==0.  This relies on overflow in left-
     shift returning 0 */
    unsigned int m=1;
    int bit = 0;

    while((num&m) != m){
	m <<= 1;
	bit++;
    }
    return bit;
}

int ilog2(unsigned int n){
    return hibit(n);
}

unsigned int cksum(const void *buf, unsigned int n){
    unsigned int sum = 0;
    unsigned int leftover;
    const unsigned int *ip = buf;
    const unsigned char *cp;

    /* Worthwhile to make the result independent of word-ordering, wordsize,
       etc??  Not now... */
    leftover = n%sizeof(unsigned int);
    n /= sizeof(unsigned int);
    while(n--){
	sum ^= *ip++;
    }
    cp = (const unsigned char *)ip;
    for(n=0; n<leftover; n++){
	sum ^= (*cp++) << (n*CHAR_BIT);
    }

    return sum;
}

unsigned int countbits(unsigned int n){
    unsigned int ret = 0;
    do{
	if( n&1 ) ret++;
    }while((n >>= 1));
    return ret;
}

/* My two neighbors are obtained by :
   a) toggle the lowest bit of procnum
   b) toggle one past the lowest turned-on bit
   ( but be careful when there are no turned on bits, or only the highest 
   is turned on).
   */
int Gcup(unsigned int proc, unsigned int nproc){
    if( proc == nproc>>1 ){
	return -1;
    }else if( parity(proc) ){
	return proc^(1<<lobit(proc));
    }else{
	return proc^1;
    }
}

int Gcdown(unsigned int proc, unsigned int nproc){
    if(proc == 0){
	return -1;
    }else if(parity(proc)){
	return proc^1;
    }else{
	return proc^(1<<lobit(proc));
    }
}

void NobjInitial(int gnobj, int nproc, int procnum, int *nobj, int *start)
{
    int leftover;

    *nobj = gnobj / nproc;
    *start = (*nobj)*procnum;
    leftover = gnobj - (*nobj)*nproc;
    if (procnum < leftover){
	++(*nobj);
	*start += procnum;
    }else{
	*start += leftover;
    }
}

void NobjInitial64(int64_t gnobj, int nproc, int procnum, int *nobj, int64_t *start)
{
    int leftover;

    *nobj = gnobj / nproc;
    *start = (int64_t)(*nobj)*procnum;
    leftover = gnobj - (*nobj)*nproc;
    if (procnum < leftover){
	++(*nobj);
	*start += procnum;
    }else{
	*start += leftover;
    }
}

/* Both these routines came from alt.sources.  */
/* From: eillihca@drizzle.StanFord.EDU ( Achille Hui, the Day Dreamer ) */
unsigned long
bin2gray(unsigned long b){
	return b ^ (b>>1);
}

/* If you look back at the property of ^ and >>, you will notice ^ behave
   like an addtion:

	   x + y = y + x            <->         x ^ y = y ^ x
     (x + y) + z = x + (y + z)      <->   (x ^ y) ^ z = x ^ (y ^ z)

   and >> behave like a linear operator L:

        L(x + y) = L(x) + L(y)      <->    (x ^ y)>>1 = (x>>1) ^ (y>>1)

   Hence the bin2gray operation behave just like the linear operator:

			1 + L                   

   with inverse 
         -1            2   3                     2       4       8
     (1+L)  = 1 - L + L - L + ... = (1 - L)(1 + L )(1 + L )(1 + L )...  (*)

   Since x ^ y ^ y = x, addition is just the same as subaction.  Furthurmore,
   x>>32 = 0 for any 32 bit integer and hence all of
              32          64                     
	(1 + L  ) , (1 + L  ), .... equal to 1.

   As a result, your just need to keep the first 5 terms of RHS of (*)
*/
unsigned long
gray2bin(unsigned long g){
	register unsigned long t = g;
	t ^= t>>1;			/* 1 - L   */
					/*      2  */
	t ^= t>>2;			/* 1 + L   */
					/*      4  */
	t ^= t>>4;                      /* 1 + L   */
	                                /*      8  */
	t ^= t>>8;                      /* 1 + L   */
                                        /*      16 */
	t ^= t>>16;                     /* 1 + L   */
	return t;
}
