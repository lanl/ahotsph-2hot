/* Some common routines for dealing with files. */
#include <unistd.h>
#include <fcntl.h>
#include "protos.h"
#include "Malloc.h"
#include "mpmy.h"

int fexists(const char *name){
    int fd, ret;

    ret = 0;
    if( MPMY_Procnum() == 0 ){
	/* We could call stat, but then we'd have to deal with the */
	/* complications of different flavors of struct stat on different */
	/* machines...Yuck. */
	if( (fd=open(name, O_RDONLY)) >= 0 ){
	    close(fd);
	    ret = 1;
	}
    }
    MPMY_Combine(&ret, &ret, 1, MPMY_INT, MPMY_SUM);
    return ret;
}

int fexists_and_unlink(const char *name){
    int fd, ret;

    ret = 0;
    if( MPMY_Procnum() == 0 ){
	/* We could call stat, but then we'd have to deal with the */
	/* complications of different flavors of struct stat on different */
	/* machines...Yuck. */
	if( (fd=open(name, O_RDONLY)) >= 0 ){
	    close(fd);
	    ret = 1;
	}
	unlink(name);
    }
    MPMY_Combine(&ret, &ret, 1, MPMY_INT, MPMY_SUM);
    return ret;
}

int
ForceCheckpoint(void)
{
    return fexists_and_unlink("_ForceCheckpoint_") || fexists("_ForceStop_");
}

int
ForceOutput(void)
{
    return fexists_and_unlink("_ForceOutput_");
}

int
ForceStop(void)
{
    return fexists_and_unlink("_ForceStop_");
}
