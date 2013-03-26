/* This file contains the parallel I/O suitable for CMMD and NX.
   It wouldn't be hard to add cubix-syntax as well.  Are there any
   other options?  Would they fit in this structure?   The only
   difference between CMMD and NX is in Fopen, where one
   system calls gopen() and the other calls CMMD_set_io_mode().
   We use a pre-processor symbol (__INTEL_SSD__) or (__CM5__) to decide
   which one to use.  __INTEL_SSD__ is set by the ARCH-specific Makefiles,
   while __CM5__ is set by mpmy_cm5.c, which #includes this file.
*/
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "protos.h"
#include "mpmy.h"
#include "mpmy_io.h"
#include "Msgs.h"
#include "iozero.h"

#ifndef EINVAL
/* just in case... */
#define EINVAL 0
#endif

#define NFILES 4096
static struct _File{
    int fd;
    int iomode;
    int iotype;
    int flags;
} _files[NFILES];

static int do_nfileio;

MPMYFile *
MPMY_Fopen(const char *path, int flags)
{
    int fd;
    int mode = 0644;
    int iomode = MPMY_SINGL;	/* default */
    int iotype = 0;
    int real_flags = 0;

    Msgf(("Fopen %s\n", path));
    if (flags & MPMY_RDONLY) real_flags |= O_RDONLY;
    if (flags & MPMY_WRONLY) real_flags |= O_WRONLY;
    if (flags & MPMY_RDWR)   real_flags |= O_RDWR;
    if (flags & MPMY_APPEND) real_flags |= O_APPEND;
    if (flags & MPMY_TRUNC)  real_flags |= O_TRUNC;
    if (flags & MPMY_CREAT)  real_flags |= O_CREAT;

    /* Should we make sure that only one of them is on?? */
    if (flags & MPMY_MULTI) iomode = MPMY_MULTI;
    if (flags & MPMY_SINGL) iomode = MPMY_SINGL;

    if (flags & MPMY_NFILE) iotype = MPMY_NFILE;
    if (flags & MPMY_IOZERO) Error("MPMY_IOZERO not supported\n");
    if (flags & MPMY_INDEPENDENT) Error("MPMY_INDEPENDENT not supported\n");

    if (iotype == MPMY_NFILE) {
	char real_path[256];
	sprintf(real_path, "%s.p%03d", path, MPMY_Procnum());
	Msgf(("Fopen %s in NFILE mode\n", path));
	fd = open(real_path, real_flags, mode);
    } else if (flags & MPMY_SINGL) {
	Msgf(("Fopen %s in SINGL mode\n", path));
	fd = open(path, real_flags, mode);
    } else {
	Msgf(("Fopen %s in MULTI mode\n", path));
	fd = open(path, real_flags, mode);
    }

    if (fd >= NFILES) Error("fd too large (%d)\n", fd);
    if( fd >= 0 ){
	_files[fd].iomode = iomode;
	_files[fd].iotype = iotype;
	Msgf(("Fopen returns %d, iomode=%d, flags=0x%x\n", fd, iomode, flags));
	return &(_files[fd]);
    }else{
	Msgf(("Fopen fails, errno=%d\n", errno));
	return NULL;
    }
}

int 
MPMY_Nfileio(int val){
    int oldval = do_nfileio;
    do_nfileio = val;
    return oldval;
}

int
MPMY_Fclose(MPMYFile *Fp)
{
    struct _File *fp = (struct _File *)Fp;
    int ret;

    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
    if (fp->iomode == MPMY_SINGL) ret = close0(fp->fd);
    else ret = close(fp->fd);
    Msgf(("Fclose of %d returns %d\n", fp->fd, ret));
    return ret;
}

int 
MPMY_Mkdir(const char *path, int mode){
    return mkdir0(path, mode);
}

int
MPMY_Fread(void *ptr, int size, int nitems, MPMYFile *Fp)
{
    struct _File *fp = (struct _File *)Fp;
    int ret;

    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
    if (fp->iomode == MPMY_SINGL) ret = read0(fp->fd, ptr, size*nitems);
    else ret = read(fp->fd, ptr, size*nitems);

    Msgf(("Fread from %d returns %d\n", fp->fd, ret));
    if (ret % size) Error("MPMY_Fread has a problem\n");
    return ret/size;
}

int
MPMY_Fwrite(const void *ptr, int size, int nitems, MPMYFile *Fp)
{
    struct _File *fp = (struct _File *)Fp;
    int ret;

    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
    if (fp->iomode == MPMY_SINGL) ret= write0(fp->fd, ptr, size*nitems);
    else ret = write(fp->fd, ptr, size*nitems);
    /* Msgf(("Fwrite to %d returns %d.\n", fp->fd, ret)); */
    if (ret % size) Error("MPMY_Fwrite has a problem\n");
    return ret/size;
}

int
MPMY_Fseek(MPMYFile *Fp, off_t offset, int whence)
{
    struct _File *fp = (struct _File *)Fp;
    off_t ret;
    int real_whence = 0;

    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }

    if (whence == MPMY_SEEK_SET) real_whence = SEEK_SET;
    if (whence == MPMY_SEEK_CUR) real_whence = SEEK_CUR;
    if (whence == MPMY_SEEK_END) real_whence = SEEK_END;

    if (fp->iotype == MPMY_SINGL) {
	ret = lseek0(fp->fd, offset, real_whence);
    } else {
	ret = lseek(fp->fd, offset, real_whence);
    }
    if (ret != -1) ret = 0;
    Msgf(("Fseek returns %ld\n", (long)ret));
    return ret;
}

int
MPMY_Ftell(MPMYFile *Fp)
{
    struct _File *fp = (struct _File *)Fp;
    off_t ret;

    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
    if (fp->iotype == MPMY_SINGL) {
	ret = tell0(fp->fd);
    } else {
	ret = lseek(fp->fd, 0L, SEEK_CUR);
    }
    Msgf(("Ftell returns %ld\n", (long)ret));
    return ret;
}

off_t
MPMY_Flen(MPMYFile *Fp)
{
    struct _File *fp = (struct _File *)Fp;
    off_t ret;

    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
    if (fp->iotype & MPMY_NFILE) {
	ret = flen(fp->fd);
	Msgf(("flen segment %ld\n", (long)ret));
	if (sizeof(ret) != sizeof(long)) {
	  Error("Bad types in MPMY_Flen\n");
	}
	MPMY_Combine(&ret, &ret, 1, MPMY_LONG, MPMY_SUM);
    } else if (fp->iotype == MPMY_SINGL) {
	ret = flen0(fp->fd);
    } else {
	ret = flen(fp->fd);
    }
    Msgf(("Flen returns %ld\n", (long)ret));
    return ret;
}

int 
MPMY_Fseekrd(MPMYFile *Fp, off_t offset, int whence, void *buf, int reclen,
	     int nrecs)
{
    struct _File *fp = (struct _File *)Fp;
    int doseek;

    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
    if (fp->iomode == MPMY_SINGL) {
	nrecs = fseekrd0(fp->fd, offset, whence, buf, reclen, nrecs);
	return nrecs;
    }

    if( whence == MPMY_SEEK_CUR ){
	doseek = (offset != 0);
    }else if( whence == MPMY_SEEK_SET ){
	/* don't worry about errors.  If ftell returns -1, */
	/* doseek will be turned on, and the fseek below will */
	/* (probably) fail */
	doseek = (MPMY_Ftell(Fp) != offset);
    }else{
	doseek = 1;
    }

    if( doseek ){
	if( MPMY_Fseek(Fp, offset, whence) ){
	    if( whence == MPMY_SEEK_CUR && offset > 0 ){
		/* Make a final heroic effort to seek by reading forward! */
		char junk[BUFSIZ];
		int nleft = offset;
		while( nleft ){
		    int ntry = ( nleft > sizeof(junk) ) ? sizeof(junk) : nleft;
		    if( MPMY_Fread(junk, ntry, 1, Fp) != 1 ){
			Error("fseekrd: incremental fread(%#lx, %d, 1, %#lx) failed, errno=%d\n",
			      (unsigned long)junk, ntry, 
			      (unsigned long)fp, errno);
			return -1;
		    }
		    nleft -= ntry;
		}
	    }else{
		Error("fseekrd: fseek(%#lx, %lld, %d) failed, errno=%d\n",
		      (unsigned long)fp, offset, whence, errno);
		return -1;
	    }
	}
    }
    if( MPMY_Fread(buf, reclen, nrecs, Fp) != nrecs ){
	Error("fseekrd: fread(%#lx, %d, %d, %#lx) failed, errno=%d\n", 
	      (unsigned long)buf, reclen, nrecs, (unsigned long)fp, errno);
	return -1;
    }
    return nrecs;
}

#include "iozero.c"
#include "io_generic.c"
