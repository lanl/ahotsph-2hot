#define KEYdotC
/* Most of the definitions are in key.h */
#include <stdio.h>		/* just for sprintf */
#include "key.h"
#include "protos.h"

/* Non-inlined definitions go here. */

char *
PrintKey(Key_t key)
{
    static char str[128];

    if (NK == 1)
      sprintf(str, "%lo", key.k[0]);
    else {
	if (key.k[1] == 0) {
	    sprintf(str, "%lo", key.k[0]);
	} else {
	    /* This only works for NDIM==3 */
	    sprintf(str, "%lo%01lo%021lo", key.k[1] >> 2, /* bits 66-126*/
		    ((key.k[1] & 03) << 1) | (key.k[0] >> 63), /* bits 63,64,65 */
		    key.k[0] & ~(1L << 63));	/* bits 0-62 */
	}
    }
    return str;
}

#if 0
int
TreeLevel(Key_t key, int ndim)
{
    int level;
    int chubits = (KEYBITS-1)/ndim;
    Key_t testkey;

    /* First check whether it's a 'body' (at the deepest level.)  */
    /* This will save considerable time... */
    testkey = KeyLshift(KeyInt(1), chubits*ndim);
    if( KeyEQ( testkey, KeyAnd(testkey, key) ) )
	return chubits;

    /* Now start looking from low levels */
    testkey = KeyInt(1);
    for (level = 0; level<chubits; level++){
	if( KeyEQ(key, testkey) )
	    return level;
	key = KeyRshift(key, ndim);
    }
    
    return -1;
}

/* Find the common level between two 'body'-keys */
int
CommonLev(Key_t bkey1, Key_t bkey2, int ndim)
{
    int level = (KEYBITS-1)/ndim;
    Key_t key0 = KeyInt(0);

    for (bkey1 = KeyXOR(bkey1, bkey2); KeyNEQ(bkey1, key0); level--)
      bkey1 = KeyRshift(bkey1, ndim);
    
    return(level);
}

int
KeyContained(Key_t outer, Key_t key, int ndim)
{
    int ret;
    int difference;
    
    /* What if difference is negative?! */
    difference = TreeLevel(key, ndim) - TreeLevel(outer, ndim);
    key = KeyRshift(key, ndim*difference);
    ret = KeyEQ(key, outer);
    return(ret);
}
#endif
