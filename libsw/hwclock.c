#include <stdio.h>
#include <string.h>
#include "error.h"
#include "hwclock.h"

static double hwtick_val;
#define two_to_32 4294967296.0

static double hwclock_offset;

#if defined(__x86_64__) && defined(__GNUC__)
#define DEFAULT_MHZ 1200.e6
static double
mhz_from_proc(){
  FILE *fp = fopen("/proc/cpuinfo", "r");
  char line[512];

  if( fp == NULL ){
    Warning("Can't open /proc/cpuinfo\n");
    return DEFAULT_MHZ;
  }
    
  while( fgets(line, sizeof(line), fp) ){
    char *m;
    int nscan;
    double mhz;
    
    if( (m = strstr(line, "cpu MHz")) ){
      /* sscanf should match any amount of whitespace around the : */
      nscan = sscanf(m, "cpu MHz : %lf\n", &mhz);
      if( nscan == 1 ){
	fclose(fp);
	return mhz*1.0e6;
      }else{
	Warning("sscanf returns %d, on '%s' of /proc/cpuinfo mhz line failed\n", nscan, line);
	fclose(fp);
	return DEFAULT_MHZ;	/* let's just guess 200Mhz */
      }
    }
  }
  Warning("Did not find cpu MHz in /proc/cpuinfo\n");
  fclose(fp);
  return DEFAULT_MHZ;
}
#endif

/* This doesn't belong here, but I can't be bothered to figure out where
   it does belong! */
double 
hwclock(void)
{
#if defined(__x86_64__)
    static int init = 1;
    unsigned int counter[2];
  
    __asm__("rdtsc \n\t"
      "movl %%eax,%0 \n\t"
      "movl %%edx,%1 \n\t"
      : "=m" (((unsigned *)counter)[0]), "=m" (((unsigned *)counter)[1])
      :
      : "eax" , "edx");

    if (init) {
	init = 0;
	hwtick_val = 1.0/mhz_from_proc();
	hwclock_offset = hwtick_val*((double)counter[1]*two_to_32 + (double)counter[0]);
	return 0.0;
    }
    return(hwtick_val*((double)counter[1]*two_to_32 + (double)counter[0])-hwclock_offset);
#else
    {
      struct timeval tp;
      struct timezone tzp;

      gettimeofday(&tp,&tzp);
      return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
    }
#endif
}

void
zero_hwclock(void)
{
    hwclock_offset += hwclock();
}

double
hwtick(void)
{
  return hwtick_val;
}
