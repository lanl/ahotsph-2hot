%{
/*
    SDF Library for reading Self-Describing Files
    Copyright (C) 1991,1992  John K. Salmon

    Terms and conditions are specified in the file "copyright.h",
    and more precisely in the file COPYING.LIB which you should have
    received with this library.
*/

/* We don't rely on bison's -p argument any more.  
   Instead, just #define the name changes ourselves.  These are taken
   from the beginning of bison -p output.  These are far from a complete
   set of external 'yy' names, as a quick run throug 'nm' will show.  Maybe
   all the others come from lex.  I dunno.  In any event, the namespace is only
   partially cleaned up.  Perhaps we should apply for Superfund money
   to finish the cleanup?  bison -p does no better.
*/
#define yyparse SDFyyparse
#define yylex SDFyylex
#define yyerror SDFyyerror
#define yylval SDFyylval
#define yychar SDFyychar
#define yydebug SDFyydebug
#define yynerrs SDFyynerrs

#include <stdarg.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>
#include "protos.h"
#include "Msgs.h"
#include "SDF-private.h"
#include "obstack.h"
#include "Malloc.h"

#ifndef __DELTA__
#define YYDEBUG 1
#endif

#if YYDEBUG
/* yacc sends its debug output throught printf.  We change that... */
#define printf Msg_do		/* MUST be after protos.h!!! */
#endif

#ifdef cray
/* This wants to be a long on the cray?? */
extern long int yydebug;
#else
extern int yydebug;
#endif
extern void yyerror(char *fmt, ...);


static enum SDF_type_enum curtype;
static blk_descrip_t cur_blk;
static int cur_file_offset;
static int cur_data_offset;
static SDF  *cur_hdr;
static int no_more_data_blks;
static int zero_len_blknum;

char *SDFtype_names[] = {"notype", "char", "short", "int", "long", "int64_t",
			 "float", "double", "string"};

 int SDFtype_sizes[] = {0, sizeof(char), sizeof(short), sizeof(int), sizeof(long),
			sizeof(int64_t), sizeof(float), sizeof(double), sizeof(char *)};

static int do_value_param(enum value_param_enum type, const_t value);
static int data_dcl(declaration_t dcl);
static int const_dcl(declaration_t dcl, const_list_t consts);
static void finish_curblk(void);
static const_t convert_const(const_t *cp, enum SDF_type_enum type);
static int finish_parse(void);

%}

%token STRUCT
%token <string> NAME
%token <type> TYPE
%token <constant> CONST
%token <valueparam> VALUEPARAM
%token PARAMETER
%token EOHDR LEXERROR
%token ';' '=' '[' ']' '{' '}' ','
%type <declaration> declaration
%type <one_dcl> dcl1
%type <const_list> const_lst comma_sep_consts
%type <dcl_list> comma_sep_dcls typed_dcl_list many_typed_dcl_list
%union{
    enum SDF_type_enum type;
    enum value_param_enum valueparam;
    char *string;
    const_t constant;
    declaration_t declaration;
    dcl_list_t dcl_list;
    one_dcl_t one_dcl;
    const_list_t const_list;
}
%%
hdr : hdr1 {if(finish_parse()) YYERROR;}
    | hdr1 EOHDR {if(finish_parse()) YYERROR; else YYACCEPT;}
    | LEXERROR {YYERROR;}
    ;

hdr1 : stmt
    | hdr1 stmt
    ;

stmt : declaration ';' {if(data_dcl($1)) YYERROR;}
    | declaration '=' const_lst ';' {if(const_dcl($1, $3)) YYERROR;}
    | PARAMETER VALUEPARAM '=' CONST ';' {if(do_value_param($2, $4)) YYERROR;}
    ;

declaration : typed_dcl_list {$$.dcl_list = $1; $$.Nrec = 1;}
    | STRUCT '{' many_typed_dcl_list ';' '}' {$$.dcl_list=$3; $$.Nrec=1;}
    | STRUCT '{' many_typed_dcl_list ';' '}' '[' CONST ']'
	{
	    if( $7.type != SDF_INT64 ){
		yyerror("Expected integer constant");
		YYERROR;
	    }else{
		$$.dcl_list = $3; $$.Nrec = $7.u.int64val;
	    }
	}
    | STRUCT '{' many_typed_dcl_list ';' '}' '[' ']'
	{ $$.dcl_list = $3; $$.Nrec = 0;}
    ;

many_typed_dcl_list : typed_dcl_list {$$ = $1;}
    | many_typed_dcl_list ';' typed_dcl_list
	{
	    int sz;

	    $$.ndcl = $1.ndcl + $3.ndcl;
	    $$.obs = $1.obs;
	    sz = obstack_object_size(&$3.obs);
	    (void)obstack_grow(&$$.obs, obstack_finish(&$3.obs), sz);
	    (void)obstack_free(&$3.obs, NULL);
	}
    ;

typed_dcl_list: TYPE {curtype = $1;} comma_sep_dcls {$$ = $3;}
    ;

comma_sep_dcls: dcl1
	{
	    obstack_begin(&$$.obs, 16*sizeof($1));
	    $$.ndcl = 1;
	    (void)obstack_grow(&$$.obs, &$1, sizeof($1));
	}
    | comma_sep_dcls ',' dcl1 
	{
	    $$ = $1;
	    $$.ndcl += 1;
	    (void)obstack_grow(&$$.obs, &$3, sizeof($3));
	}
    ;
    
dcl1 : NAME {$$.name = $1; $$.type = curtype; $$.arrcnt = 1;}
    | NAME '[' CONST ']' 
	{
	    if( $3.type != SDF_INT64 ){
		yyerror("Expected integer constant");
		YYERROR;
	    }else{
		$$.name = $1; $$.type = curtype; $$.arrcnt = $3.u.int64val;
	    }
	}
    | NAME '[' ']' {$$.name=$1; $$.type=curtype; $$.arrcnt = 0;}
    ;

const_lst : CONST 
	{ 
	    $$.nconst = 1; 
	    obstack_begin(&$$.obs, 16*sizeof($1)); 
	    (void)obstack_grow(&$$.obs, &$1, sizeof($1));
	}
    | '{' comma_sep_consts '}' {$$ = $2;}
    ;

comma_sep_consts : CONST
	{ 
	    $$.nconst = 1; 
	    obstack_begin(&$$.obs, 16*sizeof($1)); 
	    (void)obstack_grow(&$$.obs, &$1, sizeof($1));
	}
    | comma_sep_consts ',' CONST
	{
	    $$ = $1;
	    $$.nconst += 1;
	    (void)obstack_grow(&$$.obs, &$3, sizeof($3));
	}
    ;

%%
static int SDFlineno;

static const char *Dataname, *Hdrname;

#ifdef STANDALONE
char SDFerrstring[512];
unsigned int SDFcpubyteorder(void){return 0;}

main(int argc, char **argv)
{
    SDF hdr;

    if(argc > 1){
	yydebug = 1;
    }else{
	yydebug = 0;
    }
    SDFyyprepare(&hdr, "-", "-");
    if(yyparse()){
	printf("Terminated on error. \n");
	exit(1);
    }
    exit(0);
}
#endif

static int SDF_iomode = MPMY_RDONLY | MPMY_MULTI;

void SDFsetiomode(int mode)
{
    if (mode == SDF_SINGL) {
	SDF_iomode = MPMY_RDONLY | MPMY_SINGL;
    } else {
	SDF_iomode = MPMY_RDONLY | MPMY_MULTI;
    }
}
 


void SDFyyerror(char *fmt, ...)
{
    char *p;
    va_list ap;

    va_start(ap, fmt);
    vsprintf(SDFerrstring, fmt, ap);
    p = SDFerrstring + strlen(SDFerrstring);
    sprintf(p, " lineno = %d\n", SDFlineno);
    va_end(ap);
}

int SDFyyprepare(SDF *hdr, const char *hdrname, const char *dataname)
{
    no_more_data_blks = 0;

    cur_file_offset = 0;
    cur_data_offset = 0;

    cur_blk.Nrec = 1;
    cur_blk.inmem = 1;
    cur_blk.begin_offset = 0;
    cur_blk.reclen = 0;

    cur_hdr = hdr;
    cur_hdr->nblks = 0;
    cur_hdr->nvecs = 0;
    /* Assume that MPMY_Fopen does the 'right' thing with "-" */
    if( SDF_Hdropen(hdrname) < 0 ){
	sprintf(SDFerrstring, "SDFopen: could not open %s\n", hdrname);
	return -1;
    }
    Dataname = dataname;
    Hdrname = hdrname;
    SDFlineno = 0;		/* or 1?  It's always +-1 anyway */
    SDFlexprepare();

    obstack_begin(&cur_hdr->blks_obs, 16*sizeof(blk_descrip_t));
    obstack_begin(&cur_hdr->vecs_obs, 32*sizeof(vec_descrip_t));
    obstack_begin(&cur_hdr->data_obs, 2048);
    return 0;
}

static int finish_parse(void)
{
    int i;

    finish_curblk();
    cur_hdr->blks = (blk_descrip_t *)obstack_finish(&cur_hdr->blks_obs);
    cur_hdr->vecs = (vec_descrip_t *)obstack_finish(&cur_hdr->vecs_obs);
    cur_hdr->data = obstack_finish(&cur_hdr->data_obs);
    cur_hdr->vec_names = Malloc(cur_hdr->nvecs * sizeof(char *));
    for(i=0; i<cur_hdr->nvecs; i++){
	cur_hdr->vec_names[i] = cur_hdr->vecs[i].name;
    }

    if( (Dataname == NULL) || (Dataname[0] == '\0')
       || (strcmp(Hdrname, Dataname)==0)){
	cur_hdr->begin_file_offset = SDF_Hdroffset();
	if( cur_hdr->begin_file_offset < 0 ){
	    yyerror("Can't get offset of end of header\n");
	    return -1;
	}
    }else{
	cur_hdr->begin_file_offset = 0;
    }
    SDF_Hdrclose();

    /* cur_hdr->datafp = MPMY_Fopen(Dataname, MPMY_RDONLY); */
    /* If we come up with a better model for IO, call it here..  */
    cur_hdr->datafp = MPMY_Fopen(Dataname, SDF_iomode);

    Msgf(("cur_hdr->datafp = %p\n", cur_hdr->datafp));

    if( cur_hdr->datafp == NULL ){
	sprintf(SDFerrstring, "SDFopen: could not open data file: %s\n", 
		Dataname);
	return -1;
    }

    if(no_more_data_blks){
	blk_descrip_t *zerolenblk;
	off_t bytesleft, recsleft;
	off_t datalen;

	zerolenblk = &cur_hdr->blks[zero_len_blknum];
	if(zerolenblk->Nrec != 0){
	    yyerror("Zero length block has non-zero length!?\n");
	    return -1;
	}
	if( cur_hdr->begin_file_offset < 0 ){
	    yyerror("Can't have zero-len blk in an unseekable file\n");
	    return -1;
	}
	Msgf(("About to call MPMY_Flen\n"));
	if( (datalen = MPMY_Flen(cur_hdr->datafp)) < 0 ){
	    yyerror("Could not get length of data file.\n");
	    return -1;
	}
	bytesleft = datalen 
	    - (zerolenblk->begin_offset + cur_hdr->begin_file_offset); 
	Msgf(("datalen = %ld, butesleft = %ld\n", 
	      (long)datalen, (long)bytesleft));
	if( bytesleft < 0 ){
	    yyerror("File too short.\n");
	    return -1;
	}
	recsleft = bytesleft/zerolenblk->reclen;
	if( recsleft*zerolenblk->reclen != bytesleft ){
	  printf("datalen is %ld, bytesleft is %ld\n", (long)datalen, (long)bytesleft);
	    yyerror("File ends between record boundaries\n");
	    return -1;
	}
	zerolenblk->Nrec = recsleft;
    }
    return 0;
}

static int do_value_param(enum value_param_enum param, const_t value)
{
    switch(param){
    case BYTEORDER:
	if( value.type != SDF_INT64 )
	    return -1;
	cur_hdr->byteorder = value.u.int64val;
	cur_hdr->swapping = (cur_hdr->byteorder != SDFcpubyteorder());
	break;
    }
    return 0;
}
    

static int data_dcl(declaration_t dcl)
{
    dcl_list_t *dcl_list;
    one_dcl_t *base, *dclp;
    int i, offset;
    vec_descrip_t vec_descrip;

#if YYDEBUG
    if(yydebug)
	printf("Declaration of %ld records:\n", dcl.Nrec);
#endif
    if(no_more_data_blks){
	yyerror("You can't have data following an implicit-length dcl.\n");
	return -1;
    }

    /* Test to see if we can append this dcl to the current */
    /* block. */
    if(cur_blk.inmem || cur_blk.Nrec != 1 ||  dcl.Nrec != 1){
	finish_curblk();
	cur_blk.Nrec = dcl.Nrec;
	cur_blk.reclen = 0;
	cur_blk.inmem = 0;
	cur_blk.begin_offset = cur_file_offset;
#if YYDEBUG
	if(yydebug)
	    printf("New block (%d) at offset %d in file\n",
		   cur_hdr->nblks, cur_file_offset);
#endif
    }

    if(dcl.Nrec == 0){
	no_more_data_blks = 1;
	zero_len_blknum = cur_hdr->nblks;
    }
    
    offset = cur_blk.reclen;

    dcl_list = &dcl.dcl_list;
    base = (one_dcl_t *)obstack_base(&dcl_list->obs);
    for(i=0; i<dcl_list->ndcl; i++){
	dclp = &base[i];
	vec_descrip.name = dclp->name;
	vec_descrip.arrcnt = dclp->arrcnt;
	vec_descrip.type = dclp->type;
	vec_descrip.blk_off = offset;
	vec_descrip.blk_num = cur_hdr->nblks;
	vec_descrip.nread = 0;
	offset += SDFtype_sizes[dclp->type] * dclp->arrcnt;
	cur_hdr->nvecs++;
	(void)obstack_grow(&cur_hdr->vecs_obs, 
			   &vec_descrip, sizeof(vec_descrip));
#if YYDEBUG
	if(yydebug){
	    printf("\t %s %s[%d]", SDFtype_names[dclp->type], dclp->name,
		   dclp->arrcnt);
	    printf(" in block %ld at offset %ld\n",
		   vec_descrip.blk_num, vec_descrip.blk_off);
	}
#endif
    }
    (void)obstack_free(&dcl_list->obs, NULL);
    cur_blk.reclen = offset;
    return 0;
}

static void finish_curblk(void)
{
    cur_hdr->nblks++;
    (void)obstack_grow(&cur_hdr->blks_obs, &cur_blk, sizeof(cur_blk));
    if(cur_blk.inmem){
	cur_data_offset += cur_blk.reclen * cur_blk.Nrec;
    }else{
	cur_file_offset += cur_blk.reclen * cur_blk.Nrec;
    }
}

static int const_dcl(declaration_t dcl, const_list_t consts)
{
    dcl_list_t *dcl_list;
    one_dcl_t *dclbase, *dclp;
    const_t *cp, *cbase, converted;
    vec_descrip_t vec_descrip;
    int i, j, k;
    int offset;
    void *to;

    dcl_list = &dcl.dcl_list;
    if(dcl.Nrec == 0){
	dcl.Nrec = consts.nconst;
#if 0 /* Was it really this easy?? */
	yyerror("Cannot deal with implicit length constant dcls.");
	return -1;
#endif
    }

    /* Test to see if we can append this dcl to the current */
    /* block. */
    if(!cur_blk.inmem || cur_blk.Nrec != 1 ||  dcl.Nrec != 1){
	finish_curblk();
	cur_blk.Nrec = dcl.Nrec;
	cur_blk.reclen = 0;
	cur_blk.inmem = 1;
	cur_blk.begin_offset = cur_data_offset;
#if YYDEBUG
	if(yydebug)
	    printf("New block (%d) at offset %d in data\n",
		   cur_hdr->nblks, cur_data_offset);
#endif
    }

    offset = cur_blk.reclen;
    cbase = (const_t *)obstack_base(&consts.obs);
    dclbase = (one_dcl_t *)obstack_base(&dcl_list->obs);

    for(i=0; i<dcl_list->ndcl; i++){
	dclp = &dclbase[i];
	if(dclp->arrcnt == 0){
	    if(dclp->type == SDF_CHAR 
	       && cbase[i].type == SDF_STRING 
	       && dcl.Nrec == 1){
		dclp->arrcnt = strlen(cbase[i].u.stringval)+1;
		/* Round up for padding purposes. */
		dclp->arrcnt = (dclp->arrcnt + 7)& (~0x7);
	    }else if(i == dcl_list->ndcl-1 && dcl.Nrec == 1){
		dclp->arrcnt = consts.nconst - i;
	    }else{
		yyerror("Can't figure out implicit dcl from context.");
		return -1;
	    }
	}

	vec_descrip.name = dclp->name;
	vec_descrip.arrcnt = dclp->arrcnt;
	vec_descrip.type = dclp->type;
	vec_descrip.blk_off = offset;
	vec_descrip.blk_num = cur_hdr->nblks;
	vec_descrip.nread = 0;
	offset += SDFtype_sizes[dclp->type] * dclp->arrcnt;
	cur_hdr->nvecs++;
	(void)obstack_grow(&cur_hdr->vecs_obs, 
		     &vec_descrip, sizeof(vec_descrip));
#if YYDEBUG
	if(yydebug){
	    printf("\t %s %s[%d]", SDFtype_names[dclp->type], dclp->name,
		   dclp->arrcnt);
	    printf(" in block %ld at offset %ld\n",
		   vec_descrip.blk_num, vec_descrip.blk_off);
	}
#endif
    }
    cur_blk.reclen = offset;

    cp = cbase;
    for(i=0; i<dcl.Nrec; i++){
	for(j=0; j<dcl_list->ndcl; j++){
	    dclp = &dclbase[j];
#if YYDEBUG
	    if(yydebug)
		printf("\t %s %s[%d] (%d) = ", 
		       SDFtype_names[dclp->type], dclp->name,
		       dclp->arrcnt, i);
#endif
	    if( dclp->type == SDF_CHAR && cp->type == SDF_STRING){
#if YYDEBUG
		if(yydebug)
		    printf("\"%s\"\n", cp->u.stringval);
#endif
		to = obstack_next_free(&cur_hdr->data_obs);
		(void)obstack_blank(&cur_hdr->data_obs, dclp->arrcnt);
		/* Should we warn 
		   if strlen(cp->u.stringval) > dclp->arrcnt ????
		 It implies that the 'string' won't be null-terminated? */
		(void)strncpy(to, cp->u.stringval, dclp->arrcnt);
#ifdef YYDEBUG
		if(yydebug)
		    printf("Freeing const string 0x%lx\n", 
			   (unsigned long)cp->u.stringval);
#endif
		Free(cp->u.stringval);
		cp++;
		continue;
	    }

	    for(k=0; k<dclp->arrcnt; k++){
		converted = convert_const(cp, dclp->type);
		if(converted.type == SDF_NOTYPE){
		    yyerror("Failed constant conversion.");
		    return -1;
		}
		(void)obstack_grow(&cur_hdr->data_obs, &converted.u, 
			     SDFtype_sizes[converted.type]);

#ifdef YYDEBUG
		if(yydebug){
		    printf("(%s)", SDFtype_names[cp->type]);
		    switch(converted.type){
		    case SDF_CHAR:
			printf("%c", converted.u.charval);
			break;
		    case SDF_SHORT:
			printf("%d", converted.u.shortval);
			break;
		    case SDF_INT:
			printf("%d", converted.u.intval);
			break;
		    case SDF_LONG:
			printf("%ld", converted.u.longval);
			break;
		    case SDF_INT64:
#if __WORDSIZE==64
			printf("%ld", converted.u.int64val);
#else
			printf("%lld", converted.u.int64val);
#endif
			break;
		    case SDF_FLOAT:
			printf("%.7g", converted.u.floatval);
			break;
		    case SDF_DOUBLE:
			printf("%.17g", converted.u.doubleval);
			break;
		    default:
			printf("Unrecognized type: %d\n", converted.type);
			break;
		    }
		    if(k == dclp->arrcnt-1){
			printf("\n");
		    }else{
			printf(", ");
		    }
		}
#endif
		cp++;
	    }
	}
    }
    (void)obstack_free(&dcl_list->obs, NULL);
    (void)obstack_free(&consts.obs, NULL);
    return 0;
}

static const_t convert_const(const_t *cp, enum SDF_type_enum newtype)
/* Return a constant of type NEWTYPE, with the same value as */
/* *CP.  If the conversion does not preserve value, then */
/* return a constant of NOTYPE, with garbage value. */
{
    const_t value;
    double dval = 0.;
    int64_t ival = 0;

    if(cp->type == newtype){
      /* IRIX -32 bug fix */
      memcpy(&value, cp, sizeof(value));
      /* value = *cp; */
      return value;
    }

    if(cp->type == SDF_STRING || newtype == SDF_STRING){
	value.type = SDF_NOTYPE;
	yyerror("Cannot do const conversions with strings.\n");
	return value;
    }

    /* Now rely on the fact that a double can faithfully hold */
    /* any other arithmetic type (except long ints on 64-bit archs). */
    switch(cp->type){
    case SDF_CHAR:
	dval = (double)cp->u.charval;
	break;
    case SDF_SHORT:
	dval = (double)cp->u.shortval;
	break;
    case SDF_INT:
	dval = (double)cp->u.intval;
	ival = cp->u.intval;
	break;
    case SDF_LONG:
	dval = (double)cp->u.longval;
	ival = cp->u.longval;
	break;
    case SDF_INT64:
	dval = (double)cp->u.int64val;
	ival = cp->u.int64val;
	break;
    case SDF_FLOAT:
	dval = cp->u.floatval;
	break;
    case SDF_DOUBLE:
	dval = cp->u.doubleval;
	break;
    default:
	dval = 0.0;
	yyerror("Cannot do const conversions with strings.\n");
    }

    value.type = newtype;
    switch(newtype){
    case SDF_CHAR:
	value.u.charval = (char)dval;
	if( value.u.charval != dval ){
	    yyerror("Can't fit value into char.");
	    value.type = SDF_NOTYPE;
	}
	break;
    case SDF_SHORT:
	value.u.shortval = (short)dval;
	if( value.u.shortval != dval ){
	    yyerror("Can't fit value into short.");
	    value.type = SDF_NOTYPE;
	}
	break;
    case SDF_INT:
	value.u.intval = (int)dval;
	if( value.u.intval != dval ){
	    value.u.intval = ival;
	    if( value.u.intval != ival ){
		yyerror("Can't fit value into int.");
		value.type = SDF_NOTYPE;
	    }
	    break;
	}
	break;
    case SDF_LONG:
	value.u.longval = (long)dval;
	if( value.u.longval != dval ){
	    value.u.longval = ival;
	    if( value.u.longval != ival ){
		yyerror("Can't fit value into long.");
		value.type = SDF_NOTYPE;
	    }
	    break;
	}
	break;
    case SDF_INT64:
	value.u.int64val = (int64_t)dval;
	if( value.u.int64val != dval ){
	    value.u.int64val = ival;
	    if( value.u.int64val != ival ){
		yyerror("Can't fit value into int64_t.");
		value.type = SDF_NOTYPE;
	    }
	    break;
	}
	break;
    case SDF_FLOAT:
	if(dval > FLT_MAX || dval < -FLT_MAX){
	    yyerror("Can't fit value into float.");
	    value.type = SDF_NOTYPE;
	}
	value.u.floatval = dval;
	break;
    case SDF_DOUBLE:
	value.u.doubleval = dval;
	break;
    default:
	yyerror("Impossible case.\n");
	break;
    }
    return value;
}
    
void *SDFobstack_chunk_alloc(size_t n)
{ 
    void *p = Malloc(n);
#if YYDEBUG
    if(yydebug)
	printf("malloc(%ld) = 0x%lx\n", (long)n, (unsigned long)p);
#endif
    return p;
}

void SDFobstack_chunk_free(void *p)
{ 
#if YYDEBUG    
    if(yydebug)
	printf("free(0x%lx)\n", (unsigned long)p);
#endif
    Free(p); 
}

/* This symbol tells a Flex-based scanner not to bother trying to
   call isatty to figure out whether to do char-at-a-time input.  The
   actual behavior is under our explicit control anyway (see SDF-lex.l),
   but linking against fileno() and isatty() can be annoying. */
#define YY_ALWAYS_INTERACTIVE 1
#include "SDF-lex.c"
