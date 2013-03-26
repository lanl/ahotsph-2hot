#ifndef _RingDOTh
#define _RingDOTh

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
void Ring(void *bptr, int bsize, int bnobj,
     void *optr, int osize, int onobj, int oused, 
     void initf(void *, void *), void interactf(void *, void *, int, int));

void Ring2(void *bptr, int bsize, int bnobj,
	   void *optr, int osize, int onobj, int tsize, 
	   void initf(void *, void *), void interactf(void *, void *, int, int), void finishf(void *, void *));

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RingDOTh */
