#ifndef _LsvDOTh
#define _LsvDOTh

#define LSV_ANY (-2)

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
extern int LSV_procnum;
extern int LSV_nproc;

extern char Smy_name[];		/* my hostname or inet address */

void Ssend(const void *outb, int outcnt, int dest, int type);
int Srecv_block(void *inb, int size, int type, int *from);
int Srecv(void *inb, int size, int type, int *from);
void Sclose(void);
void Sdiag(int (*)(const char *, ...));

void Sinit_host1(int *portp, char **namep);
void Sinit_host(int nproc);
void Sinit_elt(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _LsvDOTh */
