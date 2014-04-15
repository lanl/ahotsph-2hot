/* #define SHOW_WGTS */
/* SetupDecomp() figures out a way to assign every item to a processor */
/* The assignments are available by using DestDecomp() */

#include <stdio.h>
#include <stdlib.h>
#include "Assert.h"
#include "key.h"
#include "decomp.h"
#include "Malloc.h"
#include "mpmy.h"
#include "timers.h"
#include "stk.h"
#include "Msgs.h"
#include "gc.h"
#include "protos.h"
#include "rsort.h"

Timer_t DecompTm;
Timer_t DecompWaitTm;
Timer_t DecompCommTm;

#define MAXNPROC 262144
#define TOPBIT (((KEYBITS-1)/3)*3)

/* Don't dynamically allocate decomptab, since it can fragment the heap */
/* This has probably broken Set/SaveDeccomp */
static Key_t decomptab[MAXNPROC];
static int save_decomp;
static Key_t (*getkey_s)(const void *);
#ifdef SHOW_WGTS
static float *decomp_wgt;
#endif

#define CLEAR 0
#define SAVE 1
#define SET 2

int
MPMY_Doc(void)
{
    int doc = ilog2(MPMY_Nproc());
    if (MPMY_Nproc() != 1 << doc)
      doc++;			/* for non power-of-two sizes */
    return doc;
}

int
MPMY_PowOf2(void)
{
    int doc = ilog2(MPMY_Nproc());
    return (MPMY_Nproc() == 1 << doc);
}

static Key_t
getkn(const void *k)
{
    return *(Key_t *)k;
}

/* This technology needs reworking, with the decomp stored in a sortresult_t */
/* As it is, you can only keep one decomposition, which works for now */

/* Made incompatible interface change from version 16 to 19 */
/* Caller needs to allocate memory for the stored decomptab, and copy data from the returned pointer */
void *
SaveDecomp19(void)
{
    save_decomp = SAVE;
    return decomptab;
}

void
SetDecomp19(void *ptr)
{
    save_decomp = SET;
    if (ptr == 0) return;	/* use existing decomptab */
    memcpy(decomptab, ptr, MPMY_Nproc() * sizeof(Key_t));
}

void
ClearDecomp19(void *ptr)
{
    save_decomp = CLEAR;
}

static void
keyminf(const void *a, const void *b, void *ret)
{
    if (KeyLT(*(Key_t *)a, *(Key_t *)b)) *(Key_t *)ret = *(Key_t *)a;
    else *(Key_t *)ret = *(Key_t *)b;
}

static void
keymaxf(const void *a, const void *b, void *ret)
{
    if (KeyGT(*(Key_t *)a, *(Key_t *)b)) *(Key_t *)ret = *(Key_t *)a;
    else *(Key_t *)ret = *(Key_t *)b;
}

static void
SetupDecompStatic(sortresult_t *decompp, 
		  Key_t (*getkey)(const void *))
{
    unsigned int size = decompp->size;
    unsigned int nobj = decompp->nobj;
    char *data = decompp->data;
    
    Msgf(("SetupDecomp: starting in mode %d\n", save_decomp));    
    if (save_decomp == SET) {
	Msgf(("SetupDecomp: save decomp is set, returning\n"));
	return;
    }
    StartTimer(&DecompTm);

    Key_t keymax = KeyInt(0);
    Key_t keymin = KeyNot(KeyInt(0));
    for (char *b = data; b < data + nobj * size; b += size) {
	Key_t tmp = getkey(b);
	if (KeyGT(tmp, keymax)) keymax = tmp;
	if (KeyLT(tmp, keymin)) keymin = tmp;
    }
    Msgf(("key bounds %s ", PrintKey(keymin)));
    Msgf(("%s\n", PrintKey(keymax)));

    MPMY_Comm_request req;
    MPMY_ICombine_Init(&req);
    MPMY_ICombine_func(&keymin, &keymin, sizeof(Key_t), keyminf, req);
    MPMY_ICombine_func(&keymax, &keymax, sizeof(Key_t), keymaxf, req);
    MPMY_ICombine_Wait(req);

    Msgf(("global key bounds %s ", PrintKey(keymin)));
    Msgf(("%s\n", PrintKey(keymax)));

    int shift = 63;
    keymin = KeyRshift(keymin, shift);
    keymax = KeyAdd(KeyRshift(keymax, shift), KeyInt(1));
    Msgf(("shifted key bounds %s ", PrintKey(keymin)));
    Msgf(("%s\n", PrintKey(keymax)));

    Key_t domain = KeySub(keymax, keymin);
    Msgf(("domain %s\n", PrintKey(domain)));

    /* NK == 2 specific */
    for (int i = 0; i < MPMY_Nproc()-1; i++) {
	decomptab[i] = KeyInt((domain.k[0]/MPMY_Nproc()) * (i + 1));
	decomptab[i] = KeyLshift(KeyAdd(keymin, decomptab[i]), shift);
    }
    Key_t tmp = KeyLshift(KeyInt(1), TOPBIT);
    tmp = KeyOr(tmp, KeySub(tmp, KeyInt(1)));
    decomptab[MPMY_Nproc()-1] = tmp;
    for (int i = 0; i < MPMY_Nproc(); i++) {
	Msgf(("[%5d] %s\n", i, PrintKey(decomptab[i])));
    }
    assert(MPMY_Nproc() <= MAXNPROC);
    StopTimer(&DecompTm);
    Msgf(("SetupDecomp done\n"));
    Msg_do("decomptab[%d] %s %ld\n", 
	   MPMY_Procnum(), PrintKey(decomptab[MPMY_Procnum()]), decomptab[MPMY_Procnum()].k[NK-1]);
}

void
SetupDecomp(sortresult_t *decompp, 
	    float (*weight)(const void *), Key_t (*getkey)(const void *))
{
    getkey_s = getkey;		/* needed by DestDecomp() */

    if (!weight) {
	SetupDecompStatic(decompp, getkey);
	return;
    }

    Msgf(("SetupDecomp: starting in mode %d\n", save_decomp));    
    if (save_decomp == SET) {
	Msgf(("SetupDecomp: save decomp is set, returning\n"));
	return;
    }
    StartTimer(&DecompTm);

    int radixBits = 18;
    int shift = TOPBIT-radixBits;
    unsigned int radix = 1 << radixBits;
    unsigned int mask = radix - 1;
    double *keyden = Calloc(radix, sizeof(double));

    unsigned int size = decompp->size;
    unsigned int nobj = decompp->nobj;
    char *data = decompp->data;
    int counter = 0;
    Msgf(("%s\n", PrintKey(getkey(data))));
    for (char *b = data; b < data + nobj * size; b += size) {
	keyden[KeyAndInt(KeyRshift(getkey(b), shift), mask)] += weight(b);
	if (counter++ % 100000 == 0) Msgf(("%s\n", PrintKey(KeyRshift(getkey(b), shift))));
    }
    StartTimer(&DecompCommTm);
    Native_MPMY_Combine(keyden, keyden, radix, MPMY_DOUBLE, MPMY_SUM);
    StopTimer(&DecompCommTm);

#if 1
    if (MPMY_Procnum() == 0) {
	FILE *fp = fopen("decomp.txt", "w");
	for (int i = 1; i < radix; i++) {
	    fprintf(fp, "%d %g\n", i, keyden[i]);
	}
	fclose(fp);
    }
#endif

    for (int i = 1; i < radix; i++) keyden[i] += keyden[i-1];
    double fac = MPMY_Nproc() / keyden[mask];
    for (int i = 1; i < radix; i <<= 1) {
	Msgf(("%d %g\n", i, fac * keyden[i]));
    }

    int j = 0;
    Key_t base = KeyLshift(KeyInt(1), TOPBIT);
    for (int i = 0; i < MPMY_Nproc()-1; i++) {
	while (fac * keyden[j] < i + 1) j++;
	if ((j & 077) == 077) j++;
	if ((j & 077) == 001) j--;
	decomptab[i] = KeyOr(KeyLshift(KeyInt(j), shift), base);
	Msgf(("[%5d] %s\n", i, PrintKey(decomptab[i])));
    }
    Key_t tmp = KeyLshift(KeyInt(1), TOPBIT);
    tmp = KeyOr(tmp, KeySub(tmp, KeyInt(1)));
    decomptab[MPMY_Nproc()-1] = tmp;
    Msgf(("[%5d] %s\n", MPMY_Nproc()-1, PrintKey(decomptab[MPMY_Nproc()-1])));

    Free(keyden);
    assert(MPMY_Nproc() < MAXNPROC);

    StopTimer(&DecompTm);
    Msgf(("SetupDecomp done\n"));
    Msg_do("decomptab[%d] %s %ld\n", 
	    MPMY_Procnum(), PrintKey(decomptab[MPMY_Procnum()]), decomptab[MPMY_Procnum()].k[NK-1]);
    Msg_flush();
}


void
SetupDecomp1(sortresult_t *decompp, 
	     float (*weight)(const void *), Key_t (*getkey)(const void *))
{
    char *b;
    unsigned int size = decompp->size;
    unsigned int nobj = decompp->nobj;
    char *data = decompp->data;
    Key_t *key_n, tmp;
    double wtfac;
    unsigned int nin;
    Stk ostk;
    double total_wgt;
    struct {
	Key_t key;
	float n;
    } *keydata, *kn;
    unsigned int i;
    
    getkey_s = getkey;

    if (!weight) {
	SetupDecompStatic(decompp, getkey);
	return;
    }

    Msgf(("SetupDecomp: starting in mode %d\n", save_decomp));    
    if (save_decomp == SET) {
	Msgf(("SetupDecomp: save decomp is set, returning\n"));
	return;
    }
    StartTimer(&DecompTm);
    total_wgt = 0.0;
    keydata = Malloc(nobj * sizeof(*keydata));
    i = 0;
    tmp = KeyInt(0);
    for(b = data; b < data + nobj * size; b += size) {
	extern char *PrintBodyContentsLong(void *p);
	double wt = weight(b);
	total_wgt += wt;
	keydata[i].key = getkey(b);
	if (KeyEQ(keydata[i].key, tmp)) {
	    Msg_do("Identical keys %ld %s\n", keydata[i].key.k[0], PrintKey(keydata[i].key));
	} else {
	    tmp = keydata[i].key;
	}
	keydata[i++].n = wt;
    }
    StartTimer(&DecompWaitTm);
    MPMY_Combine(&total_wgt, &total_wgt, 1, MPMY_DOUBLE, MPMY_SUM);
    StopTimer(&DecompWaitTm);
    wtfac = total_wgt/(MPMY_Nproc()*1013.0);
    Msgf(("total weight %lf, wtfac %lf\n", total_wgt, wtfac));
    total_wgt = 0.0;
    StkInitEz(&ostk);
    Msgf(("sorting %d keys in SetupDecomp\n", nobj));
    if (nobj) {
	/* qsort(keydata, nobj, sizeof(*keydata), cmpkey2); */
	rsort(keydata, nobj, sizeof(*keydata), 12, KEYBITS, getkn);
	tmp = keydata->key;
	for(kn = keydata; kn < keydata + nobj-1; kn++) {
	    total_wgt += kn->n;
	    if (total_wgt + (kn+1)->n >= wtfac) {
		StkPushType(&ostk, kn->key, Key_t);
		if (KeyEQ(kn->key, tmp)) {
		    Warning("Identical keys in decomp at %ld %s\n", kn-keydata, PrintKey(kn->key));
		} else {
		    tmp = kn->key;
		}
		total_wgt -= wtfac;
	    }
	}
	if (total_wgt + (keydata+nobj-1)->n >= 0.5*wtfac) {
	    StkPushType(&ostk, (keydata+nobj-1)->key, Key_t);
	}
    }
    Free(keydata);
    Msgf(("Sending %ld bytes\n", StkSz(&ostk)));
    StartTimer(&DecompCommTm);
    nin = MPMY_NGather(StkBase(&ostk), StkSz(&ostk), MPMY_CHAR, 
		       (void **)&key_n, 0);
    StopTimer(&DecompCommTm);
    nin /= sizeof(Key_t);
    StkTerminate(&ostk);

    assert(MPMY_Nproc() < MAXNPROC);
#ifdef SHOW_WGTS
    decomp_wgt = Calloc(MPMY_Nproc(), sizeof(float));
#endif
    if (MPMY_Procnum() == 0) {
	int j;
	double f = (double)nin/MPMY_Nproc();
	int i = 0;
	Msgf(("nin is %d\n", nin));
	/* qsort(key_n, nin, sizeof(Key_t), cmpkey2); */
	rsort(key_n, nin, sizeof(Key_t), 12, KEYBITS, getkn);
	for (j = 0; j < MPMY_Nproc()-1; j++) {
	    i = (j + 1) * f;
	    assert(i < nin);
	    /* mask_decomp_keys */
	    decomptab[j] = KeyAnd(key_n[i], KeyNot(KeyInt((1<<30)-1)));
	    Msgf(("[%5d] %d %s\n", j, i, PrintKey(decomptab[j])));
	}
	tmp = KeyLshift(KeyInt(1), KEYBITS-1);
	tmp = KeyOr(tmp, KeySub(tmp, KeyInt(1)));
	decomptab[MPMY_Nproc()-1] = tmp;
	Msgf(("[%5d] %d %s\n", j, nin, 
	      PrintKey(decomptab[MPMY_Nproc()-1])));
	Free(key_n);
    }
    Msgf(("Doing decomptab Bcast\n"));
    StartTimer(&DecompCommTm);
    MPMY_Bcast(decomptab, MPMY_Nproc()*sizeof(Key_t)/sizeof(int), MPMY_INT, 0);
    StopTimer(&DecompCommTm);
    StopTimer(&DecompTm);
    Msgf(("SetupDecomp done\n"));
    Msg_do("decomptab[%d] %s %ld\n", 
	    MPMY_Procnum(), PrintKey(decomptab[MPMY_Procnum()]), decomptab[MPMY_Procnum()].k[NK-1]);
}

int
DestDecomp(void *b)
{
    const Key_t key = getkey_s(b);
    int nproc = MPMY_Nproc();
    int procnum = MPMY_Procnum();
    int i = (nproc > 3) ? nproc/2-1 : 1;
    int inc = (i > 2) ? i/2 : 1;

    /* Be fast if nothing is going to move */
    if (KeyLE(key, decomptab[procnum]) && (!procnum || KeyGT(key, decomptab[procnum-1])))
      i = procnum;
    else while (1) {
	if (KeyGT(key, decomptab[i])) i += inc;
	else if (i && KeyLE(key, decomptab[i-1])) i -= inc;
	else break;
	if (inc > 1) inc >>= 1;
    }

    if (i < 0 || i >= MPMY_Nproc()) {
	Msg_do("i=%d, inc=%d\n", i, inc);
	Msg_do("decomptab[0] %s\n", PrintKey(decomptab[0]));
	Msg_do("decomptab[nproc-1] %s\n", PrintKey(decomptab[nproc-1]));
	Msg_do("decomptab[procnum] %s\n", PrintKey(decomptab[procnum]));
	Msg_do("decomptab[procnum-1] %s\n", PrintKey(decomptab[procnum-1]));
	Error("Bad DestDecomp i=%d, inc=%d key=%s", i, inc, PrintKey(key));
    }
    assert (i >= 0 && i < MPMY_Nproc());
    assert (KeyLE(key, decomptab[i]) && (!i || KeyGT(key, decomptab[i-1])));
#ifdef SHOW_WGTS
    decomp_wgt[i] += weight(b);
#endif
    return i;
}

void
FinishDecomp(void)
{
#ifdef SHOW_WGTS
    int i;

    MPMY_Combine(decomp_wgt, decomp_wgt, MPMY_Nproc(), MPMY_FLOAT, MPMY_SUM);
    for (i = 0; i < MPMY_Nproc(); i++) {
	singlPrintf("%3d %s %f\n", i, PrintKey(decomptab[i]), decomp_wgt[i]);
    }
    Free(decomp_wgt);
#endif
}
