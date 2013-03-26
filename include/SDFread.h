#ifndef _RdDataDOTh
#define _RdDataDOTh

#include "timers.h"
#include "SDF.h"
/* Can read distributed datafiles if csdfp contains something like:
struct {char datafiles[64];}[4] = {"foo1", "foo2", "foo3", "foo4"};
*/
#ifdef __cplusplus
extern "C" {
#endif
extern Timer_t SDFreadTm;

/* By default, SDFread will look for a "char datafile[]" in csdfp and
   read data from there.  The name of the variable to look for is
   stored in this variable.  I.e., it defaults to "datafile".  Set it
   to NULL to turn this feature off altogether. */
extern char *SDFread_datafile;

/* Do the same thing with "hdrfile" */
extern char *SDFread_hdrfile;

/* Also by default, SDFread will look for a variable "int npart" in csdfp
   and attempt to read that many "particles" from datafile.  This variable
   storest the name of that variable.  Default:  "npart"; */
extern char *SDFread_npart;

SDF *SDFread(SDF *csdfp, void **btabp, int *gnobjp, int *nobjp, int stride,
	    /* char *name, offset_t offset, int *confirm */...);
SDF *SDFread64(SDF *csdfp, void **btabp, int64_t *gnobjp, int *nobjp, int stride,
	    /* char *name, offset_t offset, int *confirm */...);
SDF *SDFreadf(char *hdr, char *name, void **btabp, int *gnobjp, int *nobjp, 
	    int stride, /* char *name, offset_t offset, int *confirm */...);
SDF *SDFreadf64(char *hdr, char *name, void **btabp, int64_t *gnobjp, int *nobjp, 
		int stride, /* char *name, offset_t offset, int *confirm */...);
#ifdef __cplusplus
}
#endif
#endif
