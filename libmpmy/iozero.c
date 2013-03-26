/* More include madness.  This is such good stuff, that I can't resist */
/* just "#include"ing it into all the mpmy_??io.c files...*/

/* Try to use different tags for the different kinds of broadcasts... */
#define BCAST_CLOSE 0x3579
#define BCAST_OPEN 0x3779
#define BCAST_READ1 0x3979
#define BCAST_READ2 0x3b79
#define BCAST_WRITE 0x3d79
#define BCAST_LSEEK 0x3f79
#define BCAST_TELL 0x4179
#define BCAST_FLEN 0x4379
#define BCAST_MKDIR 0x4579

static int
open0(const char *path, int flags, int mode)
{
    int ret;
    
    /* paragon left the const out of the prototype */
    Msgf(("open0(path=%s, flags=%#x, mode=%#x\n", path, flags, mode));
    if (MPMY_Procnum() == 0){
	ret = open((char *)path, flags, mode);
	Msgf(("open returns %d on node 0\n", ret));
    }
    MPMY_BcastTag(&ret, 1, MPMY_INT, 0, BCAST_OPEN);
    Msgf(("open0 returning %d\n", ret));
    return ret;
}

static int
close0(int fd)
{
    int ret;

    if (MPMY_Procnum() == 0) ret = close(fd);
    MPMY_BcastTag(&ret, 1, MPMY_INT, 0, BCAST_CLOSE);
    return ret;
}

static long
read0(int fd, void *buf, unsigned long nbytes)
{
    long ret;

    if (MPMY_Procnum() == 0){
	ret = read(fd, buf, nbytes);
	Msgf(("read0:  read(fd=%d, nbytes=%ld) returns %ld\n", 
	      fd, nbytes, ret)); 
    }
    MPMY_BcastTag(&ret, 1, MPMY_LONG, 0, BCAST_READ1);
    Msgf(("read0:  after Bcast ret = %ld\n", ret));
    if( ret > 0 )
	MPMY_BcastTag(buf, ret, MPMY_CHAR, 0, BCAST_READ2);
    if( Msg_test(__FILE__)){
	int i;
	int sum = 0;
	char *cbuf = buf;
	for(i=0; i<ret; i++){
	    sum ^= cbuf[i];
	}
	Msg_do("iozero: Fread(%ld), got %ld, sum=%d\n", nbytes, ret, sum);
    }

    return ret;
}


static long
write0(int fd, const void *buf, unsigned long nbytes)
{
    long ret;

    /* paragon left the const out of the prototype */
    if (MPMY_Procnum() == 0) ret = write(fd, (void *)buf, nbytes);
    MPMY_BcastTag(&ret, 1, MPMY_INT, 0, BCAST_WRITE);
    return ret;
}

static off_t
lseek0(int fd, off_t offset, int whence)
{
    off_t ret;

    if (MPMY_Procnum() == 0) ret = lseek(fd, offset, whence);
    MPMY_BcastTag(&ret, 1, MPMY_OFFT, 0, BCAST_LSEEK);
    return ret;
}

static off_t
tell0(int fd)
{
    off_t ret;

    if (MPMY_Procnum() == 0) ret = lseek(fd, 0L, SEEK_CUR);
    MPMY_BcastTag(&ret, 1, MPMY_OFFT, 0, BCAST_TELL);
    return ret;
}

static off_t
flen(int fd)
{
    off_t ret;

    ret = lseek(fd, 0, SEEK_END);
    Msg_do("lseek on %d returns %ld, errno is %d\n", fd, ret, errno);

    return  ret;
}

static off_t
flen0(int fd)
{
    off_t ret;

    if (MPMY_Procnum() == 0) {
	ret = flen(fd);
	Msgf(("flen0 of %d returns %ld\n", fd, (long)ret));
    }
    MPMY_BcastTag(&ret, 1, MPMY_OFFT, 0, BCAST_FLEN);
    return ret;
}

static int
mkdir0(const char *path, int mode){
#if defined(__SUNMOS__) || defined(__AP1000__)
    return -1;
#else
    int ret;
    if( MPMY_Procnum() == 0 ){
	ret = mkdir(path, mode);
	if( ret && errno == EEXIST ){
	    /* Let's just pretend we really made it... */
	    ret = 0;
	}
    }
    MPMY_BcastTag(&ret, 1, MPMY_INT, 0, BCAST_MKDIR );
    return ret;
#endif
}

static long
fseekrd0(int fd, off_t offset, int whence, void *buf, int reclen,
	 int nrecs)
{
    int doseek;
    int real_whence;
    void *tmpbuf;
    int i;
    long nread;
    off_t len;
    struct {
	off_t offset;
	long whence;
	long reclen;
	long nrecs;
    } parbuf, *allbuf;

    Msgf(("fseekrd0: (fd=%d, offset=%ld, whence=%d, buf=%p, reclen=%d, nrecs=%d)\n",
	  fd, offset, whence, buf, reclen, nrecs));

    parbuf.offset = offset;
    parbuf.whence = whence;
    parbuf.reclen = reclen;
    parbuf.nrecs = nrecs;

    if (MPMY_Procnum() == 0) {
	allbuf = Malloc(sizeof(parbuf)*MPMY_Nproc());
	MPMY_Gather(&parbuf, sizeof(parbuf), MPMY_CHAR, allbuf,0);
	tmpbuf = Malloc(reclen*nrecs);
	for (i = 0; i < MPMY_Nproc(); i++) {
	    offset = allbuf[i].offset;
	    whence = allbuf[i].whence;
	    reclen = allbuf[i].reclen;
	    nrecs = allbuf[i].nrecs;
	    
	    if( whence == MPMY_SEEK_CUR ){ 
		/* I'm not sure this is a well-defined operation */
		doseek = (offset != 0);
	    }else if( whence == MPMY_SEEK_SET ){
	        off_t cur_off = lseek(fd, 0, SEEK_CUR);
		if (cur_off < 0){
		    static int issued_seek_warning;
		    if( errno == ESPIPE ){
			if( !issued_seek_warning ){
			    SeriousWarning("fseekrd:  Can't seek on a pipe.\nAssuming the seek is a no-op.  Good luck.  You will not be warned again\n");
			    issued_seek_warning = 1;
			}
		    }else{
			Error("fseekrd:  lseek(%d, 0, SEEK_CUR) failed, errno=%d\n",
			      fd, errno);
		    }
		    doseek = 0;
		}else{
		    doseek = (offset != cur_off);
		}
	    }else{
		doseek = 1;
	    }
	    if( doseek ){
		switch(whence){
		case MPMY_SEEK_SET:
		    real_whence = SEEK_SET;
		    break;
		case MPMY_SEEK_CUR:
		    real_whence = SEEK_CUR;
		    break;
		case MPMY_SEEK_END:
		    real_whence = SEEK_END;
		    break;
		default:
		    Error("Illegal value of whence (%d) in fseekrd\n", whence);
		}
		if (lseek(fd, offset, real_whence) == -1L) {
		    Error("fseekrd: cycle %d lseek(%d, %ld, %d) failed, errno=%d\n",
			  i, fd, offset, whence, errno);
		}
	    }
	    len = 0;
	    while (len < reclen*nrecs) {
		nread = read(fd, (char *)tmpbuf+len, reclen*nrecs-len);
		if (nread == -1) {
		    Error("fseekrd: read(%d, %ld) failed, errno=%d\n", 
			  fd, reclen*nrecs-len, errno);
		} else if (nread == 0) {
		    Error("fseekrd: read(%d, %ld) got EOF\n", 
			  fd, reclen*nrecs-len);
		} else {
		    Msgf(("fseekrd: got %ld\n", nread));
		    len += nread;
		}
	    }
	    if (len != reclen*nrecs) {
	        Error("fseekrd: Wrong amount of data\n");
	    }
	    if (i) {
		MPMY_send(tmpbuf, reclen*nrecs, i, MPMY_IOTAG);
	    } else {
		memcpy(buf, tmpbuf, reclen*nrecs);
	    }
	}
	Free(tmpbuf);
	Free(allbuf);
    } else {
	MPMY_Gather(&parbuf, sizeof(parbuf)/sizeof(long), MPMY_LONG, NULL, 0);
	MPMY_recvn(buf, reclen*nrecs, 0, MPMY_IOTAG);
    }
    return parbuf.nrecs;
}

#define MAX_IOBUF (64*1024*1024)

static long
write0_multi(int fd, const void *buf, off_t nbytes)
{
    long ret;
    off_t *sizes;
    int i, n, sync;
    char *tmpbuf = 0;
    off_t total_bytes;
    int nproc = MPMY_Nproc();
    int procnum = MPMY_Procnum();

    MPMY_Combine(&nbytes, &total_bytes, 1, MPMY_OFFT, MPMY_SUM);
    Msgf(("write0_multi, nbytes is %ld, total_bytes is %ld\n", nbytes, total_bytes));
    /* If the total is small enough, we might as well concat it on proc 0 */
    if (total_bytes <= MAX_IOBUF) {
	MPMY_NGather(buf, nbytes, MPMY_CHAR, (void **)&tmpbuf, 0);
	if (procnum == 0) {
	    ret = write(fd, tmpbuf, total_bytes);
	    if (ret != total_bytes) Error("write failed, errno=%d\n", errno);
	    Msgf(("write0 %ld (one block)\n", ret));
	    Free(tmpbuf);
	}
	return nbytes;
    } else if (procnum == 0) {
	sizes = Malloc(sizeof(off_t)*nproc);
	MPMY_Gather(&nbytes, 1, MPMY_OFFT, sizes, 0);
	ret = 0;
	while (nbytes > 0) {
	    off_t nwrite = (nbytes > MAX_IOBUF) ? MAX_IOBUF : nbytes;
	    off_t wrote;
	    wrote = write(fd, buf+ret, nwrite);
	    if (wrote != nwrite) Shout("write failed, errno=%d\n", errno);
	    ret += wrote;
	    nbytes -= wrote;
	}
	Msgf(("write0 %ld\n", ret));
	if (nproc > 1) {
	    tmpbuf = Malloc(nbytes);
	    for (i = procnum+1; i < nproc; i++) {
		/* This could be double-buffered */
		tmpbuf = Realloc(tmpbuf, sizes[i]);
		/* Avoid deluge of messages by sync */
		MPMY_send(&sync, sizeof(int), i, MPMY_IOTAG);
		MPMY_recvn(tmpbuf, sizes[i], i, MPMY_IOTAG);
		n = write(fd, tmpbuf, sizes[i]);
		if (n != sizes[i]) Shout("write failed, errno=%d\n", errno);
		Msgf(("write%d %d\n", i, n));
		/* should we send errno as well? */
		MPMY_send(&n, sizeof(int), i, MPMY_IOTAG);
	    }
	    Free(tmpbuf);
	}
	Free(sizes);
    } else {
	MPMY_Gather(&nbytes, 1, MPMY_OFFT, NULL, 0);
	MPMY_recvn(&sync, sizeof(int), 0, MPMY_IOTAG);
	Msgf(("sending %ld bytes\n", nbytes));
	MPMY_send(buf, nbytes, 0, MPMY_IOTAG);
	MPMY_recvn(&ret, sizeof(int), 0, MPMY_IOTAG);
    }
    return ret;
}

static long
read0_multi(int fd, void *buf, off_t nbytes)
{
    int ret;
    off_t *sizes;
    int i, n;
    char *tmpbuf;
    int nproc = MPMY_Nproc();
    int procnum = MPMY_Procnum();

    if (sizeof(off_t) != sizeof(long long)) {
      Error("Type problem in write0_multi\n");
    }
    if (procnum == 0) {
	sizes = Malloc(sizeof(off_t)*nproc);
	MPMY_Gather(&nbytes, 1, MPMY_OFFT, sizes, 0);
	ret = read(fd, buf, nbytes);
	if (ret != nbytes) Shout("read failed, errno=%d\n", errno);
	Msgf(("read0 %d\n", ret));
	if (nproc > 1) {
	    tmpbuf = Malloc(nbytes);
	    for (i = procnum+1; i < nproc; i++) {
		/* This could be double-buffered */
		tmpbuf = Realloc(tmpbuf, sizes[i]);
		n = read(fd, tmpbuf, sizes[i]);
		MPMY_send(tmpbuf, n, i, MPMY_IOTAG);
		if (n != sizes[i]) Shout("read failed, errno=%d\n", errno);
		Msgf(("read%d %d\n", i, n));
	    }
	    Free(tmpbuf);
	}
	Free(sizes);
    } else {
	MPMY_Status stat;
	MPMY_Comm_request req;

	MPMY_Gather(&nbytes, 1, MPMY_OFFT, NULL, 0);
	MPMY_Irecv(buf, nbytes, 0, MPMY_IOTAG, &req);
	MPMY_Wait(req, &stat);
	ret = MPMY_Count(&stat);
    }
    return ret;
}


