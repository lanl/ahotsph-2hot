#ifndef _KeyDOTh
#define _KeyDOTh

#include <limits.h>

/* Should we use long long keys???
   Compiling tree.c (which does a fair amount of key arith), with 
   LONG_LONG_KEYS turned on results
   in 25% shorter sparc code (12k vs. 9k), and 15% shorter i860 code
   (14k vs 12k).  

   HOWEVER:  in both cases, the LONG_LONG_KEYS code does NOT inline
   the leftshift and rightshift operators.  They are implemented as
   calls to ___lshrdi3 and ___lshldi3 in libgcc.a

   The bottom line:  TBD.  I-cache vs. call overhead, vs. do the __lsh calls
   prevent gcc from doing any optimizations across the call?  I'd guess
   that the LONG_LONG_KEYS are faster.
*/
#if defined(KEY96BITS)
#define NK 3
#define _KTYPE unsigned int
#define KEYBITS 94
#else
#if defined(LONG_NK1_KEY)
#define NK 1
#define _KTYPE unsigned long int
#else
#if defined(LONG_LONG_KEYS)
#define NK 2
#define _KTYPE unsigned long long int
#define KEYBITS 94
#else
#define NK 2
#define _KTYPE unsigned long int
#define KEYBITS 94
#endif

#endif /* LONG_LONG */
#endif /* ONE_LONG */

/* Test for #if FORCE_KEY_ALIGNMENT, not for #ifdef, which gives
   the Make.$(ARCH) the opportunity to do -DFORCE_KEY_ALIGNMENT=0
*/
#if !defined(FORCE_KEY_ALIGNMENT) &&  NK==1
#define FORCE_KEY_ALIGNMENT 1
#endif

/* We have to typedef Key_t as a struct, or else we can't return it from */
/* a function */
typedef struct {
    _KTYPE k[NK];
} Key_t;

/* Be careful! KEYBITS is not necessarily where the "body" bit is located */
#ifndef KEYBITS
#define KEYBITS (CHAR_BIT*sizeof(Key_t))
#endif

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
extern char *PrintKey(Key_t key);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#if (defined(__GNUC__) || defined(__ICC__)) || defined(KEYdotC)

#if (__STDC_VERSION__ >= 199901L) && !defined (KEYdotC)
#define INLINE inline
#else
#if (defined (__GNUC__) || defined(__ICC__)) && !defined (KEYdotC)
#define INLINE extern __inline__
#else
#define INLINE
#endif
#endif

#if NK==1

INLINE int
KeyGT(Key_t key1, Key_t key2)
{
    return (key1.k[0] > key2.k[0]);
}

INLINE int
KeyLT(Key_t key1, Key_t key2)
{
    return (key1.k[0] < key2.k[0]);
}

INLINE int
KeyGE(Key_t key1, Key_t key2)
{
    return (key1.k[0] >= key2.k[0]);
}

INLINE int
KeyLE(Key_t key1, Key_t key2)
{
    return (key1.k[0] <= key2.k[0]);
}

INLINE int
KeyEQ(Key_t key1, Key_t key2)
{
    return (key1.k[0] == key2.k[0]);
}

INLINE int
KeyNEQ(Key_t key1, Key_t key2)
{
    return (key1.k[0] != key2.k[0]);
}

INLINE Key_t
KeyXOR(Key_t key1, Key_t key2)
{
    Key_t ret;
    ret.k[0] = key1.k[0] ^ key2.k[0];
    return(ret);
}

INLINE Key_t
KeyAnd(Key_t key1, Key_t key2)
{
    Key_t ret;
    ret.k[0] = key1.k[0] & key2.k[0];
    return(ret);
}

INLINE Key_t
KeyOr(Key_t key1, Key_t key2)
{
    Key_t ret;
    ret.k[0] = key1.k[0] | key2.k[0];
    return(ret);
}

INLINE int
KeyCmp(Key_t key1, Key_t key2)
{
  if(key1.k[0] > key2.k[0] )
    return 1;
  else if( key1.k[0] < key2.k[0] )
    return -1;
  else
    return 0;
}

INLINE Key_t
KeyNot(Key_t key1)
{
    key1.k[0] = ~key1.k[0];
    return key1;
}

INLINE Key_t
KeyRshift(Key_t u, int b)
{
    Key_t ret;
    if (b >= CHAR_BIT*sizeof(u.k[0])) ret.k[0] = 0;
    else ret.k[0] = u.k[0] >> b;
    return(ret);
}

INLINE Key_t
KeyLshift(Key_t u, int b)
{
    Key_t ret;
    ret.k[0] = u.k[0] << b;
    return(ret);
}

/* cast int to Key_t */
INLINE Key_t
KeyInt(int i)
{
    Key_t ret;
    ret.k[0] = i;
    return(ret);
}

INLINE Key_t
KeyOrInt(Key_t u, unsigned int i)
{
    Key_t ret;
    ret.k[0] = u.k[0] | i;
    return(ret);
}

/* bitwise and key with int */
/* This is a common operation, and is more efficient than converting the */
/* int to a key.  It returns an int! */

INLINE unsigned int
KeyAndInt(Key_t u, unsigned int i)
{
    return(u.k[0] & i);
}

/* bitwise and key with ~int */
/* This is also  common operation, and is more efficient than converting the */
/* int to a key.  It returns an Key_t (leaving the top bits alone)! */

INLINE Key_t
KeyAndNotInt(Key_t u, unsigned int i)
{
    u.k[0] &= ~((_KTYPE)i);
    return u;
}

INLINE Key_t
KeyAdd(Key_t key1, Key_t key2)
{
    Key_t ret;
    
    ret.k[0] = key1.k[0] + key2.k[0];
    return(ret);
}

INLINE Key_t
KeySub(Key_t key1, Key_t key2)
{
    Key_t ret;
    
    ret.k[0] = key1.k[0] - key2.k[0];
    return(ret);
}

INLINE Key_t
KeyAddInt(Key_t key1, int i)
{
    Key_t ret;

    ret.k[0] = key1.k[0] + i;
    return(ret);
}

#else
#if NK==2

INLINE int
KeyGT(Key_t key1, Key_t key2)
{
    if( key1.k[1] > key2.k[1] )
      return 1;
    else if( key1.k[1] < key2.k[1] )
      return 0;
    else
      return (key1.k[0] > key2.k[0]);
}

INLINE int
KeyLT(Key_t key1, Key_t key2)
{
    if( key1.k[1] < key2.k[1] )
      return 1;
    else if( key1.k[1] > key2.k[1] )
      return 0;
    else
      return (key1.k[0] < key2.k[0]);
}

INLINE int
KeyGE(Key_t key1, Key_t key2)
{
    if( key1.k[1] > key2.k[1] )
      return 1;
    else if( key1.k[1] < key2.k[1] )
      return 0;
    else
      return (key1.k[0] >= key2.k[0]);
}

INLINE int
KeyLE(Key_t key1, Key_t key2)
{
    if( key1.k[1] < key2.k[1] )
      return 1;
    else if( key1.k[1] > key2.k[1] )
      return 0;
    else
      return (key1.k[0] <= key2.k[0]);
}

INLINE int
KeyEQ(Key_t key1, Key_t key2)
{
    return (key1.k[0] == key2.k[0] && key1.k[1] == key2.k[1]);
}

INLINE int
KeyNEQ(Key_t key1, Key_t key2)
{
    return (key1.k[0] != key2.k[0] || key1.k[1] != key2.k[1]);
}

INLINE Key_t
KeyXOR(Key_t key1, Key_t key2)
{
    Key_t ret;
    ret.k[0] = key1.k[0] ^ key2.k[0];
    ret.k[1] = key1.k[1] ^ key2.k[1];
    return(ret);
}

INLINE Key_t
KeyAnd(Key_t key1, Key_t key2)
{
    Key_t ret;
    ret.k[0] = key1.k[0] & key2.k[0];
    ret.k[1] = key1.k[1] & key2.k[1];
    return(ret);
}

INLINE Key_t
KeyOr(Key_t key1, Key_t key2)
{
    Key_t ret;
    ret.k[0] = key1.k[0] | key2.k[0];
    ret.k[1] = key1.k[1] | key2.k[1];
    return(ret);
}

INLINE int
KeyCmp(Key_t key1, Key_t key2)
{
  if(key1.k[1] > key2.k[1] )
    return 1;
  else if( key1.k[1] < key2.k[1] )
    return -1;

  if(key1.k[0] > key2.k[0] )
    return 1;
  else if( key1.k[0] < key2.k[0] )
    return -1;
  return 0;
}

INLINE Key_t
KeyNot(Key_t key1)
{
    key1.k[0] = ~key1.k[0];
    key1.k[1] = ~key1.k[1];
    return key1;
}

INLINE Key_t
KeyRshift(Key_t u, int b)
{
    Key_t ret;
    long bm = CHAR_BIT*sizeof(u.k[0])-b;
    
    if (b == 0) {
	ret = u;
    } else if (bm > 0) {
	ret.k[0] = (u.k[0] >> b) | (u.k[1] << bm);
	ret.k[1] = u.k[1] >> b;
    } else {
	ret.k[0] = u.k[1] >> -bm;
	ret.k[1] = 0;
    }
    return(ret);
}

INLINE Key_t
KeyLshift(Key_t u, int b)
{
    Key_t ret;
    long bm = CHAR_BIT*sizeof(u.k[0])-b;
    
    if (b == 0) {
	ret = u;
    } else if (bm > 0) {
	ret.k[1] = (u.k[1] << b) | (u.k[0] >> bm);
	ret.k[0] = u.k[0] << b;
    } else {
	ret.k[1] = u.k[0] << -bm;
	ret.k[0] = 0;
    }
    return(ret);
}

/* cast int to Key_t */
INLINE Key_t
KeyInt(int i)
{
    Key_t ret;
    ret.k[1] = 0;
    ret.k[0] = i;
    return(ret);
}

INLINE Key_t
KeyOrInt(Key_t u, unsigned int i)
{
    Key_t ret;
    ret.k[0] = u.k[0] | i;
    ret.k[1] = u.k[1];
    return(ret);
}

/* bitwise and key with int */
/* This is a common operation, and is more efficient than converting the */
/* int to a key.  It returns an int! */

INLINE unsigned int
KeyAndInt(Key_t u, unsigned int i)
{
    return(u.k[0] & i);
}

/* bitwise and key with ~int */
/* This is also  common operation, and is more efficient than converting the */
/* int to a key.  It returns an Key_t (leaving the top bits alone)! */

INLINE Key_t
KeyAndNotInt(Key_t u, unsigned int i)
{
    u.k[0] &= ~((_KTYPE)i);
    return u;
}

INLINE Key_t
KeyAdd(Key_t key1, Key_t key2)
{
    Key_t ret;
    
    ret.k[0] = key1.k[0] + key2.k[0];
    ret.k[1] = key1.k[1] + key2.k[1];
    /* We assume keys are unsigned quantities */
    if (ret.k[0] < key1.k[0] || ret.k[0] < key2.k[0]) /* carry */
      ret.k[1]++;
    return(ret);
}

INLINE Key_t
KeySub(Key_t key1, Key_t key2)
{
    Key_t ret;
    
    ret.k[0] = key1.k[0] - key2.k[0];
    ret.k[1] = key1.k[1] - key2.k[1];
    /* We assume keys are unsigned quantities */
    if (ret.k[0] > key1.k[0] || ret.k[0] > key2.k[0]) /* borrow */
      ret.k[1]--;
    return(ret);
}

INLINE Key_t
KeyAddInt(Key_t key1, int i)
{
    Key_t ret;

    ret.k[0] = key1.k[0] + i;
    ret.k[1] = key1.k[1];
    if (i >= 0 && ret.k[0] < key1.k[0])	/* carry */
      ret.k[1]++;
    else if (i < 0 && ret.k[0] > key1.k[0]) /* borrow */
      ret.k[1]--;
    return(ret);
}

#else
 # error NK must be 1 or 2
#endif /* NK==2 */
#endif /* NK==1 */

INLINE int
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
INLINE int
CommonLev(Key_t bkey1, Key_t bkey2, int ndim)
{
    int level = (KEYBITS-1)/ndim;
    Key_t key0 = KeyInt(0);

    for (bkey1 = KeyXOR(bkey1, bkey2); KeyNEQ(bkey1, key0); level--)
      bkey1 = KeyRshift(bkey1, ndim);
    
    return(level);
}

INLINE int
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


#endif /* __GNUC__ || key.c */
#endif /* _KeyDOTh */
