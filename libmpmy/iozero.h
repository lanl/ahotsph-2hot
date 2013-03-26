#ifndef IOzeroDOTh
#define IOzeroDOTh

#define MPMY_IOTAG (0x183)
#define IoZero(fp) (fp->flags & MPMY_IOZERO)

static int open0(const char *path, int flags, int mode);
static int close0(int fd);
static int mkdir0(const char *path, int mode);
static long read0(int fd, void *buf, unsigned long nbytes);
static long write0(int fd, const void *buf, unsigned long nbytes);
static off_t lseek0(int fd, off_t offset, int whence);
static off_t tell0(int fd);
static off_t flen(int fd);
static off_t flen0(int fd);
static long fseekrd0(int fd, off_t offset, int whence, void *buf, 
		     int reclen, int nrecs);
static long write0_multi(int fd, const void *buf, off_t nbytes);
static long read0_multi(int fd, void *buf, off_t nbytes);
#endif
