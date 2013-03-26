/* This really should be implemented for us... */
/* Nevertheless, we should do better.. */
#include <math.h>
#ifndef HUGE
#define HUGE 1.e38
#endif

int finite(double x){
    return x<HUGE && x>-HUGE;
}

