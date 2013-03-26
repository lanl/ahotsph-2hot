/*
 * Copyright 1992,1993 Michael S. Warren.  All Rights Reserved.
 */

/* define this to skip the FIONREAD code entirely */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#ifndef FD_SET
/* Sigh.  Where o' where is FD_SET found sys/types?, sys/select? */
/* Unfortunately, sys/select doesn't always exist! */
#include <sys/select.h>
#endif
#include "protos.h"
#include "lsv.h"
#include "error.h"
#include "Msgs.h"
#include "Assert.h"
#include "Malloc.h"

#ifdef __DO_SWAP__
/* There's probably a better way to do most of the multi-word swaps... */
#include "byteswap.h"
static int _x, _y;
#define Swap(x) (_x=x, Byteswap(sizeof(int), 1, &_x, &_y), _y)
#else
#define Swap(x) x
#endif

/* This test should be accomplished some other way!! */
#if defined(USE_ALARM) && !( defined(__x86_64__) || defined(_AIX) || defined(__SUN5__) )
#define HAVE_SIGVEC
#endif

#if defined(__SUN4__)
/* These should really be prototyped somewhere else, 
   but this will do for now */
int close(int);
int getpid(void);
#ifdef HAVE_SIGVEC
int sigvec(int sig, struct sigvec *vec, struct sigvec *ovec);
#endif
int ioctl(int fd, int cmd, void *p);
int gethostname(char *name, int namelen);
char *inet_ntoa(struct in_addr in);
int socket(int domain, int type, int protocol);
int bind(int s, struct sockaddr *name, int namelen);
int recvfrom(int s, void *buf, int len, int flags, 
	     struct sockaddr *from, int *fromlen);
int sendto(int s, const void *msg, int len, 
	   int flags, struct sockaddr *to, int tolen);
int select(int width, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	   struct timeval *timeout);
int getsockname(int s, struct sockaddr *name, int *namelen);
void bzero(char *b, int length);
#endif

#if defined(__SUN5__)
/* This is a bsd-ism.  It may not even exist everywhere?! */
int gethostname(char *name, int namelen);
#endif

#define MAXDEFER 4000		/* maximum number of messages deferred */
#define HLEN (5*sizeof(int))	/* size of packet header */
#define MAXLEN 8192		/* size of total packet */
#define BLOCK (MAXLEN-HLEN)	/* size of basic data packet */
#define ACK_TYPE (-1)		/* Message type for ack */
#define H_MAGIC (0x9f07)	/* Magic number for headers */

#define MAX_PORT_ATTEMPTS 150	/* Number of ports to try before giving up */
#define LSV_TYPE 2		/* Type used to communicate with host */
#define HOST_NUM (-1)		/* can't redefine this without mem adjust. */
#ifndef INADDR_NONE
#define INADDR_NONE -1		/* 'bad' return from inet_addr  */
#endif

static int host_num = HOST_NUM;	/* host integer address */
static int my_pid;		/* my process id */
static unsigned int *seqout;	/* array of outgoing sequence counters */
static unsigned int *seqin;	/* array of incoming sequence counters */
static unsigned int *nretry;	/* array of retry counts */
static struct sockaddr_in host_addr;
static struct sockaddr_in my_addr;

static int sock;		/* file descriptor for my UDP socket */
static struct sockaddr_in *addr; /* sockaddrs for all processors */
static char *msgbuf[MAXDEFER];	/* pointers to deferred messages */
static int msgcnt;		/* number of deferred messages */
#ifdef USE_ALARM
static volatile int failed;		/* flag for recvfrom timeout */
static void to_alarm(int);
#endif
static void sock_init(struct sockaddr_in *acc, int bind_flag);
static void common_init(const int n);
static int bsend(int s, const void *outbuf, int sent, int dest, int type);
static int brecv(int s, void *inb, int sent, int *dest, int type, int block);
static int chk_deferw(int type, int *src);
static void send_ack(int s, int dest, int seq, struct sockaddr_in *dest_addr);
static int chk_defer(int src, int type, int seq);

int LSV_procnum;		/* my integer address (procnum) */
int LSV_nproc;			/* how many 'elt's */

/*   If hears nothing before its 'block' arg expires, it returns
     BRECV_TIMEDOUT */
#define BRECV_TIMEDOUT (-2)

/* It sure would be nice to be able to set these at runtime!!! */
/* TIMEOUT1 passed to brecv when we're 'blocking', i.e., from Srecv_block. */
#define TIMEOUT1 200
/* TIMEOUT2 is what we pass when we really expect something, i.e., when
   we are waiting for subsequent blocks after we have received the first
   one. */
#define TIMEOUT2 50
/* Srecv{_block} will allow retry brecv after a timeout this many 
   times before giving up altogether. */
#define NRETRY 2
/* Ssend waits for an ack from the recipient.  It retries this many
   times, incrementing the timer by one each time, so in the end, the
   total time waited is ACK_NRETRY*(ACK_NRETRY+1)/2 seconds.  Now that
   the user code is free to use alarm, we can rely on the user to bail out
   if things seem to be taking too long.  Thus, we set this quite high. */
#ifndef USE_ALARM
#define ACK_NRETRY 100
#else
#define ACK_NRETRY 20
#endif

void
Sclose(void)
{
    close(sock);
    /* Should we free some of the arrays??? */
}

void
Sinit_elt(void)
{
    int suspend_proc;
    int pid;
    int nin;
    int size = sizeof(struct sockaddr_in);
    int hostport;
    char *hostip;
    unsigned long inaddr;

    assert(getenv("LSV_PROCNUM") && getenv("LSV_NPROC") 
	   && getenv("LSV_HOSTPORT") && getenv("LSV_HOST") );

    LSV_procnum = atoi(getenv("LSV_PROCNUM"));
    LSV_nproc = atoi(getenv("LSV_NPROC"));
    hostip = getenv("LSV_HOST");
    hostport = atoi(getenv("LSV_HOSTPORT"));

    /* fill in host_addr here */
    memset(&host_addr, sizeof(host_addr), 0);
    host_addr.sin_family = AF_INET;
    /* Now try to figure out the host's address from hostname */
    /* Don't bother with gethostbyname here.  Assume that the host
       has worked out its preferred numeric IP address and passed that
       to us through the environment var LSV_HOST */
    if ((inaddr = inet_addr(hostip)) != INADDR_NONE) /* it is numeric */
      host_addr.sin_addr.s_addr = inaddr;
    else
      Error("inet_addr(%s) failed, errno=%d\n", hostip, errno);
    host_addr.sin_port = htons(hostport);
    Msgf(("hostip: %s, host_addr.sin_addr %s, host_addr.sin_port: %d\n", 
	  hostip, inet_ntoa(host_addr.sin_addr), ntohs(host_addr.sin_port)));

    sock_init(&host_addr, 0); /* get host sockaddr */
    sock_init(&my_addr, 1); /* get my sockaddr */
    Msgf(("my_addr.sin_addr: %s, my_addr.sin_port: %d\n", 
	  inet_ntoa(my_addr.sin_addr), ntohs(my_addr.sin_port)));


    common_init(LSV_nproc);

    Ssend(&my_addr, size, HOST_NUM, LSV_TYPE); /* send to host */
    
    nin = Srecv_block(addr, LSV_nproc * size, LSV_TYPE, &host_num);
    if (nin != LSV_nproc * size)
	Error("Bad number of addrs received\n");

    if( getenv("LSV_SUSPEND") && strlen(getenv("LSV_SUSPEND")) > 0 ){
	pid = getpid();
	suspend_proc = atoi(getenv("LSV_SUSPEND"));
	if (suspend_proc == -1) /* suspend all */
	  suspend_proc = LSV_procnum;
	if (LSV_procnum == suspend_proc){
	    Shout("suspending pid=%d, LSV_procnum=%d\n", 
		  pid, LSV_procnum);
	    kill(pid, SIGSTOP);
	}
    }
}

/* Sinit_host1 is called BEFORE the host forks off child processes.
   It has to figure out its own port and hostname, so it can pass it
   to the children in the environment. */
void
Sinit_host1(int *portp, char **namep){
    char *p;

    sock_init(&host_addr, 1);
    *portp = ntohs(host_addr.sin_port);
    if( (p=getenv("LSV_HOST")) == NULL ){
      /* If there is no LSV_HOST in the environment, then ask the system */
      p = inet_ntoa(host_addr.sin_addr);
    }
    *namep = malloc(strlen(p)+1);
    strcpy(*namep, p);
}

/* Sinit_host is called after the host forks the child processes.
   Its job is to communicate all the port info with them so they
   can talk to one another directly. */
void
Sinit_host(int n)		/* n is how many nodes we talk to */
{
    int i, nin;
    int size = sizeof(struct sockaddr_in);

    common_init(n);
    Msgf(("After common_init\n"));
    LSV_procnum = HOST_NUM;
    
    for (i = 0; i < n; i++) {
	nin = Srecv_block(&addr[i], size, LSV_TYPE, &i);
	if (nin != size) 
	    Error("Bad recv size (%d) in Sinit_host, i=%d\n", nin, i);
	Msgf(("Received addr[%d]:  family: %d, port: %d, addr: %s\n",
	      i, addr[i].sin_family, ntohs(addr[i].sin_port), 
	      inet_ntoa(addr[i].sin_addr)));
    }
    Msgf(("All sockaddrs received.  Broadcasting to compute processes\n"));
    for (i = 0; i < n; i++) {
	Ssend(addr, n * size, i, LSV_TYPE); 
	Msgf(("Sent addrs to %d\n", i));
    }
}

static void
common_init(const int n)
{
#if defined(HAVE_SIGVEC)
    struct sigvec vec;
#endif

    /* to enforce sequencing */
    seqin =  (unsigned int *) calloc(n+1, sizeof(unsigned int));
    seqout = (unsigned int *) calloc(n+1, sizeof(unsigned int));
    nretry = (unsigned int *) calloc(n+1, sizeof(unsigned int));
    addr =   (struct sockaddr_in *) calloc(n+1, sizeof(struct sockaddr_in));
    if (seqin == NULL || seqout == NULL || nretry == NULL || addr == NULL)
      Error("No more memory in Sinit\n");
    seqin++;			/* allow indexing [-1:n-1] */
    seqout++;
    nretry++;
    addr++;
    
    /* We use the convention that 0, ..., n-1 are nodes, and -1 is the host */
    addr[HOST_NUM] = host_addr;	/* struct assignment */
    my_pid = getpid();
    
#ifdef USE_ALARM
#if defined (HAVE_SIGVEC)
    vec.sv_handler = to_alarm;
    vec.sv_flags = SV_INTERRUPT;
    vec.sv_mask = 0;
    sigvec(SIGALRM, &vec, NULL);
#else
    signal(SIGALRM, to_alarm);
#endif /* HAVE_SIGVEC */
#endif /* USE_ALARM */

}

#ifdef USE_ALARM
static void
to_alarm(int sig)
{
    assert( sig == SIGALRM );
    failed = 1;
    signal(SIGALRM, to_alarm);
}
#endif

/* dest should be last argument */

void
Ssend(const void *outb, int outcnt, int dest, int type)
{
    int sent;
    const char *buf = outb;
    int ret;

    Msgf(("Ssend: (%d) to %d len %d\n", type, dest, outcnt));
    do {
	sent = (outcnt > BLOCK) ?  BLOCK : outcnt;
	ret = bsend(sock, buf, sent, dest, type);
	if( ret < 0 ){
	    Error("Ssend: bsend failed, type=%d, dest=%d, outcnt=%d\n",
		  type, dest, outcnt);
	}
	outcnt -= sent;
	buf += sent;
    } while (outcnt || sent == BLOCK);
}

int
Srecv(void *inb, int size, int type, int *from)
{
    int sent, inbytes = 0;
    char *buf = inb;
    int timeout;
    int verbose = 0;

    /* Only the first brecv is non-blocking.  We wait for the rest. */
    sent = brecv(sock, buf+inbytes, size, from, type, 0);
    if( sent < 0 ){
	if( sent == BRECV_TIMEDOUT ){
	    return -1;		/* nothing available, not an error! */
	}
	Error("First brecv failed in Srecv, errno=%d\n", errno);
    }

    inbytes += sent;
    size -= sent;
    timeout = TIMEOUT2;
    while(sent == BLOCK){
    tryagain:
	sent = brecv(sock, buf+inbytes, size, from, type, timeout);
	if( sent < 0 ){
	    if( sent == BRECV_TIMEDOUT ){
		Warning("Srecv:  brecv timed out, will wait indefinitely now\n");
		verbose = 1;
		timeout = -1;
		goto tryagain;
	    }
	    Error("brecv failed in Srecv, inbytes=%d, errno=%d\n", 
		  inbytes, errno);
	}
	inbytes += sent;
	size -= sent;
    }
    if( verbose || Msg_test(__FILE__) )
	Msg_do("Srecv: (%d) from %d ret %d\n", type, *from, inbytes);
    return(inbytes);
}

/* last arg should not be a pointer */

int
Srecv_block(void *inb, int size, int type, int *from)
{
    int sent, inbytes = 0;
    char *buf = inb;
    int timeout = TIMEOUT1;
    int verbose = 0;

    do {
    tryagain:
	sent = brecv(sock, buf+inbytes, size, from, type, timeout);
	if( sent < 0 ){
	    if( sent == BRECV_TIMEDOUT ){
		Warning("Srecv_block: brecv(size=%d, from=%d, type=%d, timeout=%d) timed out, inbytes=%d, errno=%d\n",
			       size, *from, type, timeout,
			       inbytes, errno);
		timeout = -1;
		verbose = 1;
		goto tryagain;
	    }
	    return -1;
	}
	inbytes += sent;
	size -= sent;
	timeout = TIMEOUT2;
    } while (sent == BLOCK);
    if( verbose || Msg_test(__FILE__) )
	Msg_do("Srecv_block: (%d) from %d ret %d\n", type, *from, inbytes);
    return(inbytes);
}

void Sdiag(int (*printf_like)(const char *, ...)){
    int i;
    int *buf;
    printf_like("lsv.c:  counters to and from various destinations\n[dest seqout[dest] seqin[dest] nretry[dest]]\n");
    for(i=-1; i<LSV_nproc; i++){
      printf_like("%d %d %d %d\n", i, seqout[i], seqin[i], nretry[i]);
    }
    printf_like("lsv.c unread message buffers:\n");
    printf_like("[src seqno type length]\n");
    for(i=0; i<msgcnt; i++){
	buf = (int *)msgbuf[i];
	printf_like("%d %d %d %d\n", buf[1], buf[2], buf[3], buf[4]);
    }
}

static int
bsend(int s, const void *outbuf, int sent, int dest, int type)
{
    int incnt;
    int outb[MAXLEN/sizeof(int)];
    int inbuf[MAXLEN/sizeof(int)];
    int src, seq, intype, inlen;
    struct sockaddr_in src_addr;
    int len = sizeof(struct sockaddr_in);
    int retry = 0;
    int nfail = 0;
    int nsent;
    int verbose = 0;
#ifndef USE_ALARM
    struct timeval timeout;
    fd_set rdset;
    int selreturn;
#endif

    Msgf(("bsend seq:%d type:%d len: %d to %d...", seqout[dest], type, sent, dest)); 

    outb[0] = Swap(H_MAGIC);
    outb[1] = Swap(LSV_procnum);
    outb[2] = Swap(seqout[dest]);
    outb[3] = Swap(type);
    outb[4] = Swap(sent);

    memcpy(outb + HLEN/sizeof(int), outbuf, sent);
    sent += HLEN;

  try_again:
    errno = 0;
    retry++;
    if ((nsent=sendto(s, (void *)outb, sent, 0, (struct sockaddr *)(addr+dest), 
	       sizeof(struct sockaddr_in))) != sent) {
	Shout("sendto:  addr: %d %s\n", 
	      ntohs(addr[dest].sin_port), inet_ntoa(addr[dest].sin_addr));
	Error("sendto(s=%d, sent=%d) seqout[%d]=%d returns %d, errno=%d ", 
	      s, sent, dest, seqout[dest], nsent, errno);
    }
    
  ackrecv:
#ifndef USE_ALARM
    timeout.tv_sec = retry;
    timeout.tv_usec = 0;
    FD_ZERO(&rdset);
    FD_SET(s, &rdset);
    selreturn = select(s+1, &rdset, NULL, NULL, &timeout);
    if( selreturn < 0 ){
      if (errno == EINTR) 
	goto ackrecv; /* SIGPROF interrupts select */
      else {
	SeriousWarning("bsend select failed, errno=%d\n", errno);
	return -1;
      }
    }else if(selreturn == 0){
      /* This warning may indicate a serious problem, or it just may
	 mean that the network or the destination node is saturated. */
	Warning("bsend timed out waiting for ack for seqout[%d]=%d\n", dest, 
		seqout[dest]);
	nretry[dest]++;
	if( retry > ACK_NRETRY ){
	    SeriousWarning("bsend timed out %d times waiting for ack\n",
			   retry);
	    return -1;
	}
	verbose = 1;
	goto try_again;
    }
    if( verbose || Msg_test(__FILE__) )
	Msg_do("bsend ackrecv ready\n");
    
    memset(&src_addr, 0, sizeof(struct sockaddr_in));
    incnt = recvfrom(s, (void *)inbuf, MAXLEN, 0, (struct sockaddr *)&src_addr, &len);
#else
    memset(&src_addr, 0, sizeof(struct sockaddr_in));
    failed = 0;
    alarm(1+retry);
    errno = 0;
    incnt = recvfrom(s, inbuf, MAXLEN, 0, (struct sockaddr *)&src_addr, &len);
    if (failed) {
	Msgf(("retry, errno=%d\n", errno)); 
	if (retry > ACK_NRETRY) {
	    SeriousWarning("bsend timed out on ack, errno=%d\n", errno);
	    return -1;
	}
	goto try_again;
    }
    alarm(0);
#endif

    if (incnt < 0) {
	Warning("recvfrom(ack from %d) returns %d, errno=%d\n", dest, incnt, errno);
	if(nfail++ > 5){
	    SeriousWarning("recvfrom(ack from %d) returns %d, errno=%d\n", dest, incnt, errno);
	    return -1;
	}
	goto try_again;
    }
    if (Swap(inbuf[0]) != H_MAGIC) {
	Warning("Bad header in bsend\n");
	goto ackrecv;
    }

#ifdef __DO_SWAP__
    inbuf[1] = Swap(inbuf[1]);
    inbuf[2] = Swap(inbuf[2]);
    inbuf[3] = Swap(inbuf[3]);
    inbuf[4] = Swap(inbuf[4]);
#endif

    src    = inbuf[1];
    seq    = inbuf[2];
    intype = inbuf[3];
    inlen  = inbuf[4];

    if (intype != ACK_TYPE) {	/* got a data packet */
	if (seq < seqin[src])
	  Msgf(("aignore2 %d from %d ", seq, src));
	else if (chk_defer(src, intype, seq) > -1)
	  Msgf(("aignore %d from %d ", seq, src));
	else {
	    msgbuf[msgcnt] = Malloc((incnt+8)&~07);
	    memcpy(msgbuf[msgcnt], inbuf, incnt);
	    msgcnt++;
	    if (msgcnt >= MAXDEFER) Error("msgcnt too large\n");
	    Msgf(("adefer seq:%d type:%d, len:%d from %d ", 
		  seq, intype, inlen, src)); 
	}
	send_ack(s, src, seq, &src_addr);
	goto ackrecv;
    } else {
	if (incnt != HLEN)
	  Warning("Bad incnt, errno=%d", errno);
	else if (dest != src || seq != seqout[dest])
	  Msgf(("aduplicate ack %d from %d ", seq, src));
	else {
	    Msgf(("ack seq: %d, dest: %d\n", seq, dest));
	    seqout[dest]++;
	    return (sent-HLEN);
	}
	goto ackrecv;
    }
}

/* Block is a timeout.  We treat a negative value as meaning to block
   forever*/
static int
brecv(int s, void *inb, int sent, int *dest, int type, int block)
{
    int src, seq, incnt;
    struct sockaddr_in src_addr;
    int len = sizeof(struct sockaddr_in);
    int inlen, i;
    int intype;
    int inbuf[(BLOCK+HLEN)/sizeof(int)];
    fd_set rdset;
    struct timeval timeout, *timeoutp;
    int selreturn;
 
    Msgf(("brecv(sent=%d, dest=%d, type=%d, block=%d)\n",
	  sent, *dest, type, block));
    if (*dest == LSV_ANY)
      i = chk_deferw(type, &src); /* sets src */
    else
      i = chk_defer(*dest, type, seqin[*dest]);
    if (i != -1) {
	if (*dest == LSV_ANY) *dest = src;
	inlen = *(int *)(msgbuf[i]+4*sizeof(int));
	if (inlen > sent) Error("Too much data\n");
	memcpy(inb, msgbuf[i]+HLEN, inlen);
	Free(msgbuf[i]);
	msgbuf[i] = msgbuf[--msgcnt];
	Msgf(("brecv %d (%d) from %d.\n", seqin[*dest], type, *dest));
	seqin[*dest]++;
	return(inlen);
    }

  datrecv:
    if( block >= 0 ){
	timeout.tv_sec = block;
	timeout.tv_usec = 0;
	timeoutp = &timeout;
    }else{
	timeoutp = NULL;
    }
    FD_ZERO(&rdset);
    FD_SET(s, &rdset);
    selreturn = select(s+1, &rdset, NULL, NULL, timeoutp);
    if( selreturn < 0 ){
      if (errno == EINTR) 
	goto datrecv; /* SIGPROF interrupts select */
      else {
	SeriousWarning("select failed, errno=%d\n", errno);
	return -1;
      }
    }else if(selreturn == 0){
	return BRECV_TIMEDOUT;
    }
    Msgf(("brecv any (%d)...", type));

    memset(&src_addr, 0, sizeof(struct sockaddr_in));
    /* What's the best thing to do here if we timed out?  
       return? goto datrecv? something else?*/
    incnt = recvfrom(s, (void *)inbuf, MAXLEN, 0, (struct sockaddr *)&src_addr, &len);

    if (incnt < 0) {
	Warning("brecv: recvfrom, errno=%d", errno);
	goto datrecv;
    }
    if (Swap(inbuf[0]) != H_MAGIC) {
	Warning("Bad header in brecv (%x,%x,%x,%x), incnt %d\n",
		Swap(inbuf[0]), Swap(inbuf[1]), Swap(inbuf[2]), Swap(inbuf[3]),
		incnt);
	goto datrecv;
    }

#ifdef __DO_SWAP__
    inbuf[1] = Swap(inbuf[1]);
    inbuf[2] = Swap(inbuf[2]);
    inbuf[3] = Swap(inbuf[3]);
    inbuf[4] = Swap(inbuf[4]);
#endif

    src    = inbuf[1];
    seq    = inbuf[2];
    intype = inbuf[3];
    inlen  = inbuf[4];
    
    Msgf((" [%d, %d, %d, %d] ", src, seq, intype, inlen));

    if (intype == ACK_TYPE) {
	Msgf(("duplicate ack %d from %d ", seq, src));
	goto datrecv;
    }
    else if (inlen != incnt-HLEN) {
	Shout("Bad inlen in bsend, errno=%d", errno);
	goto datrecv;
    }
    else if (type == intype && seq == seqin[src] &&
	     (*dest == src || *dest == LSV_ANY)) {
	if (*dest == LSV_ANY) {
	    *dest = src;
	    Msgf(("%d from %d ", seq, src));
	}
	send_ack(s, src, seq, &src_addr);
	if (inlen > sent) Error("Too much data\n");
	memcpy(inb, inbuf+HLEN/sizeof(int), inlen);
	seqin[src]++;
	return(inlen);
    } else {
	if (seq < seqin[src])
	  Msgf(("ignore2 %d from %d ", seq, src));
	else if (chk_defer(src, intype, seq) > -1)
	  Msgf(("ignore3 %d from %d ", seq, src));
	else if (src != *dest || type != intype || seq != seqin[src]) {
	    msgbuf[msgcnt] = Malloc((incnt+8)&~07);
	    memcpy(msgbuf[msgcnt], inbuf, incnt);
	    msgcnt++;
	    if (msgcnt >= MAXDEFER) Error("msgcnt too large\n");
	    Msgf(("defer %d (%d) seqin[%d]=%d ", seq, intype, src, seqin[src]));
	}
	send_ack(s, src, seq, &src_addr);
	goto datrecv;
    }
}

static int
chk_defer(int src, int type, int seq)
{
    int i, insrc, inseq, intype;
    
    Msgf(("chk_defer(src=%d, type=%d, seq=%d)\n", src, type, seq));
    for (i = 0; i < msgcnt; i++) {
	insrc = *(int *)(msgbuf[i]+sizeof(int));
	inseq = *(int *)(msgbuf[i]+2*sizeof(int));
	intype = *(int *)(msgbuf[i]+3*sizeof(int));
	if (src == insrc && type == intype && seq == inseq){
	    Msgf(("deferred match: msgbuf[%d]\n", i));
	    return(i);
	}
    }
    Msgf(("no match\n"));
    return(-1);
}

static int
chk_deferw(int type, int *src)
{
    int i, insrc, inseq, intype;
    
    for (i = 0; i < msgcnt; i++) {
	insrc = *(int *)(msgbuf[i]+sizeof(int));
	inseq = *(int *)(msgbuf[i]+2*sizeof(int));
	intype = *(int *)(msgbuf[i]+3*sizeof(int));
	if (type == intype && inseq == seqin[insrc]) {
	    *src = insrc;
	    return(i);
	}
    }
    return(-1);
}

static void
send_ack(int s, int dest, int seq, struct sockaddr_in *dest_addr)
{
    int ack[HLEN/sizeof(int)];

    ack[0] = Swap(H_MAGIC);
    ack[1] = Swap(LSV_procnum);
    ack[2] = Swap(seq);
    ack[3] = Swap(ACK_TYPE);
    ack[4] = Swap(0);
    if (sendto(s, (void *)ack, HLEN, 0, (struct sockaddr *)dest_addr,
	       sizeof(struct sockaddr_in)) != HLEN)
      Warning("sendto, errno=%d", errno);
    else
      Msgf(("acked\n"));
}

static void
sock_init(struct sockaddr_in *acc, int bind_flag)
{
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if( sock < 0 ){
	Error("socket failed, errno=%d\n", errno);
    }
#ifdef SOCKBUF
    {
	int sockbuf;
	/* These appear not to be supported on the delta */
	sockbuf= SOCKBUF;
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sockbuf, sizeof(int))) {
	    SeriousWarning("sockopt sndbuf, errno=%d", errno);
	    exit(1);
	}
	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &sockbuf, sizeof(int))) {
	    SeriousWarning("sockopt rcvbuf, errno=%d", errno);
	    exit(1);
	}
    }
#endif

    if (bind_flag) {
	int ret;
	int len = sizeof(struct sockaddr_in);
	char hostname[256];
	struct hostent *hp;
	char *p;

	/* If we're being asked to do a 'bind', then that implies that
	   we should fill in the sockaddr as well */
	memset(acc, 0, sizeof(struct sockaddr_in));
	acc->sin_family = AF_INET;
	acc->sin_addr.s_addr = INADDR_ANY;
	acc->sin_port = 0;		/* INADDR_ANY? */
	ret = bind(sock,(struct sockaddr *)acc,sizeof(struct sockaddr_in));
	if (ret < 0 ) { 
	    Error("LSV:  Can't bind socket. errno=%d\n", 
		  errno);
	}
	if( getsockname(sock, (struct sockaddr *)acc, &len) ){
	    Error("LSV:  Can't getsockname.  errno=%d\n", errno);
	}

	/* Unfortunately, getsockname doesn't replace INADDR_ANY
	   with a valid saddr_in.  We should be able to overrule
	   gethostname with an env var or a cmd-line arg */
	if( (p = getenv("LSV_MYNAME")) ){
	  strncpy(hostname, p, sizeof(hostname));
	}else{
	  if( gethostname(hostname, sizeof(hostname)) )
	    Error("gethostname failed, errno=%d\n", errno);
	}
	hostname[sizeof(hostname)-1] = '\0';
	if( (hp = gethostbyname(hostname)) == NULL )
	  Error("gethostbyname(%s) failed\n", hostname);
	memcpy(&(acc->sin_addr), hp->h_addr, hp->h_length);
	
	/* Now acc holds 'correct' info about the socket */
	Msgf(("Bound port %d\n", ntohs(acc->sin_port)));
	errno = 0;		/* clear errors */
    }
}
