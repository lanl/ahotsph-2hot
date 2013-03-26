#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "error.h"
#include "singlio.h"
#include "Msgs.h"
#include "protos.h"
#include "mpmy.h"
#include "byteswap.h"
#include "memfile.h"

static void sock_init(char *hostname, int *port, 
		      struct sockaddr_in *acc, int bind_flag);

static void setup_handler(void);
static void io_ready(int);

static int sock;		/* file descriptor for my UDP socket */

void
sigio_setup(void)
{
    int port = 4000;
    struct sockaddr_in my_addr;

    setup_handler();
    sock_init(NULL, &port, &my_addr, 1); /* get my sockaddr */
    
    if (fcntl(sock, F_SETOWN, getpid()) < 0) 
      Error("F_SETOWN error\n");

#ifdef FASYNC
    if (fcntl(sock, F_SETFL, FASYNC) < 0)
      Error("F_SETFL FASYNC error\n");
#endif

}


static void
setup_handler(void)
{
    signal(SIGIO, io_ready);
}

static void
io_ready(int sig)
{
    int node;

    PrintMemfile();
    signal(SIGIO, io_ready);
    return;
}

/* This is virtually identical to the lsv code */

static void
sock_init(char *hostname, int *port, struct sockaddr_in *acc, int bind_flag)
{
    struct hostent *hp;
    char host_name[256];
    unsigned long inaddr;
    int tries = 0;
    
    if (hostname == NULL) {
	if( (hostname = getenv("LSV_HOSTNAME")) == NULL ){
	    if (gethostname(host_name, 256))
		Error("sock_create: gethostname failed\n");
	    hostname = host_name;
	}
    }
    memset(acc, 0, sizeof(struct sockaddr_in));
    acc->sin_family = htons(AF_INET);

    if ((inaddr = inet_addr(hostname)) != -1) /* it is numeric */
      acc->sin_addr.s_addr = inaddr;
    else if ((hp = gethostbyname(hostname)) != (struct hostent *)0)
      memcpy(&(acc->sin_addr), hp->h_addr, hp->h_length);
    else
      Error("gethostbyname failed\n");

    if (bind_flag) {
	sock = socket(AF_INET, SOCK_DGRAM, 0);
    }
  try_again:
    acc->sin_port = htons(*port);
    if (bind_flag) {
	int ret;
	ret = bind(sock,(struct sockaddr *)acc,sizeof(struct sockaddr_in));
	if (ret < 0 ) { 
	    if (tries < 100)  {
		/* printf("bind returns %d\n", ret); */
		(*port)++;
		tries++;
		goto try_again;
	    } else {
		Error("Can't bind socket. Tried %d, up to port %d\n", 
		      tries, *port);
		exit(1);
	    }
	}
	Msg_do("sigio_dump at %s port %d\n", hostname, *port);
	errno = 0;		/* clear errors */
    }
}
