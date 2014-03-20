/*
 * Copyright 1993 Michael S. Warren and John K. Salmon.  All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "SDF.h"
#include "protos.h"
#include "Malloc.h"
#include "SDFwrite.h"
#include "SDFread.h"
#include "pixel.h"
#include "vop.h"
#include "Msgs.h"
#include "tree.h"  /* includes timers.h and pqsort.h */
#include "files.h"
#include "getparam.h"
#include "mpmy.h"
#include "randoms.h"
#include "fastflpt.h"
#include "singlio.h"
#include "chn.h"

static Key_t GetKeyFromPixel(const pixel *ptr);
static void write_image(pixel *image, int npix, int nx, int ny, 
			float *rmin, float *rmax,
			int pixmin, int pixmax, int do_log, char *name);

/* Use this to sort by "ident" for output */
static float UnityCost(const void *ptr){
    return 1.0;
}

void
Image(float *x, float *y, float *val, int stride, int nobj, 
      float *rmin, float *rmax, int x_pixels, int y_pixels, 
      int pixmin, int pixmax, int do_log, char *name)
{
    tree_t thetree;
    float keyfactor;
    pixel *imtab;
    int npix;
    Stk s;
    sortresult_t srt;
    int xp[NDIM];
    Key_t key;
    hcellptr pp;

    if (stride & (sizeof(float)-1))
      Error("stride is not multiple of float size\n");
    stride /= sizeof(float);

    Msgf(("x_pixels is %d, x_max %g, x_min %g\n", x_pixels, rmax[0], rmin[0]));
    keyfactor = x_pixels / (rmax[0] - rmin[0]);

    thetree.htab = Calloc(HASH_TABLE_SIZE, sizeof(hcellptr));
    thetree.ndim = 2;
    thetree.hash_mask = HASH_MASK;
    ChnInit(&thetree.hcellchn, sizeof(hcell), 4096, Realloc_f);

    /* If the stack is realloced, all is lost */
    /* We need a hybrid stk/chn type of thing */
    StkInit(&s,(nobj+1)*sizeof(pixel), Realloc_f, 0);

    /* Assign mass to each key/array value */
    singlPrintf("Computing image.\n");
    while (nobj--) {
	pixel *b;
	xp[0] = keyfactor * (*x - rmin[0]);
	xp[1] = keyfactor * (*y - rmin[1]);
	if (xp[0] < x_pixels && xp[0] >= 0 && xp[1] < y_pixels && xp[1] >= 0) {
	    key = KeyInt(xp[1]*x_pixels+xp[0]);
	    if ((pp = Find(&thetree, key))) {
		b = pp->ptr;
		b->value += *val;
		b->key = key;
	    } else {
		b = StkPush(&s, sizeof(pixel));
		Enter(&thetree, key, b, 0);
		b->value = *val;
		b->key = key;
	    }
	}
	x += stride;
	y += stride;
	val += stride;
    }

    imtab = StkBase(&s);
    npix = StkSz(&s)/sizeof(pixel);
    imtab = Realloc(imtab, npix * sizeof(pixel));

    /* Domain decompose 2d image */
    singlPrintf("Decomposing image.\n");
    pqsortsetup_order(&srt, imtab, npix, sizeof(pixel), .01f, 1, Realloc_f);
    pqsort(&srt, (pq_wgtproto)UnityCost, (pq_keyproto)GetKeyFromPixel);
    imtab = srt.data;
    npix = srt.nobj;

    Msgf(("npix is %d\n", npix));
    
    singlPrintf("Writing image.\n");
    write_image(imtab, npix, x_pixels, y_pixels, rmin, rmax, pixmin, pixmax,
		do_log, name);

    Free(imtab);
    Free(thetree.htab);
    ChnTerminate(&thetree.hcellchn);
}

void
ImageSlice(float *x, float *y, float *z, float *val, int stride, int nobj, 
      float *rmin, float *rmax, int x_pixels, int y_pixels, 
      int pixmin, int pixmax, int do_log, char *name)
{
    tree_t thetree;
    float keyfactor;
    pixel *imtab;
    int npix;
    Stk s;
    sortresult_t srt;
    int xp[NDIM];
    Key_t key;
    hcellptr pp;

    if (stride & (sizeof(float)-1))
      Error("stride is not multiple of float size\n");
    stride /= sizeof(float);

    Msgf(("x_pixels is %d, x_max %g, x_min %g\n", x_pixels, rmax[0], rmin[0]));
    keyfactor = x_pixels / (rmax[0] - rmin[0]);

    thetree.htab = Calloc(HASH_TABLE_SIZE, sizeof(hcellptr));
    thetree.ndim = 2;
    thetree.hash_mask = HASH_MASK;
    ChnInit(&thetree.hcellchn, sizeof(hcell), 4096, Realloc_f);

    /* If the stack is realloced, all is lost */
    /* We need a hybrid stk/chn type of thing */
    StkInit(&s,(nobj+1)*sizeof(pixel), Realloc_f, 0);

    /* Assign mass to each key/array value */
    singlPrintf("Computing image.\n");
    while (nobj--) {
	pixel *b;
	if (*z > rmin[2] && * z < rmax[2]) {
	  xp[0] = keyfactor * (*x - rmin[0]);
	  xp[1] = keyfactor * (*y - rmin[1]);
	  if (xp[0] < x_pixels && xp[0] >= 0 && xp[1] < y_pixels && xp[1] >= 0) {
	    key = KeyInt(xp[1]*x_pixels+xp[0]);
	    if ((pp = Find(&thetree, key))) {
	      b = pp->ptr;
	      b->value += *val;
	      b->key = key;
	    } else {
	      b = StkPush(&s, sizeof(pixel));
	      Enter(&thetree, key, b, 0);
	      b->value = *val;
	      b->key = key;
	    }
	  }
	}
	x += stride;
	y += stride;
	z += stride;
	val += stride;
    }

    imtab = StkBase(&s);
    npix = StkSz(&s)/sizeof(pixel);
    imtab = Realloc(imtab, npix * sizeof(pixel));

    /* Domain decompose 2d image */
    singlPrintf("Decomposing image.\n");
    pqsortsetup_order(&srt, imtab, npix, sizeof(pixel), .01f, 1, Realloc_f);
    pqsort(&srt, (pq_wgtproto)UnityCost, (pq_keyproto)GetKeyFromPixel);
    imtab = srt.data;
    npix = srt.nobj;

    Msgf(("npix is %d\n", npix));
    
    singlPrintf("Writing image.\n");
    write_image(imtab, npix, x_pixels, y_pixels, rmin, rmax, pixmin, pixmax,
		do_log, name);

    Free(imtab);
    Free(thetree.htab);
    ChnTerminate(&thetree.hcellchn);
}

void 
setmax(float val, float *vmax, float *vmin, int do_log)
{
    if (do_log) {
	float tmp = log(val);
	if (tmp > *vmax) *vmax = tmp;
	if (tmp < *vmin) *vmin = tmp;
    } else {
	if (val > *vmax) *vmax = val;
	if (val < *vmin) *vmin = val;
    }
}

#define MSG_TAG 1324
#define PixMask ((1<<30)-1)

static void
write_image(pixel *image, int npix, int nx, int ny, float *rmin, float *rmax,
	    int pixmin, int pixmax, int do_log, char *name)
{
    int k;
    pixel *ip;
    float scale;
    outpixel *output_image;
    float vmax = -1e30f;
    float vmin = 1e30f;
    int first_k, last_k;
    value_type *fimage = NULL;
    MPMY_Comm_request req;
    int procnum = MPMY_Procnum();
    int nproc = MPMY_Nproc();
    int n;
    value_type val;

    if (procnum != nproc-1) {
	last_k = KeyAndInt(image[npix-1].key, PixMask);
	MPMY_send(&last_k, sizeof(int), procnum+1, MSG_TAG);
    } else {
	last_k = nx*ny;
    }
    if (procnum) {
	MPMY_recvn(&first_k, sizeof(int), procnum-1, MSG_TAG);
    } else {
	first_k = 0;
    }
    
    Msgf(("First key is %d\n", first_k));
    Msgf(("Last key is %d\n", last_k));
    
    n = last_k - first_k;
    fimage = Calloc(n+1, sizeof(value_type));
    
    for (ip = image; ip < image+npix; ip++) {
	k = KeyAndInt(ip->key, PixMask) - first_k;
	if (k > n || k < 0)
	    Error("Bad k value (%d), n is %d, key is %d, ip is %ld\n", 
		  k, n, KeyAndInt(ip->key, PixMask), ip-image);
	fimage[k] += ip->value;
	setmax(fimage[k], &vmax, &vmin, do_log);
    }
    if (procnum != nproc-1) {
	MPMY_send(&fimage[n], sizeof(value_type), procnum+1, MSG_TAG);
    }
    if (procnum) {
	MPMY_recvn(&val, sizeof(value_type), procnum-1, MSG_TAG);
	fimage[0] += val;
	setmax(fimage[0], &vmax, &vmin, do_log);
    }
    
    MPMY_ICombine_Init(&req);
    MPMY_ICombine(&vmax, &vmax, 1, MPMY_FLOAT, MPMY_MAX, req);
    MPMY_ICombine(&vmin, &vmin, 1, MPMY_FLOAT, MPMY_MIN, req);
    MPMY_ICombine_Wait(req);
    
    output_image = Malloc(n * sizeof(outpixel));
	
    scale = (pixmax-pixmin)/(vmax - vmin);
    if (do_log) {
	for (k = 0; k < n; k++) {
	    output_image[k].value = (fimage[k] == (float)0.0) 
		? 0 : pixmin+(log(fimage[k])-vmin)*scale;
	}
    } else {
	for (k = 0; k < n; k++) {
	    output_image[k].value = (fimage[k] == (float)0.0) 
		? 0 : pixmin+(fimage[k]-vmin)*scale;
	}
    }

    SDFwrite(name, nx*ny, n, output_image, sizeof(outpixel), OUTBODYDESC,
	     "nx", SDF_INT, nx,
	     "ny", SDF_INT, nx,	/* only square for now */
	     "max_pixel", SDF_INT, pixmax, 
	     "min_pixel", SDF_INT, pixmin, 
	     "log_pixel", SDF_INT, do_log, 
	     "max_value", SDF_FLOAT, vmax,
	     "min_value", SDF_FLOAT, vmin,
	     "image_xmin", SDF_FLOAT, rmin[0],
	     "image_xmax", SDF_FLOAT, rmax[0],
	     "image_ymin", SDF_FLOAT, rmin[1],
	     "image_ymax", SDF_FLOAT, rmax[1],
	     "image_zmin", SDF_FLOAT, rmin[2],
	     "image_zmax", SDF_FLOAT, rmax[2],
	     NULL);
    singlPrintf("\nOutput to %s done.\n", name);
    Free(output_image);
    Free(fimage);
}

static Key_t GetKeyFromPixel(const pixel *ptr){
    /* Decomp ignores the last bits of the Key */
    return KeyLshift(ptr->key, 30);
}
