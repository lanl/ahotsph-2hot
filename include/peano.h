#ifndef __PeanoDOt_H_
#define __PeanoDOt_H_
#include "key.h"

Key_t PHKeyFromInts(unsigned int ikey[], int ndim, int depth);
unsigned int IntsFromPHKey(Key_t key, unsigned int ikey[], int ndim);

#endif
