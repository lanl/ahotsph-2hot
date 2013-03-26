#ifndef _SinglIODOTh
#define _SinglIODOTh

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
int singlPrintf(const char *, ...);
void singlFflush(void);
int singlAutoflush(int);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
