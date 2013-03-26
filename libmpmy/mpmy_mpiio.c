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
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#ifndef OPEN_MPI
#include <mpio.h>
#endif
#include "protos.h"
#include "mpmy.h"
#include "mpmy_io.h"
#include "Msgs.h"

#ifndef EINVAL
/* just in case... */
#define EINVAL 0
#endif

#define NFILES 4096

/* MPI only allows 2GB buffer sizes, and MPI_Get_Count uses an int */
#define MAXIOSIZE (256*1024*1024)

static struct _File{
    MPI_File fd;
    int iomode;
} _files[NFILES];

static int files;		/* need dynamic storage */

static int do_nfileio;

MPMYFile *
MPMY_Fopen(const char *path, int flags)
{
    MPI_File fd;
    MPI_Info info;
    int iomode = MPMY_SINGL;	/* default */
    int real_flags = MPI_MODE_RDONLY; /* if no flags specified */
    int ret;

    Msgf(("Fopen %s, flags = 0x%x\n", path, flags));
    if (flags & MPMY_RDONLY) real_flags = MPI_MODE_RDONLY;
    if (flags & MPMY_WRONLY) real_flags = MPI_MODE_WRONLY;
    if (flags & MPMY_RDWR)   real_flags = MPI_MODE_RDWR;
    if (flags & MPMY_APPEND) real_flags |= MPI_MODE_APPEND;
    if (flags & MPMY_TRUNC && ((flags & MPMY_WRONLY) || (flags & MPMY_RDWR))) {
	int fd;
	if (MPMY_Procnum() == 0) {
	    fd = open(path, O_RDWR|O_TRUNC, 0644);
	    if (fd < 0) Msgf(("Fopen fails, errno=%d\n", errno));
	    else close(fd);
	    MPMY_Sync();
	} else {
	    MPMY_Sync();
	}
    }
    if (flags & MPMY_CREAT)  real_flags |= MPI_MODE_CREATE;

    /* Panasas optimizations */
    if (!(flags & MPMY_RDONLY))
	real_flags |= MPI_MODE_UNIQUE_OPEN;	/* dangerous? */
    MPI_Info_create(&info);
    MPI_Info_set(info, "panfs_concurrent_write", "1");

    /* Should we make sure that only one of them is on?? */
    if (flags & MPMY_MULTI) iomode = MPMY_MULTI;
    if (flags & MPMY_SINGL) iomode = MPMY_SINGL;

    if (flags & MPMY_NFILE) Error("MPMY_NFILE not supported\n");
    if (flags & MPMY_IOZERO) Error("MPMY_IOZERO not supported\n");
    if (flags & MPMY_INDEPENDENT) Error("MPMY_INDEPENDENT not supported\n");

    if (flags & MPMY_SINGL) {
	Msgf(("Fopen %s in SINGL mode\n", path));
    } else {
	Msgf(("Fopen %s in MULTI mode\n", path));
    }
    Msgf(("MPI_File_open %s with flags = 0x%x\n", path, real_flags));
    ret = MPI_File_open(MPI_COMM_WORLD, (char *)path, real_flags, info, &fd);

    if (files >= NFILES) Error("files too large\n");
    if (ret == 0) {
	_files[files].iomode = iomode;
	_files[files].fd = fd;
	Msgf(("Fopen returns fd %p, iomode=%d, flags=0x%x\n", 
	      fd, iomode, flags));
	return &(_files[files++]);
    } else {
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
    Msgf(("Fclose %p\n", fp->fd));
    ret = MPI_File_close(&fp->fd);
    return ret;
}

int 
MPMY_Mkdir(const char *path, int mode)
{
    int ret;

    if( MPMY_Procnum() == 0 ){
	ret = mkdir(path, mode);
	if( ret && errno == EEXIST ){
	    /* Let's just pretend we really made it... */
	    ret = 0;
	}
    }
    MPMY_BcastTag(&ret, 1, MPMY_INT, 0, 0x4579 );
    return ret;
}

size_t
MPMY_Fread(void *ptr, size_t size, size_t nitems, MPMYFile *Fp)
{
    struct _File *fp = (struct _File *)Fp;
    MPI_Status status;
    int cnt;
    size_t nread = size*nitems;
    const char *p = ptr;

    Msgf(("MPMY_Fread %ld %ld\n", size, nitems));
    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
    if (fp->iomode == MPMY_SINGL) {
	if (nread >= (1 << 31)) Error("MPMY_SINGL does not yet support large reads\n");
	MPI_File_read_all(fp->fd, (void *)p, nread, MPI_CHAR, &status);
	MPI_Get_count(&status, MPI_CHAR, &cnt);
    } else {
	MPI_Offset mpi_offset;
	size_t left, *sizes;
	int i;
	assert(sizeof(size_t) == MPMY_Datasize[MPMY_LONG]);
	sizes = Malloc(sizeof(size_t)*MPMY_Nproc());
	/* Use scan instead? */
	Native_MPMY_Allgather(&nread, 1, MPMY_LONG, sizes);
	MPI_File_get_position(fp->fd, &mpi_offset);
	Msgf(("Starting offset in MPMY_Fread is %lld\n", mpi_offset));
	for (i = 0; i < MPMY_Procnum(); i++) {
	    mpi_offset += sizes[i];
	}
	Msgf(("My offset in MPMY_Fread is %lld\n", mpi_offset));
	Free(sizes);
	left = nread;
	while (left > 0) {
	    nread = (left < MAXIOSIZE) ? left : MAXIOSIZE;
	    Msgf(("read %ld at %lld\n", nread, mpi_offset));
	    Msg_flush();
	    MPI_File_read_at(fp->fd, mpi_offset, (void *)p, nread, MPI_CHAR, &status);
	    left -= nread;
	    p += nread;
	    mpi_offset += nread;
	    MPI_Get_count(&status, MPI_CHAR, &cnt);
	}
    }
    return cnt/size;
}

size_t
MPMY_Fwrite(const void *ptr, size_t size, size_t nitems, MPMYFile *Fp)
{
    struct _File *fp = (struct _File *)Fp;
    MPI_Status status;
    int cnt;
    size_t nwrite = size*nitems;
    const char *p = ptr;

    Msgf(("MPMY_Fwrite %ld %ld\n", size, nitems));
    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
    if (fp->iomode == MPMY_SINGL) {
	if (nwrite >= (1 << 31)) Error("MPMY_SINGL does not yet support large writes\n");
	MPI_File_write_all(fp->fd, (void *)p, nwrite, MPI_CHAR, &status);
	MPI_Get_count(&status, MPI_CHAR, &cnt);
	if (cnt != nwrite) Error("MPMY_Fwrite has a problem, wrote %d of %ld\n",
				 cnt, nwrite);
    } else {
	MPI_Offset mpi_offset;
	size_t left, *sizes;
	int i;
	assert(sizeof(size_t) == MPMY_Datasize[MPMY_LONG]);
	sizes = Malloc(sizeof(size_t)*MPMY_Nproc());
	Native_MPMY_Allgather(&nwrite, 1, MPMY_LONG, sizes);
	MPI_File_get_position(fp->fd, &mpi_offset);
	Msgf(("Starting offset in MPMY_Fwrite is %lld\n", mpi_offset));
	for (i = 0; i < MPMY_Procnum(); i++) {
	    mpi_offset += sizes[i];
	}
	Msgf(("My offset in MPMY_Fwrite is %lld\n", mpi_offset));
	Free(sizes);
	left = nwrite;
	while (left > 0) {
	    nwrite = (left < MAXIOSIZE) ? left : MAXIOSIZE;
	    Msgf(("write %ld at %lld\n", nwrite, mpi_offset));
	    Msg_flush();
	    MPI_File_write_at(fp->fd, mpi_offset, (void *)p, nwrite, MPI_CHAR, &status);
	    left -= nwrite;
	    p += nwrite;
	    mpi_offset += nwrite;
	    MPI_Get_count(&status, MPI_CHAR, &cnt);
	    if (cnt != nwrite) Error("MPMY_Fwrite has a problem, wrote %d of %ld\n",
				     cnt, nwrite);
	}
    }
    return nitems;
}

off_t
MPMY_Fseek(MPMYFile *Fp, off_t offset, int whence)
{
    struct _File *fp = (struct _File *)Fp;
    int ret;
    MPI_Offset mpi_offset;
    int real_whence = 0;

    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }

    if (whence == MPMY_SEEK_SET) real_whence = MPI_SEEK_SET;
    if (whence == MPMY_SEEK_CUR) real_whence = MPI_SEEK_CUR;
    if (whence == MPMY_SEEK_END) real_whence = MPI_SEEK_END;

    mpi_offset = offset;	/* potential conversion problem */
    if (fp->iomode == MPMY_SINGL) {
	ret = MPI_File_seek_shared(fp->fd, mpi_offset, real_whence);
    } else {
	ret = MPI_File_seek(fp->fd, mpi_offset, real_whence);
    }
    if (ret != -1) ret = 0;
    Msgf(("Fseek to %ld returns %d\n", (long)mpi_offset, ret));
    return ret;
}

off_t
MPMY_Ftell(MPMYFile *Fp)
{
    struct _File *fp = (struct _File *)Fp;
    MPI_Offset mpi_offset;

    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
    if (fp->iomode == MPMY_SINGL) {
	MPI_File_get_position_shared(fp->fd, &mpi_offset);
    } else {
	MPI_File_get_position(fp->fd, &mpi_offset);
    }
    Msgf(("Ftell returns %ld\n", (long)mpi_offset));
    return mpi_offset;
}

off_t
MPMY_Flen(MPMYFile *Fp)
{
    struct _File *fp = (struct _File *)Fp;
    MPI_Offset mpi_offset_current, mpi_offset_end;

    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
    if (fp->iomode == MPMY_SINGL) {
	MPI_File_get_position_shared(fp->fd, &mpi_offset_current);
	MPI_File_seek_shared(fp->fd, mpi_offset_current, MPI_SEEK_END);
	MPI_File_get_position_shared(fp->fd, &mpi_offset_end);
	MPI_File_seek_shared(fp->fd, mpi_offset_current, MPI_SEEK_SET);
    } else {
	MPI_File_get_position(fp->fd, &mpi_offset_current);
	MPI_File_seek(fp->fd, mpi_offset_current, MPI_SEEK_END);
	MPI_File_get_position(fp->fd, &mpi_offset_end);
	MPI_File_seek(fp->fd, mpi_offset_current, MPI_SEEK_SET);
    }
    Msgf(("Flen returns %ld\n", (long)mpi_offset_end));
    return mpi_offset_end;
}

size_t
MPMY_Fseekrd(MPMYFile *Fp, off_t offset, int whence, void *buf, size_t reclen,
	     size_t nrecs)
{
    struct _File *fp = (struct _File *)Fp;
    MPI_Offset mpi_offset;
    MPI_Status status;
    int cnt;
    size_t nread = reclen*nrecs;
    const char *p = buf;

    Msgf(("Fseekrd %ld at %ld\n", (size_t)reclen*nrecs, offset));
    mpi_offset = offset;
    if( fp == NULL ){
	errno = EINVAL;
	return -1;
    }
	if (fp->iomode == MPMY_SINGL) {
	    MPI_File_read_at_all(fp->fd, mpi_offset, (void *)p, nread, MPI_BYTE, &status);
	} else {
	    MPI_File_read_at(fp->fd, mpi_offset, (void *)p, nread, MPI_BYTE, &status);
	}
	MPI_Get_count(&status, MPI_BYTE, &cnt);
	if (cnt != nread) Error("MPMY_Fseekrd has a problem, got %d expected %ld\n",
				cnt, nread);
    return nrecs;
}

#include "io_generic.c"
