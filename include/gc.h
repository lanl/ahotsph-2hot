#ifndef _GCdotH
#define _GCdotH

#include <sys/types.h>

/* little functions for doing gray-code stuff. */

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

/* Return the parity of num, i.e., parity(0x22)=0 */
unsigned int parity(unsigned int num);

/* Return the index of the highest bit in num, i.e., 
   hibit(3) = 1, hibit(1)=0, hibit(513)=9, hibit(0)=-1 */
int hibit(unsigned int num);

/* Return the index of the lowest bit in num, i.e., 
   lobit(3) = 0, lobit(2)=1, lobit(512)=9, lobit(513)=0, lobit(0)=BITSPERWORD
   */
int lobit(unsigned int num);

/* Return the integer log2 of the argument.  Round down.  Return -1 for 0. */
/* Same as hibit! */
int ilog2(unsigned int num);

/* Return the word-wise xor checksum of n bytes in buf. */
unsigned int cksum(const void *buf, unsigned int n);

/* Return the number of set bits in num.  Is this sometimes called
   "popcount"? */
unsigned int countbits(unsigned int num);

/* Return the 'up' graycode neighbor of proc (out of nproc) */
int Gcup(unsigned int proc, unsigned int nproc);

/* Return the 'down' graycode neighbor of proc (out of nproc) */
int Gcdown(unsigned int proc, unsigned int nproc);

/* This isn't really gray-code related, but where else can it go? */
/* It does the simple-minded "decomposition" of gnobj objects over nproc */
/* processors.  It returns how many to keep and which one to start with. */
void NobjInitial(int gnobj, int nproc, int procnum, int *nobj, int *start);
void NobjInitial64(int64_t gnobj, int nproc, int procnum, int *nobj, int64_t *start);

/* These two came from alt.sources */
/* Return the 'index' of the given gray code (assuming 32-bit longs!) */
unsigned long gray2bin(unsigned long b);

/* Return the graycode  of a given index */
unsigned long bin2gray(unsigned long g);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
