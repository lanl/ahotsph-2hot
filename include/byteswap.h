#ifndef _ByteSwapDOTh
#define _ByteSwapDOTh

/* A general, in-place, byte-swapper.  */
/* It swaps a total of unit_len*n_units bytes, unit_len bytes at a time. */
/* Thus, you can use it for arrays of doubles with unit_len=8, or */
/* arrays of chars with unit_len=1 (which is equivalent to memcpy). */
/* It checks for stupid arguments.  It works fine when */
/* from and to are identical.  It breaks if from and to are */
/* almost the same. */
#ifdef __cplusplus
extern "C" {
#endif
int Byteswap(int unit_len, int n_units, void *from, void *to);
#ifdef __cplusplus
}
#endif

/* It would probalby be worthwhile to inline these! */

#endif
