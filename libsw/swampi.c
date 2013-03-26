/*
 * Copyright 1997 Michael Warren & John Salmon.	 All Rights Reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/ioctl.h>
#if defined(__SUN4__) || defined(__SUN5__) || defined(linux)
/* SunOS hides TCP_NODELAY and TCP_MAXSEG in netinet/tcp.h */
#include <netinet/tcp.h>
#endif
#ifdef __alpha
#include <xti_inet.h>
#endif
#include "protos.h"
#include "swampi.h"
#include "mpmy.h"
#include "mpmy_io.h"
#include "mpmy_abnormal.h"
#include "error.h"
#include "Msgs.h"
#include "dll.h"
#include "hwclock.h"

#ifndef INADDR_NONE
/* e.g., on SunOS */
#define INADDR_NONE (-1)
#endif

#ifndef MAX
#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a < b) ? a : b)
#endif

#define H_MAGIC (0x9f07)	/* Magic number for headers */

#define HOST_NUM (-1)		/* can't redefine this without mem adjust. */

/* message header */
typedef struct {
    int src : 20;
    unsigned int comm : 6;
    int len;
    int tag;
    int magic;
} msghdr_t;

typedef struct {
    int proc : 20;
    unsigned int comm : 6;
    unsigned int pending : 1;
    unsigned int outgoing : 1;
    unsigned int hdr_flag : 1;
    unsigned int wild_src : 1;
    unsigned int wild_tag : 1;
    unsigned int buffered : 1;
    int len;
    int left;
    int tag;
    char *buf;			/* This is where the message belongs */
    char *ptr;			/* This might be a malloced temp buffer */
} req_t;

/* Must match MPI_Datatype enum */
static unsigned int MPI_Datasize[] = 
{ sizeof(float), sizeof(double), sizeof(long double), 
  sizeof(char), sizeof(char), sizeof(short), sizeof(int), 
  sizeof(long), sizeof(long long),
  sizeof(unsigned), sizeof(unsigned int), sizeof(unsigned char), 
  sizeof(unsigned short), sizeof(unsigned long), sizeof(unsigned long long),
  sizeof(MPI_float_int), sizeof(MPI_double_int), sizeof(MPI_long_int),
  sizeof(MPI_2int), sizeof(MPI_short_int), sizeof(MPI_long_double_int),
  2*sizeof(float), 2*sizeof(double),
  1/*user_data*/
};

char *mpi_datatype_name[_MPI_NUMDATATYPES] = {
    "float", "double", "long double", 
    "byte", "char", "short", "int", 
    "long", "long long",
    "unsigned", "unsigned int", "unsigned char", 
    "unsigned short", "unsigned long", "unsigned long long",
    "float int", "double int", "long int", 
    "2int", "short int", "long double int", 
    "complex", "double complex",
    "user data"
};

/* Must match MPI_Op enum */
char *mpi_op_name[_MPI_NUMOPS] = {
    "sum", "prod", "max", "min", "band", "bor",
    "bxor", "land", "lor", "lxor", "maxloc", "minloc"
};

typedef enum {
    ExecTm, WaitTm, TestTm, SelectTm, SendTm, RecvTm, SendBytes, RecvBytes,
    NSends, NRecvs,
    _MPI_NUMSTATS
} MPI_Statistics;

char *mpi_stats_name[_MPI_NUMSTATS] = {
    "Exec Time", "Wait Time", "Test Time", "Select Time", "Send Time", "Recv Time", 
    "Send MBytes", "Recv MBytes", "Num Sends", "Num Recvs"
};

static double mpi_statistics[_MPI_NUMSTATS];

unsigned int *_MPI_Datasize = MPI_Datasize;
int _MPI_Procnum, _MPI_Nproc;

static int MPI_Procnum, MPI_Nproc;
static int Max_fd;		/* for select */

/* these limits are arbitrary, and mostly to catch programming errors */
#define MAX_NPOST 1000		/* This many req_t ptrs for each proc */
#define MAX_NPOSTANY 1000	/* This many req ptrs for MPI_ANY_SOURCE */

#define CheckTypeOK(type) \
if (type < 0 || type >= _MPI_NUMDATATYPES) Error("Invalid type (%d)\n", type)

/* req_chn stores all send and recv request. It has a separate dll of send */
/* and receive entries for each socket, and one wildcard receive entry */
static Chn req_chn;
static Dll *recv_list, *send_list; /* we malloc Nproc of these */
static Dll wild_dll, *wild_list; /* only need one of these */

/* For messages that arrive without the corresponding Irecv posted */
static Dll bufr_dll, *bufr_list; /* only need one of these */

/* we malloc Nproc of these */
static req_t **read_active, **write_active;

static int my_pid;		/* my process id */
static struct sockaddr_in host_addr;
static struct sockaddr_in my_addr;
static int sock;		/* file descriptor for listen socket */
static int *s;			/* array of file descriptor for all channels */
static struct sockaddr_in *addr; /* sockaddrs for all listen sockets */
static int sock_bind(struct sockaddr_in *acc);
static int sock_connect(struct sockaddr_in *acc);
static void sock_setopt(int fd);
static void req_done(Dll_elmt *req, MPI_Status *stat);
static int writemsg(int fd, const void *ptr, int nbytes);
static int readmsg(int fd, void *ptr, int nbytes);
static void writemsg_block(int fd, const void *ptr, int nbytes);
static void readmsg_block(int fd, void *ptr, int nbytes);
req_t *parse_hdr(msghdr_t *hdr, req_t *req, int proc);
static req_t *io_pending(Dll *d);
static void spin_io(Dll_elmt *req);
static int do_io(req_t *req, int proc);
static void do_io_local(req_t *ireq, int proc);
static void init_elt(void);
static void sock_getopt(int fd);
static void mpi_diagnostics(void);

/* This controls the maximum amount read/written per system call */
#define PKTSIZE (20*1460)
static void
init_elt(void)
{
    int suspend_proc;
    int i, j;
    int size = sizeof(struct sockaddr_in);
    int hostport;
    char *hostip;
    unsigned long inaddr;
    int addr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in tmp_addr;
    int noblock = 1;
    char msgfile[256];
    extern int singlAutoflush(int);
    extern int _MPMY_procnum_, _MPMY_nproc_, _MPMY_initialized_;

    sock = sock_bind(&my_addr); /* establish port to listen */
    my_pid = getpid();

    if (!(getenv("MPI_PROCNUM") && getenv("MPI_NPROC") 
	  && getenv("MPI_HOSTPORT") && getenv("MPI_HOST")))
	Error("startup variables not in environment\n");

    MPI_Procnum = atoi(getenv("MPI_PROCNUM"));
    MPI_Nproc = atoi(getenv("MPI_NPROC"));
    hostip = getenv("MPI_HOST");
    hostport = atoi(getenv("MPI_HOSTPORT"));

    if (listen(sock, MPI_Nproc+1) < 0) Error("listen failed, errno=%d\n", errno);

    if( getenv("MPI_SUSPEND") && strlen(getenv("MPI_SUSPEND")) > 0 ){
	suspend_proc = atoi(getenv("MPI_SUSPEND"));
	if (suspend_proc == -1) /* suspend all */
	  suspend_proc = MPI_Procnum;
	if (MPI_Procnum == suspend_proc){
	    Shout("suspending pid=%d, MPI_Procnum=%d\n", 
		  my_pid, MPI_Procnum);
#ifdef linux
	    sleep(10);		/* hack. How can we do it right? */
#else
	    kill(my_pid, SIGSTOP);
#endif
	}
    }

    if (getenv("MPI_MESSAGE_TURNON") 
	&& strlen(getenv("MPI_MESSAGE_TURNON")) > 0 ) {
      sprintf(msgfile, "msgs/msg.%d", MPI_Procnum);
      MsgdirInit(msgfile);
      Msg_turnon(getenv("MPI_MESSAGE_TURNON"));
    }
    /* This allows us to be polite about hung processes */
    if (getenv("MPI_TIMEOUT") && strlen(getenv("MPI_TIMEOUT")) > 0 ) 
	MPMY_TimeoutSet(atoi(getenv("MPI_TIMEOUT")));

    /* This lets us use the MPMY stuff like Error */
    _MPI_Procnum = MPI_Procnum;
    _MPI_Nproc = MPI_Nproc;
    _MPMY_procnum_ = MPI_Procnum;
    _MPMY_nproc_ = MPI_Nproc;
    _MPMY_initialized_ = 1;
    _MPMY_setup_absigs();
    MPMY_OnAbnormal(MPMY_SystemAbort);
#if 1 /* should this be under argc/argv control? */
    sprintf(MPMY_Abchdir_arg, "mpi/%03d", MPI_Procnum);
    MPMY_OnAbnormal(MPMY_Abchdir);
#endif
    MPMY_OnAbnormal(mpi_diagnostics);
    MPMY_OnAbnormal(MPMY_Abannounce);
    singlAutoflush(1);
    
    /* fill in host_addr here */
    memset(&host_addr, sizeof(host_addr), 0);
    host_addr.sin_family = AF_INET;
    /* Now try to figure out the host's address from hostname */
    /* Don't bother with gethostbyname here.  Assume that the host
       has worked out its preferred numeric IP address and passed that
       to us through the environment var MPI_HOST */
    if ((inaddr = inet_addr(hostip)) != INADDR_NONE) /* it is numeric */
      host_addr.sin_addr.s_addr = inaddr;
    else
      Error("inet_addr(%s) failed, errno=%d\n", hostip, errno);
    host_addr.sin_port = htons(hostport);
    Msgf(("mpi: host is %s, address is %s, port is %d\n", 
	  hostip, inet_ntoa(host_addr.sin_addr), ntohs(host_addr.sin_port)));

    addr =   (struct sockaddr_in *) calloc(MPI_Nproc+1, size);
    s = calloc(MPI_Nproc+1, sizeof(int));
    if (addr == NULL || s == NULL) Error("out of memory\n");
    addr++;			/* offset by one so host is at -1 */
    s++;

    read_active = malloc(MPI_Nproc * sizeof(req_t *));
    write_active = malloc(MPI_Nproc * sizeof(req_t *));
    recv_list = malloc(MPI_Nproc * sizeof(Dll));
    send_list = malloc(MPI_Nproc * sizeof(Dll));
    if (read_active == NULL || write_active == NULL ||
	recv_list == NULL || send_list == NULL) Error("out of memory\n");
    /* I want to use plain realloc here, and DllCreatChn won't let me */
    /* Thus, I use ChnInit explicitly */
    /* DllCreateChn(&req_chn, sizeof(req_t), 100); */
    ChnInit(&req_chn, sizeof(Dll_elmt)+sizeof(req_t)-sizeof(int), 100, 
	    realloc);
    for (i = 0; i < MPI_Nproc; i++) {
	DllCreate(recv_list+i, &req_chn);
	DllCreate(send_list+i, &req_chn);
	read_active[i] = 0;
	write_active[i] = 0;
    }
    wild_list = &wild_dll;
    DllCreate(wild_list, &req_chn);
    bufr_list = &bufr_dll;
    DllCreate(bufr_list, &req_chn);

    s[HOST_NUM] = sock_connect(&host_addr); /* establish connection to host */

    writemsg_block(s[HOST_NUM], &MPI_Procnum, sizeof(int));
    /* send port on which we listen for connections from other procs */
    writemsg_block(s[HOST_NUM], &my_addr, size);

    /* read array of listening ports */
    readmsg_block(s[HOST_NUM], addr, MPI_Nproc * size);
    Msgf(("mpi: Got port list\n"));

    for (i = 0; i < MPI_Nproc; i++) {
	if (i == MPI_Procnum) {
	    for (j = MPI_Procnum; j < MPI_Nproc-1; j++) {
		int proc, ts;
		memset(&tmp_addr, 0, sizeof(struct sockaddr_in));
		ts = accept(sock, (struct sockaddr *)&tmp_addr, &addr_len);
		if (ts < 0) Error("accept failed, errno=%d\n", errno);
		sock_setopt(ts);
		readmsg_block(ts, &proc, sizeof(int));
		s[proc] = ts;
#ifdef FIONBIO
		/* Isn't this the same as the TCP_NODELAY that we set in
		   sock_setopt() ?  Solaris doesn't have FIONBIO at all.
		   Does it matter? */
		if(ioctl(s[proc], FIONBIO, &noblock)) {
		    Error("ioctl, errno=%d", errno);
		}
#endif
		Msgf(("mpi: Accepted %d on socket %d\n", proc, ts));
	    }
	} else if (MPI_Procnum > i) {
	    s[i] = sock_connect(addr+i); 
	    writemsg_block(s[i], &MPI_Procnum, sizeof(int));
#ifdef FIONBIO
	    /* Isn't this the same as the TCP_NODELAY that we set in
	       sock_setopt() ?  Solaris doesn't have FIONBIO at all.
	       Does it matter? */
	    if(ioctl(s[i], FIONBIO, &noblock)) {
		Error("ioctl, errno=%d", errno);
	    }
#endif
	    Msgf(("mpi: Connected to %d on socket %d\n", i, s[i]));
	}
    }
    Max_fd = 0;
    for (i = 0; i < MPI_Nproc; i++) {
	if (s[i] > Max_fd) Max_fd = s[i];
    }
    s[MPI_Procnum] = -1; /* we should never connect to ourself */
    if (MPI_Nproc > 1 && MPI_Procnum == 0) sock_getopt(s[1]);
    MPI_Barrier(MPI_COMM_PRIVATE);
    zero_hwclock();	/* zero timer here */
    Msgf(("mpi: hwclock zeroed\n"));
}

/* _MPI_init_host1 is called BEFORE the host forks off child processes.
   It has to figure out its own port and hostname, so it can pass it
   to the children in the environment. */
void
_MPI_init_host1(int *portp, char **namep, int nproc){
    char *p;

    sock = sock_bind(&host_addr);
    if (listen(sock, nproc+1) < 0) Error("listen failed, errno=%d\n", errno);
    my_pid = getpid();
    MPI_Procnum = HOST_NUM;
    *portp = ntohs(host_addr.sin_port);
    p = inet_ntoa(host_addr.sin_addr);
    *namep = malloc(strlen(p)+1);
    strcpy(*namep, p);
}

/* _MPI_init_host is called after the host forks the child processes.
   Its job is to communicate all the port info with them so they
   can talk to one another directly. */
void
_MPI_init_host(int n)		/* n is how many nodes we talk to */
{
    int i;
    int proc;
    int addr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in tmp_addr;
    int size = sizeof(struct sockaddr_in);

    addr =   (struct sockaddr_in *) calloc(n+1, size);
    s = calloc(n+1, sizeof(int));
    if (addr == NULL || s == NULL) Error("out of memory\n");
    addr++;			/* offset by one so host is at -1 */
    s++;

    for (i = 0; i < n; i++) {
	int ts;
	memset(&tmp_addr, 0, sizeof(struct sockaddr_in));
	ts = accept(sock, (struct sockaddr *)&tmp_addr, &addr_len);
	if (ts < 0) Error("accept failed, errno=%d\n", errno);
	readmsg_block(ts, &proc, sizeof(int)); /* who did it come from */
	readmsg_block(ts, addr+proc, size);    /* address that is listening */
	s[proc] = ts;
	Msgf(("mpi: Received addr from %d\n", proc));
    }
    s[HOST_NUM] = -1;
    
    for (i = 0; i < n; i++) {
	writemsg_block(s[i], addr, n * size);
	Msgf(("mpi: Sent addrs to %d\n", i));
    }
}

static void
sock_setopt(int fd)
{
    int nodelay = 1;
    int no_check = 0;		/* not clear if this does anything */
    int sendbuf = 65535;
    int recvbuf = 65535;


    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const void *)&nodelay, sizeof(int)))
	Error("setsockopt tcp_nodelay, errno=%d", errno);
    /* The idea is to turn off checksumming, since ethernet does it anyway */
#ifdef SO_NO_CHECK
    /* SunOS doesn't have SO_NO_CHECK */
    if (setsockopt(fd, SOL_SOCKET, SO_NO_CHECK, (const void *)&no_check, sizeof(int)))
	Error("sockopt no_check, errno=%d", errno);
#endif
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const void *)&sendbuf, sizeof(int)))
	Error("sockopt sndbuf, errno=%d", errno);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const void *)&recvbuf, sizeof(int)))
	Error("sockopt rcvbuf, errno=%d", errno);
}

static void
sock_getopt(int fd)
{
    int val, len;

    len = sizeof(int);
    if (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&val, &len))
	Error("getsockopt,  errno=%d", errno);
    Msgf(("mpi: tcp_nodelay %d\n", val));
    len = sizeof(int);
    if (getsockopt(fd, IPPROTO_TCP, TCP_MAXSEG, (void *)&val, &len))
	Error("getsockopt,  errno=%d", errno);
    Msgf(("mpi: tcp_maxseg %d\n", val));
    len = sizeof(int);
#ifdef SO_NO_CHECK
    if (getsockopt(fd, SOL_SOCKET, SO_NO_CHECK, (void *)&val, &len))
	Error("getsockopt, errno=%d", errno);
    Msgf(("mpi: so_no_check %d\n", val));
#endif
    len = sizeof(int);
    if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&val, &len))
	Error("getsockopt, errno=%d", errno);
    Msgf(("mpi: so_sndbuf %d\n", val));
    len = sizeof(int);
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&val, &len))
	Error("getsockopt, errno=%d", errno);
    Msgf(("mpi: so_rcvbuf %d\n", val));
    len = sizeof(int);
}

static char *
printSockaddr(const struct sockaddr_in *sa){
  static char ans[512];
  sprintf(ans, "sockaddr_in: family: %d, sin_addr: %s, sin_port is %d\n", 
	  sa->sin_family, inet_ntoa(sa->sin_addr), ntohs(sa->sin_port));
  return ans;
}

/* Connect with listening socket described by acc, return a descriptor */
static int
sock_connect(struct sockaddr_in *acc)
{
    int ret, fd;

    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	Error("socket failed, errno=%d\n", errno);
    sock_setopt(fd);
#if 0
    do {
	/* we should use a select here */
	ret = connect(fd, (struct sockaddr *)acc, sizeof(struct sockaddr_in));
	if (ret && errno != ECONNREFUSED) 
	    Error("connect failed, errno=%d\n", errno);
	else Msgf(("mpi: connection refused\n"));
	if (max_retries < 10) sleep(1);	/* try not to beat on the other side */
    } while (ret && max_retries--);
#else
    /* We want to make this non-blocking, so we can select on it. */
    fcntl(fd, F_SETFL, O_NONBLOCK);
    ret = connect(fd, (struct sockaddr *)acc, sizeof(struct sockaddr_in));
    if( ret ){
      if( errno == EINPROGRESS ){
	fd_set wtfds;
	struct timeval timeout;
	int intlen;
	FD_ZERO(&wtfds);
	FD_SET(fd, &wtfds);
	timeout.tv_sec = 20;
	timeout.tv_usec = 0;
	ret = select(fd+1, NULL, &wtfds, NULL, &timeout);
	if( ret < 0 )
	  Error("initial select on socket fails, errno=%d\n", errno);
	if( ret == 0 )
	  Error("Socket never became ready, timing out\n");
	if( ret != 1 )
	  Error("Select returns unexpected value: %d.  Giving up\n", ret);
	/* Ok, there's one fd ready, we can look at it to see whether
	   it had a genuine error, or it's hunky dory (c.f., man connect) */
	intlen = sizeof(ret);
	getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&ret, &intlen);
	if( ret ){
	  Error("getsockopt says SO_ERROR=%d trying to connect to %s, Dazed and confused\n", ret, printSockaddr(acc));
	}
      }else{
	Error("connect failed with errno=%d\n", errno);
      }
    }
#endif
    return fd;
}

/* Bind a port to listen on, fill acc with info, return a descriptor */
static int
sock_bind(struct sockaddr_in *acc)
{
    int fd, ret;
    int len = sizeof(struct sockaddr_in);
    char hostname[256];
    struct hostent *hp;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0 ) Error("socket failed, errno=%d\n", errno);
    sock_setopt(fd);
    memset(acc, 0, sizeof(struct sockaddr_in));
    acc->sin_family = AF_INET;
    acc->sin_addr.s_addr = INADDR_ANY;
    acc->sin_port = 0;		/* INADDR_ANY? */
    ret = bind(fd, (struct sockaddr *) acc, sizeof(struct sockaddr_in));
    if (ret < 0 ) Error(" Can't bind socket. errno=%d\n", errno);
    if (getsockname(fd, (struct sockaddr *)acc, &len))
	Error(" Can't getsockname.  errno=%d\n", errno);

    /* Unfortunately, getsockname doesn't replace INADDR_ANY with
       a valid saddr_in */
    /* We should be able to overrule gethostname with an env var
       or a cmd-line arg */
    if (gethostname(hostname, sizeof(hostname)))
	Error("gethostname failed, errno=%d\n", errno);
    hostname[sizeof(hostname)-1] = '\0';
    if ((hp = gethostbyname(hostname)) == NULL)
	Error("gethostbyname(%s) failed\n", hostname);
    memcpy(&(acc->sin_addr), hp->h_addr, hp->h_length);
	
    /* Now acc holds 'correct' info about the socket */
    Msgf(("mpi: Bound port %d\n", ntohs(acc->sin_port)));
    errno = 0;		/* clear errors */
    return fd;
}

static void 
req_done(Dll_elmt *elmt, MPI_Status *status)
{
    req_t *req = DllData(elmt);
    Dll *remove;
    int len;

    if (req->left || req->pending)
	Error("req_done called before all data was delivered\n");

    if (status) {
	status->MPI_SOURCE = req->proc;
	status->MPI_TAG = req->tag;
	status->count = req->len;
    }
    if (req->outgoing) {
	Msgf(("mpi: %9f delivered (%d.%d) to %d len %d\n", hwclock(), 
	      req->tag, req->comm, req->proc, req->len));
	remove = send_list+req->proc;
    } else {
	Msgf(("mpi: %9f %s (%d.%d) from %d len %d\n", hwclock(), 
	      (req->buffered) ? "unbuffered" : "received", 
	      req->tag, req->comm, req->proc, req->len));
	if (req->buffered) remove = bufr_list;
	else if (req->wild_src) remove = wild_list;
	else remove = recv_list+req->proc;
    }
    DllDelete(remove, elmt);
    len = DllLength(remove);
    if (len < 0 || len > MAX_NPOSTANY) Error("bad dll length (%d)\n", len);
}

static req_t *
io_pending(Dll *d)
{
    Dll_elmt *p;

    for (p = DllBottom(d); p != DllSup(d); p = DllUp(p)) {
	req_t *r = DllData(p);
	if (r->pending) return r;
    }
    return NULL;
}


/* if req is set, block until req is clear */
static void
spin_io(Dll_elmt *elmt)
{
    int i, j, ret;
    fd_set rdset, wtset;
    struct timeval timeout, *tv;
    req_t *req = NULL;
    req_t *r;
    req_t *wild_pending;
    double t1, t2;

    if (elmt) {
	int npoll = 100;
	req = DllData(elmt);
	Msgf(("mpi: %9f spin_io %s (%d.%d) %d \n",  hwclock(), (req->outgoing) 
	      ? "sending" : "receiving", req->tag, req->comm, req->proc));

	/* Fast path to avoid select overhead  */

	if (req->proc != MPI_ANY_SOURCE && req->proc != MPI_Procnum) {
	    i = req->proc;
	    t1 = hwclock();
	    while (req->pending && npoll-- > 0) {
		if (req->outgoing) {
		    if (write_active[i]) 
			do_io(write_active[i], i);
		    else if ((r = io_pending(send_list+i)) != NULL)
			do_io(r, i);
		} else {
		    if (read_active[i]) 
			do_io(read_active[i], i);
		    else if ((r = io_pending(recv_list+i)) != NULL)
			do_io(r, i);
		}
	    }
#if 0
	    t2 = hwclock();
	    if (req->pending)
		Msgf(("mpi: %9f polled %6.3f msec without success\n", t1,
		      (t2-t1)*1e3));
	    else 
		Msgf(("mpi: %9f polled %6.3f msec (%d times)\n", t1,
		      (t2-t1)*1e3, 30-npoll));
#endif
	}
	if (req->pending == 0) return;
    }

    do {
	FD_ZERO(&rdset);
	FD_ZERO(&wtset);

	wild_pending = io_pending(wild_list);
	for (i = 0; i < MPI_Nproc; i++) {
	    if (i == MPI_Procnum) continue;
	    /* We need to check all read ports if there is a wildcard recv */
	    if (read_active[i] || wild_pending != NULL 
		|| io_pending(recv_list+i) != NULL) 
		FD_SET(s[i], &rdset);
	    if (write_active[i] || io_pending(send_list+i) != NULL) 
		FD_SET(s[i], &wtset);
	}
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (req && MPI_Nproc > 1 && req->proc != MPI_Procnum) {
	    tv = NULL;	/* Don't let select block if there is only 1 proc */
	} else {
	    tv = &timeout;
	}

    again:
	t1 = hwclock();
	ret = select(Max_fd+1, &rdset, &wtset, NULL, tv);
	t2 = hwclock();
	if (ret == -1) {
	    if (errno == EINTR) goto again; /* SIGPROF interrupts select */
	    else Error("select, errno=%d\n", errno);
	}
	mpi_statistics[SelectTm] += t2-t1;
	Msgf(("mpi: %9f select %6.3f msec\n", t1, (t2-t1)*1e3));

	/* We use read_active and write_active to make sure we finish */
	/* reading an entire message before starting on a new one */

	/* This might work better if one did hypercube channels first */
	/* or, flipped reads and writes based on procnum parity, etc. */

	for (j = 0; j < MPI_Nproc; j++) {
	    i = (j + MPI_Procnum) % MPI_Nproc;
	    if (i == MPI_Procnum) continue;
	    if (FD_ISSET(s[i], &rdset)) {
		if (read_active[i]) 
		    do_io(read_active[i], i);
		else if ((r = io_pending(recv_list+i)) != NULL)
		    do_io(r, i);
		else if ((r = io_pending(wild_list)) != NULL)
		    if (!r->hdr_flag || r->proc == i) 
			do_io(r, i);
	    }
	}

	for (j = 0; j < MPI_Nproc; j++) {
	    i = (j + MPI_Procnum) % MPI_Nproc;
	    if (i == MPI_Procnum) continue;
	    if (FD_ISSET(s[i], &wtset)) {
		if (write_active[i]) 
		    do_io(write_active[i], i);
		else if ((r = io_pending(send_list+i)) != NULL)
		    do_io(r, i);
	    }
	}

	/* Take care of messages to ourself */
	if (read_active[MPI_Procnum]) 
	    do_io_local(read_active[MPI_Procnum], MPI_Procnum);
	else if (io_pending(send_list+MPI_Procnum) != NULL) {
	    if ((r = io_pending(recv_list+MPI_Procnum)) != NULL)
		do_io_local(r, MPI_Procnum);
	    else if ((r = io_pending(wild_list)))
		if (!r->hdr_flag || r->proc == MPI_Procnum) 
		    do_io_local(r, MPI_Procnum);
	}
    } while (req && req->pending);
}

/* Return number of bytes if we got any, to help out spin_io */
static int
do_io(req_t *req, int proc)
{
    int n = 0;
    int left;
    double t1, t2;
    msghdr_t hdr;
    int fd = s[proc];

    if (!req->hdr_flag) {

	/* In order to reduce latency, we package the header together */
	/* with some data.  One would think writev would work, but it */
	/* does not.  tcpdump indicates we get multiple packets from writev */
	/* Doing this gets latency from 350 usec down to 220 (with polling) */

	if (req->outgoing) {
	    char pktbuf[1024+sizeof(msghdr_t)];
	    msghdr_t *hdrp;
	    hdrp = (msghdr_t *)pktbuf;
	    hdrp->src = MPI_Procnum;
	    hdrp->comm = req->comm;
	    hdrp->tag = req->tag;
	    hdrp->len = req->len;
	    hdrp->magic = H_MAGIC;
	    n = MIN(req->left, 1024);
	    memcpy(pktbuf + sizeof(msghdr_t), req->ptr, n);
	    t1 = hwclock();
	    if (writemsg(fd, pktbuf, n+sizeof(msghdr_t)) == -1) return 0;
	    t2 = hwclock();
	    Msgf(("mpi: %9f %s %6.3f msec %2d %5d - %5.2f Mb/s\n", 
		  t1, "wrote ", (t2-t1)*1000.0, proc, n, n/(1e6*(t2-t1))));
	    write_active[proc] = req;
	    req->left -= n;
	    req->ptr += n;
	    mpi_statistics[SendBytes] += n;
	    mpi_statistics[SendTm] += t2-t1;
	} else {
	    if (readmsg(fd, &hdr, sizeof(msghdr_t)) == -1) return 0;
	    /* We may decide we match a different request */
	    req = parse_hdr(&hdr, req, proc);
	    read_active[proc] = req;
	}
	req->hdr_flag = 1;	/* mark header done */
    }

    if (req->pending == 0) Error("req already completed\n");
    if (req->proc != proc) Error("req is inconsistent with proc\n");
    left = MIN(req->left, PKTSIZE);
    /* Small messages get sent with the header */
    if (left) {
	t1 = hwclock();
#if 1
	n = (req->outgoing) ? write(fd, req->ptr, left) 
	   : read(fd, req->ptr, left);
#else
	n = (req->outgoing) ? send(fd, req->ptr, left, 0) 
	   : recv(fd, req->ptr, left, 0);
#endif
	t2 = hwclock();
	if (n < 0) {
	    if (errno == EAGAIN) n = 0;
	    else Error("%s failed, errno=%d\n", 
		       (req->outgoing) ? "write" : "read", errno);
	}
	if (req->outgoing) {
	    mpi_statistics[SendBytes] += n;
	    mpi_statistics[SendTm] += t2-t1;
	} else {
	    mpi_statistics[RecvBytes] += n;
	    mpi_statistics[RecvTm] += t2-t1;
	}
	Msgf(("mpi: %9f %s %6.3f msec %2d %5d - %5.2f Mb/s\n", 
	      t1, (req->outgoing) ? "wrote " : "read  ",  
	      (t2-t1)*1000.0, proc, n, n/(1e6*(t2-t1))));
	req->left -= n;
	req->ptr += n;
    }
    if (req->left == 0) {
	req->pending = 0;
	if (req->outgoing) write_active[proc] = NULL;
	else read_active[proc] = NULL;
    }
    return n;
}

static int
writemsg(int fd, const void *p, int n)
{
    int nwrote;
    int left = n;
    const char *ptr = p;
    while (left > 0) {
	if ((nwrote = send(fd, ptr, left, 0)) < 0) {
	    if (errno == EAGAIN) {
		if (left == n) return -1;
		else continue;
	    }
	    else Error("write failed, errno=%d\n", errno);
	}
	left -= nwrote;
	ptr += nwrote;
    }
    return 0;
}

static int
readmsg(int fd, void *p, int n)
{
    int nread;
    int left = n;
    char *ptr = p;
    while (left > 0) {
	if ((nread = recv(fd, ptr, left, 0)) < 0) {
	    if (errno == EAGAIN) {
		if (left == n) return -1;
		else continue;
	    }
	    else Error("read failed, errno=%d\n", errno);
	}
	left -= nread;
	ptr += nread;
    }
    return 0;
}

/* Blocking write.  Don't return until all data is sent */
static void
writemsg_block(int fd, const void *p, int n)
{
    int nwrote;
    int left = n;
    const char *ptr = p;
    while (left > 0) {
	if ((nwrote = write(fd, ptr, left)) < 0) {
	    if (errno == EAGAIN) continue;
	    else Error("write failed, errno=%d\n", errno);
	}
	left -= nwrote;
	ptr += nwrote;
    }
}

/* Blocking read.  Don't return until all data is read */
static void
readmsg_block(int fd, void *p, int n)
{
    int nread;
    int left = n;
    char *ptr = p;
    while (left > 0) {
	if ((nread = read(fd, ptr, left)) < 0) {
	    if (errno == EAGAIN) continue;
	    else Error("read failed, errno=%d\n", errno);
	}
	left -= nread;
	ptr += nread;
    }
}

static req_t *
match_tag(msghdr_t *hdr, Dll *wildp, Dll *recvp)
{
    Dll_elmt *p;
    req_t *req;

    for (p = DllBottom(wildp); p != DllSup(wildp); p = DllUp(p)) {
	req_t *r = DllData(p);
	if (r->pending && hdr->tag == r->tag && hdr->comm == r->comm)
	    return r;
    }
    for (p = DllBottom(recvp); p != DllSup(recvp); p = DllUp(p)) {
	req_t *r = DllData(p);
	if (r->pending && hdr->tag == r->tag && hdr->comm == r->comm)
	    return r;
    }
    /* No matches, so make a new entry which points to a malloced buffer */
    /* When Irecv is called with a matching tag, it will find this */
    p = DllInsertAtTop(bufr_list);
    if (DllLength(bufr_list) > MAX_NPOSTANY)
	Error("Too many recvs buffered\n");
    if (p == NULL) Error("dll is NULL\n");
    req = DllData(p);
    req->proc = hdr->src;
    req->pending = 1;
    req->wild_src = 0;
    req->outgoing = req->hdr_flag = req->wild_tag = 0;
    req->buffered = 1;
    req->len = req->left = hdr->len;
    req->comm = hdr->comm;
    req->tag = hdr->tag;
    req->buf = req->ptr = malloc(hdr->len);
    if (req->ptr == NULL) Error("out of memory\n");
    Msgf(("mpi: %9f buffering (%d.%d) from %d len %d\n", 
	  hwclock(), req->tag, req->comm, req->proc, req->len));
    return req;
}

/* return a req_t which matches the header we just read, or else buffer */
req_t *
parse_hdr(msghdr_t *hdr, req_t *req, int proc)
{
    if (hdr->magic != H_MAGIC) 
	Error("bad magic number %d\n", hdr->magic);
    if (hdr->tag != req->tag || hdr->comm != req->comm) {
	if (req->wild_tag && hdr->comm == req->comm) 
	    req->tag = hdr->tag;
	else
	    req = match_tag(hdr, wild_list, recv_list+proc);
    }
    if (hdr->src != req->proc) {
	if (req->wild_src) req->proc = hdr->src;
	else Error("bad src, got %d expected %d\n", hdr->src, req->proc);
    }
    if (hdr->len != req->len) {
	if (hdr->len >= 0 && hdr->len < req->len)
	    req->len = req->left = hdr->len;
	else Error("bad len, got %d expected %d\n", hdr->len, req->len);
    }
    Msgf(("mpi: %9f receiving (%d.%d)\n", hwclock(), req->tag, req->comm));
    return req;
}

static void
do_io_local(req_t *ireq, int proc)
{
    msghdr_t hdr;
    int left;
    double t1, t2;
    req_t *oreq = io_pending(send_list + proc);

    hdr.src = MPI_Procnum;
    hdr.tag = oreq->tag;
    hdr.comm = oreq->comm;
    hdr.len = oreq->len;
    hdr.magic = H_MAGIC;
    ireq = parse_hdr(&hdr, ireq, proc);
    ireq->hdr_flag = oreq->hdr_flag = 1;

    left = MIN(ireq->left, PKTSIZE);
    t1 = hwclock();
    memcpy(ireq->ptr, oreq->ptr, left);
    t2 = hwclock()-t1;
    Msgf(("mpi: %9f %s %2d %5d - %5.2f Mb/s\n", hwclock(), "copy ", 
	  MPI_Procnum, left, left/(1e6*t2)));
    ireq->left -= left;
    ireq->ptr += left;
    oreq->ptr += left;
    if (ireq->left == 0) {
	ireq->pending = 0;
	oreq->pending = 0;
	oreq->left = 0;		/* input may be smaller than output */
    }
}

static void
dump_req(Dll *d)
{	 
    Dll_elmt *p;

    for (p = DllBottom(d); p != DllSup(d); p = DllUp(p)) {
	req_t *r = DllData(p);
	Msg_do("\tproc %3d tag %6d flags %d%d%d%d%d len %6d left %6d\n", 
	      r->proc, r->tag, r->pending, r->hdr_flag, r->wild_src, 
	      r->wild_tag,  r->buffered, r->len, r->left);
    }
}

static void
mpi_diagnostics(void)
{
    int i, sum;

    sum = 0;
    for (i = 0; i < MPI_Nproc; i++)
	sum += DllLength(send_list+i);
    if (sum) {
	Msg_do("mpi: %d writes active:\n", sum);
	for (i = 0; i < MPI_Nproc; i++)
	    dump_req(send_list+i);
    }
    sum = 0;
    for (i = 0; i < MPI_Nproc; i++)
	sum += DllLength(recv_list+i);
    if (sum) {
	Msg_do("mpi: %d reads active:\n", sum);
	for (i = 0; i < MPI_Nproc; i++)
	    dump_req(recv_list+i);
    }
    if (DllLength(bufr_list)) {
	Msg_do("mpi: %d buffered reads active:\n", DllLength(bufr_list));
	dump_req(bufr_list);
    }
    if (DllLength(wild_list)) {
	Msg_do("mpi: %d ANY_SOURCE reads active:\n", DllLength(wild_list));
	dump_req(wild_list);
    }
}


int 
MPI_Init(int *argcp, char ***argvp)
{
    int i;
    init_elt();
    for (i = 0; i < _MPI_NUMSTATS; i++)	mpi_statistics[i] = 0.0;
    mpi_statistics[ExecTm] = hwclock();
    return MPI_SUCCESS;
}

int
MPI_Finalize(void)
{
    int i;
    Msgf(("mpi: Finalize\n"));
    MPI_Barrier(MPI_COMM_PRIVATE);
    mpi_statistics[ExecTm] = hwclock() - mpi_statistics[ExecTm];
    mpi_statistics[SendBytes] /= 1e6;
    mpi_statistics[RecvBytes] /= 1e6;
    for (i = 0; i < _MPI_NUMSTATS; i++) {
	Msg_do("%14s %12.2f\n", mpi_stats_name[i], mpi_statistics[i]);
    }
    MPMY_Abchdir();		/* puts gmon.out in different dirs */
    /* Should we free some of the arrays??? */
    return MPI_SUCCESS;
}

int
MPI_Abort(MPI_Comm comm, int errorcode)
{
    Error("MPI_Abort, errorcode=%d\n", errorcode);
}

double
MPI_Wtime(void)
{
  return hwclock();
}

double
MPI_Wtick(void)
{
  return hwtick();
}

int
MPI_Comm_rank(MPI_Comm comm, int *rank)
{
    *rank = MPI_Procnum;
    return MPI_SUCCESS;
}

int
MPI_Comm_size(MPI_Comm comm, int *size)
{
    *size = MPI_Nproc;
    return MPI_SUCCESS;
}

int
MPI_Get_count(MPI_Status *status, MPI_Datatype type, int *cnt)
{
    if (type < 0 || type >= _MPI_NUMDATATYPES)
	Error("Datatype invalid in Get_count (%d)\n", type);
    *cnt = status->count/MPI_Datasize[type];
    return MPI_SUCCESS;
}

int
MPI_Isend(void *buf, int cnt, MPI_Datatype type, int dest, int tag, 
	     MPI_Comm comm, MPI_Request *req)
{
    Dll_elmt *dll;
    req_t *request;

    Msgf(("mpi: %9f Isend: (%d.%d) to %d len %d\n", 
	  hwclock(), tag, comm, dest, cnt * MPI_Datasize[type]));
    if ((long)buf % MPI_Datasize[type]) 
	Msg_do("mpi: Unaligned send of %s at %p\n",
	       mpi_datatype_name[type], buf);
    mpi_statistics[NSends] += 1.0;

    CheckTypeOK(type);
    if (dest < 0 || dest > MPI_Nproc) 
	Error("dest invalid in Isend (%d)\n", dest);

    dll = DllInsertAtTop(send_list+dest);
    if (dll == 0) Error("dll is NULL\n");
    if (DllLength(send_list+dest) > MAX_NPOST) 
	Error("Too many Isends pending\n");
    request = DllData(dll);
    request->proc = dest;
    request->pending = 1;
    request->outgoing = 1;
    request->hdr_flag = request->wild_src = request->wild_tag = 0;
    request->buffered = 0;
    request->len = request->left = cnt * MPI_Datasize[type];
    request->comm = comm;
    request->tag = tag;
    request->buf = request->ptr = (void *)buf;
    *(MPI_Request *)req = dll;
    return MPI_SUCCESS;
}

int
MPI_Irecv(void *buf, int cnt, MPI_Datatype type, int src, int tag, 
	     MPI_Comm comm, MPI_Request *req)
{
    Dll_elmt *elmt;
    req_t *request;

    Msgf(("mpi: %9f Irecv: (%d.%d) from %d len %d\n", 
	  hwclock(), tag, comm, src, cnt * MPI_Datasize[type]));
    if ((long)buf % MPI_Datasize[type]) 
	Msg_do("mpi: Unaligned recv of %s at %p\n", 
		mpi_datatype_name[type], buf);
    mpi_statistics[NRecvs] += 1.0;

    CheckTypeOK(type);
    if (src != MPI_ANY_SOURCE && (src < 0 || src > MPI_Nproc))
	Error("source invalid in Irecv (%d)\n", src);

    /* Check if we already have the message */
    for (elmt = DllBottom(bufr_list); elmt != DllSup(bufr_list); 
	 elmt = DllUp(elmt)) {
	request = DllData(elmt);
	if ((tag == request->tag || tag == MPI_ANY_TAG) 
	    && (src == request->proc || src == MPI_ANY_SOURCE)
	    && comm == request->comm) {
	    Msgf(("mpi: %9f matched (%d.%d) len %d\n", hwclock(), tag, comm,
		 request->len-request->left));
	    /* We might not have buffered the entire message yet */
	    memcpy(buf, request->buf, request->len-request->left);
	    free(request->buf);
	    request->buf = buf;
	    request->ptr = request->buf + (request->len-request->left);
	    if (request->pending && read_active[request->proc] != request)
		Error("Irecv request was buffered+pending but not active\n");
	    *(MPI_Request *)req = elmt;
	    return MPI_SUCCESS;
	}
    }
    if (src == MPI_ANY_SOURCE) {
	elmt = DllInsertAtTop(wild_list);
	if (DllLength(wild_list) > MAX_NPOSTANY)	
	    Error("Too many wildcard Irecvs pending\n");
    } else {
	elmt = DllInsertAtTop(recv_list+src);
	if (DllLength(recv_list+src) > MAX_NPOST) 
	    Error("Too many Irecvs pending\n");
    }
    if (elmt == 0) Error("elmt is NULL\n");
    request = DllData(elmt);
    if (request == 0) Error("dlldata is NULL\n");
    request->proc = src;
    request->pending = 1;
    request->wild_src = (src == MPI_ANY_SOURCE) ? 1 : 0;
    request->outgoing = request->hdr_flag = request->wild_tag = 0;
    request->buffered = 0;
    request->len = request->left = cnt * MPI_Datasize[type];
    request->comm = comm;
    request->tag = tag;
    request->buf = request->ptr = buf;
    *(MPI_Request *)req = elmt;
    return MPI_SUCCESS;
}

int
MPI_Test(MPI_Request *rptr, int *flag, MPI_Status *status)
{
    Dll_elmt *elmt = *rptr;
    req_t *req;
    double t1;

    if (elmt == NULL) Error("NULL message request\n");
    req = DllData(elmt);

/* req->pending is the same as left being non-zero */
/* except for zero length messages */	 

    t1 = hwclock();
    if (req->pending)	
	spin_io(NULL);		
    t1 = hwclock() - t1;

    mpi_statistics[TestTm] += t1;
    if (req->pending) {
	*flag = 0;
    } else {
	req_done(elmt, status);
	*flag = 1;
    }
    return MPI_SUCCESS;
}

int
MPI_Wait(MPI_Request *rptr, MPI_Status *status)
{
    Dll_elmt *elmt = *rptr;
    req_t *req;
    double t1;

    if (elmt == NULL) Error("NULL message request\n");
    req = DllData(elmt);

    t1 = hwclock();
    while (req->pending) 
	spin_io(elmt);
    t1 = hwclock() - t1;

    mpi_statistics[WaitTm] += t1;
    req_done(elmt, status);
    
    return MPI_SUCCESS;
}

int
MPI_Waitall(int count, MPI_Request *reqv, MPI_Status *statusv)
{
    int i;
    for (i = 0; i < count; i++)
	if (statusv == NULL) MPI_Wait(reqv+i, NULL);
	else MPI_Wait(reqv+i, statusv+i);
    return MPI_SUCCESS;
}

int
MPI_Send(void *buf, int cnt, MPI_Datatype type, int dest, int tag, 
	     MPI_Comm comm)
{
    MPI_Request req;

    MPI_Isend(buf, cnt, type, dest, tag, comm, &req);
    MPI_Wait(&req, 0);
    return MPI_SUCCESS;
}

int
MPI_Recv(void *buf, int cnt, MPI_Datatype type, int src, int tag, 
	 MPI_Comm comm, MPI_Status *status)
{
    MPI_Request req;
    MPI_Irecv(buf, cnt, type, src, tag, comm, &req);
    MPI_Wait(&req, status);
    return MPI_SUCCESS;
}


int 
MPI_Sendrecv(void *sendbuf, int sendcount, MPI_Datatype sendtype,
	     int dest, int sendtag, void *recvbuf, int recvcount, 
	     MPI_Datatype recvtype, int source, int recvtag, 
	     MPI_Comm comm, MPI_Status *status) 
{
    MPI_Request rreq, sreq;
    MPI_Status sstatus;

    Msgf(("mpi: Sendrecv\n"));
    MPI_Irecv(recvbuf, recvcount, recvtype, source, recvtag, comm, &rreq);
    MPI_Isend(sendbuf, sendcount, sendtype, dest, sendtag, comm, &sreq);
    MPI_Wait(&sreq, &sstatus);
    MPI_Wait(&rreq, status);
    return MPI_SUCCESS;
}

int
MPI_Barrier(MPI_Comm comm)
{
    int i, junk;

    Msgf(("mpi: Barrier\n"));
    if (MPI_Nproc == 1) return MPI_SUCCESS;
    if (MPI_Procnum != 0) {
	MPI_Send(&junk, 1, MPI_INT, 0, 1, MPI_COMM_PRIVATE);
	MPI_Recv(&junk, 1, MPI_INT, 0, 2, MPI_COMM_PRIVATE, NULL);
    } else {
	MPI_Request *req = malloc(MPI_Nproc * sizeof(MPI_Request));
	for (i = 1; i < MPI_Nproc; i++)
	    MPI_Irecv(&junk, 1, MPI_INT, i, 1, MPI_COMM_PRIVATE, req+i-1);
	MPI_Waitall(MPI_Nproc-1, req, NULL);
	for (i = 1; i < MPI_Nproc; i++)
	    MPI_Send(&junk, 1, MPI_INT, i, 2, MPI_COMM_PRIVATE);
	free(req);
    }
    return MPI_SUCCESS;
}

#define TAG 3

int
MPI_Alltoall(void *sendbuf, int sendcount, MPI_Datatype sendtype,
	     void *recvbuf, int recvcount, MPI_Datatype recvtype, 
	     MPI_Comm comm) 
{
    int i;
    char *sbuf = sendbuf;
    char *rbuf = recvbuf;
    MPI_Request *rreq, *sreq;
    MPI_Status *status;

    Msgf(("mpi: Alltoall\n"));
    CheckTypeOK(sendtype);
    CheckTypeOK(recvtype);
    rreq = malloc(MPI_Nproc * sizeof(MPI_Request));
    sreq = malloc(MPI_Nproc * sizeof(MPI_Request));
    status = malloc(MPI_Nproc * sizeof(MPI_Status));
    if (rreq == NULL || sreq == NULL || status == NULL) 
	Error("out of memory\n");
    for (i = 0; i < MPI_Nproc; i++) {
	MPI_Irecv(rbuf+i*recvcount*MPI_Datasize[recvtype], recvcount, 
		  recvtype, i, TAG, MPI_COMM_PRIVATE, &rreq[i]);
    }
    for (i = 0; i < MPI_Nproc; i++) {
	MPI_Isend(sbuf+i*sendcount*MPI_Datasize[sendtype], sendcount, 
		  sendtype, i, TAG, MPI_COMM_PRIVATE, &sreq[i]);
    }
    for (i = 0; i < MPI_Nproc; i++) {
	MPI_Wait(&rreq[i], &status[i]);
    }	 
    for (i = 0; i < MPI_Nproc; i++) {
	MPI_Wait(&sreq[i], NULL);
    }
    free(status);
    free(sreq);
    free(rreq);
    return MPI_SUCCESS;
}

int 
MPI_Alltoallv(void *sendbuf, int *sendcnts, int *sdispls, MPI_Datatype stype, 
	      void *recvbuf, int *recvcnts, int *rdispls, MPI_Datatype rtype, 
	      MPI_Comm comm) 
{
    int i;
    char *sbuf = sendbuf;
    char *rbuf = recvbuf;
    MPI_Request *rreq, *sreq;
    MPI_Status *status;

    Msgf(("mpi: Alltoallv\n"));
    CheckTypeOK(stype);
    CheckTypeOK(rtype);
    rreq = malloc(MPI_Nproc * sizeof(MPI_Request));
    sreq = malloc(MPI_Nproc * sizeof(MPI_Request));
    status = malloc(MPI_Nproc * sizeof(MPI_Status));
    if (rreq == NULL || sreq == NULL || status == NULL) 
	Error("out of memory\n");
    for (i = 0; i < MPI_Nproc; i++) {
	MPI_Irecv(rbuf+rdispls[i]*MPI_Datasize[rtype], recvcnts[i], 
		  rtype, i, TAG, MPI_COMM_PRIVATE, &rreq[i]);
    }
    for (i = 0; i < MPI_Nproc; i++) {
	MPI_Isend(sbuf+sdispls[i]*MPI_Datasize[stype], sendcnts[i], 
		  stype, i, TAG, MPI_COMM_PRIVATE, &sreq[i]);
    }
    for (i = 0; i < MPI_Nproc; i++) {
	MPI_Wait(&rreq[i], &status[i]);
    }	 
    for (i = 0; i < MPI_Nproc; i++) {
	MPI_Wait(&sreq[i], NULL);
    }
    free(status);
    free(sreq);
    free(rreq);
    return MPI_SUCCESS;
}

/* These Comm functions are not really implemented */

int
MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    *newcomm = comm;
    return MPI_SUCCESS;
}

int
MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm) 
{
    *newcomm = comm;
    return MPI_SUCCESS;
}

int
MPI_Comm_free(MPI_Comm *commp)
{
    return MPI_SUCCESS;
}



/* These Type functions are not really implemented */

int
MPI_Type_contiguous(int len, MPI_Datatype type, MPI_Datatype *ptr)
{
  return MPI_SUCCESS;
}

int
MPI_Type_commit(MPI_Datatype *ptr)
{
  return MPI_SUCCESS;
}

/* g77 */
#define _F77(sym) sym##__
#include "swampif.c"
#undef _F77

/* pgf77 */
#define _F77(sym) sym##_
#include "swampif.c"
#undef _F77

