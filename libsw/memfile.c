#include <stdio.h>
#include <string.h>
#include "Malloc.h"
#include "Msgs.h"
#include "error.h"
#include "protos.h"
#include "mpmy.h"

/* Here we try to implement a cicular memory buffer which we can use as */
/* the vfprintf-like arg to Msg_init */

static char *memfile;
static int memfile_offset;
static int memfile_bufsz;

void
memfile_init(int sz)
{
    memfile = Malloc(sz);
    memfile_offset = 0;
    memfile_bufsz = sz-1;
    memfile[sz-1] = 0;   /* final null to make wrapped output cleaner */
}

void
memfile_delete(void)
{
    Free(memfile);
    memfile_offset = 0;
    memfile_bufsz = 0;
}

#define BUFSZ 1024

void
memfile_vfprintf(void *junk, const char *fmt, va_list args)
{
    char tbuf[BUFSZ];	/* This might overflow, but msgs should be small */
    int i, len;

    vsprintf(tbuf, fmt, args);
    len = strlen(tbuf);
    if (len >= BUFSZ) Error("Buffer overflowed in mem_vfprintf\n");
    for (i = 0; i <= len; i++) {
	memfile[(memfile_offset+i)%memfile_bufsz] = tbuf[i];
    }
    memfile_offset += len;
}

void
PrintMemfile(void)
{
    if (memfile_offset == 0) return;
    printf("----- Messages from procnum %d -----\n", MPMY_Procnum());
    if (memfile_offset < memfile_bufsz) {
	printf("%s\n", memfile);
    } else {
	printf("Buffer has wrapped\n");
	printf("%s\n%s\n", memfile+(memfile_offset+1)%memfile_bufsz,
	       memfile);
    }
    fflush(stdout);
}
