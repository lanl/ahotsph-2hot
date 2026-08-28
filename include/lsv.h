/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef _LsvDOTh
#define _LsvDOTh

#define LSV_ANY (-2)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
extern int LSV_procnum;
extern int LSV_nproc;

extern char Smy_name[]; /* my hostname or inet address */

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
