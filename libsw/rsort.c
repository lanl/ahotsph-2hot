#include <stdlib.h>
#include <string.h>
#include "Malloc.h"
#include "key.h"

#define pswap(a, b, sz) do {    \
	char _c[sz]; \
	memcpy((void *)&_c, (void *)a, sz); \
	memcpy((void *)a, (void *)b, sz); \
	memcpy((void *)b, (void *)&_c, sz); \
    } while (0)

#define STACKSIZE 16384

/* Only need to continue if there is more than 1 element */
#define push(_shift, _offset, _n) \
    if (_n > 1LL && _shift >= 0) { \
	sp->shift = _shift; \
	sp->offset = _offset; \
	(sp++)->n = _n; \
	if (sp >= stack+STACKSIZE) Error("Stack overflow\n"); \
    }

#define pop(_shift, _offset, _n) do { \
	_shift = (--sp)->shift; \
	_offset = sp->offset; \
	_n = sp->n; \
    } while (0)


/* insertion sort for small lists */
static void
isort(void *k, int n, int sz, Key_t (*getkey)(const void *))
{
    void *p, *q;

    for (p = k+sz; --n >= 1; p += sz) {
        for (q = p; q > k; q -= sz) {
            if (KeyGT(getkey(q), getkey(q-sz)))
                break;
            pswap(q, q-sz, sz);
        }
    }
}

/* American Flag radix sort using aux storage of O(1) and stack of O(logN) */
void
rsort(void *tag, int64_t n, int sz, int radixBits, int sortBits, Key_t (*getkey)(const void *))
{
    int i, shift, c;
    int64_t offset, *keyden, *pile;
    void *ak;
    unsigned int radix = 1 << radixBits;
    unsigned int mask = radix - 1;
    struct {
        int shift;
        int64_t offset;
        int64_t n;
    } stack[STACKSIZE], *sp = stack;

    keyden = Malloc(radix * sizeof(int64_t));
    pile = Malloc(radix * sizeof(int64_t));

    /* start so last mask cycle uses all radixBits */
    push((sortBits-1)/radixBits*radixBits, 0, n);

    while (sp > stack) {
        pop(shift, offset, n);
        if (n < 64) { 
	    isort(tag+offset*sz, n, sz, getkey);
	    continue;
        }

        for (i = 0; i < radix; i++)
            keyden[i] = 0;
        for (ak = tag+offset*sz; ak < tag+(offset+n)*sz; ak += sz)
            ++keyden[KeyAndInt(KeyRshift(getkey(ak), shift), mask)];
        for (pile[0] = keyden[0], i = 1; i < radix; i++)
            pile[i] = pile[i-1]+keyden[i];
        push(shift-radixBits, offset, keyden[0]);
        for (i = 1; i < radix; i++)
            push(shift-radixBits, offset+pile[i-1], keyden[i]);
        for (ak = tag+offset*sz; ak < tag+(offset+n-keyden[mask])*sz; ak += keyden[c]*sz) {
	    char tag_aux[sz];
            memcpy(tag_aux, ak, sz);      /* in-place permutation */
            while (--pile[c = KeyAndInt(KeyRshift(getkey(tag_aux), shift), mask)] > (ak-tag)/sz-offset)
                pswap(tag_aux, tag+(pile[c]+offset)*sz, sz);
            memcpy(ak, tag_aux, sz);
        }
    }
    Free(pile);
    Free(keyden);
}
