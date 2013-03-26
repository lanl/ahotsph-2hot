#ifndef MPMY_IOdotH
#define MPMY_IOdotH

#include <stdarg.h>
#include <sys/types.h>

typedef void MPMYFile;

/* mode flags for open */
/* The first three correspond to the analogous modes O_???  that seem
   to be fairly common on different unices (linux, sunos, solaris).  I
   guess that's a good thing.  But who decided that OR'ing zero
   (O_RDONLY) with other values should be used as a flag????  The latter
   ones show no commonality between other flavors of unix anyway, so
   there's nothing to remain analogous to... */
#define	MPMY_RDONLY 00000000
#define	MPMY_WRONLY 00000001
#define	MPMY_RDWR   00000002
#define	MPMY_APPEND 00000004
#define	MPMY_CREAT  00000010
#define	MPMY_TRUNC  00000020
/* io modes */
#define MPMY_SINGL  00010000	/* like cubix single mode */
#define MPMY_MULTI  00020000	/* like cubix multi mode  */
/* These four are used by the 'mpmy_pario' implementation, but that is
   no longer linked with any of our default systems... */
#define MPMY_UNIX   00040000	/* one file, UNIX multi-process semantics */
#define MPMY_IOZERO 00100000	/* if(procnum==0){...} */
#define MPMY_INDEPENDENT 00200000 /* many files.  Complete independence */
#define MPMY_NFILE 00400000	/* many files. Really. */

/* modes for seek */
#define MPMY_SEEK_SET 0
#define MPMY_SEEK_CUR 1
#define MPMY_SEEK_END 2


#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
MPMYFile *MPMY_Fopen(const char *path, int flags);
int MPMY_SetIOMode(MPMYFile *fp, int iomode);
int MPMY_Fclose(MPMYFile *fp);
int MPMY_Mkdir(const char *path, int mode);
size_t MPMY_Fread(void *ptr, size_t size, size_t nitems, MPMYFile *fp);
size_t MPMY_Fwrite(const void *ptr, size_t size, size_t nitems, MPMYFile *fp);
off_t MPMY_Fseek(MPMYFile *fp, off_t offset, int whence);
off_t MPMY_Ftell(MPMYFile *fp);
off_t MPMY_Flen(MPMYFile *fp);
int MPMY_Getc(MPMYFile *fp);
int MPMY_Ungetc(char c, MPMYFile *fp);
size_t MPMY_Fseekrd(MPMYFile *fp, off_t offset, int whence, void *buf, size_t reclen,
		    size_t nrecs);
int MPMY_Fprintf(MPMYFile *fp, const char *fmt, ...);
int MPMY_Vfprintf(MPMYFile *fp, const char *fmt, va_list args);
int MPMY_Fflush(MPMYFile *fp);
int MPMY_Nfileio(int val);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MPMY_IOdotH */
