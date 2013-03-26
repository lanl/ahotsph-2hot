/*
    SDF Library for reading Self-Describing Files
    Copyright (C) 1991,1992  John K. Salmon

    Terms and conditions are specified in the file "copyright.h",
    and more precisely in the file COPYING.LIB which you should have
    received with this library.
*/
#ifndef sdfprivateDOTh
#define sdfprivateDOTh
#include <stddef.h>
#include <stdlib.h>
#include "stdio.h"		/* from this directory!! */
#include "obstack.h"

/* Exact copy of declaration in SDF.h */
enum SDF_type_enum{SDF_NOTYPE, 
		       SDF_CHAR, 
		       SDF_SHORT, 
		       SDF_INT,
		       SDF_LONG,
		       SDF_INT64,
		       SDF_FLOAT, 
		       SDF_DOUBLE, 
		       SDF_STRING};

extern char SDFerrstring[];

enum toggle_param_enum{NOTHING};

enum value_param_enum{BYTEORDER};

/* How the lexical analyzer returns constants. */
typedef struct{
    enum SDF_type_enum type;
    union{
	char charval;
	short shortval;
	int intval;
	long longval;
	int64_t int64val;
	float floatval;
	double doubleval;
	char *stringval;
    } u;
} const_t;

typedef struct{
    int nconst;
    struct obstack obs;
} const_list_t;

typedef struct{
    char *name;
    enum SDF_type_enum type;
    int arrcnt;
} one_dcl_t;

typedef struct{
    int ndcl;
    struct obstack obs;
} dcl_list_t;

typedef struct{
    dcl_list_t dcl_list;
    int64_t Nrec;
} declaration_t;

typedef struct{
    char *name;
    enum SDF_type_enum type;
    int arrcnt;
    int64_t blk_off;
    int64_t blk_num;
    int64_t nread;
} vec_descrip_t;

typedef struct{
    int reclen;
    int inmem;
    int64_t Nrec;
    int64_t begin_offset;
} blk_descrip_t;

typedef struct{
    int nblks;
    struct obstack blks_obs;
    blk_descrip_t *blks;
    int nvecs;
    struct obstack vecs_obs;
    vec_descrip_t *vecs;
    char **vec_names;
    struct obstack data_obs;
    void *data;
    MPMYFile *datafp;
    int64_t begin_file_offset;
    int byteorder;
    int swapping;
    int hashsz;
    vec_descrip_t **hashtbl;
} SDF;

void SDFlexprepare(void);
int SDFyyprepare(SDF *hdr, const char *hdrname, const char *dataname);
void SDFobstack_chunk_free(void *p);
void *SDFobstack_chunk_alloc(size_t n);

int SDF_Hdropen(const char *name);
void SDF_Hdrclose(void);
int SDF_Hdroffset(void);
int SDF_Hdrgetc();

#define obstack_chunk_alloc SDFobstack_chunk_alloc
#define obstack_chunk_free SDFobstack_chunk_free

#include "SDF.h"


#endif /* sdfprivateDOTh */
