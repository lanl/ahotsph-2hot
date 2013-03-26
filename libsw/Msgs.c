/* 
 * This file implements a common mechanism for delivering messages
 * to the user.  It also provides a way to selectively turn 
 * on and off the status messages emanating from a part of the
 * program.  
 * Call msg_on("foo"); to see all messages of the form:
 *  msg("foo", ("printf-format", printf-args));
 * Note how __FILE__ can be used in place of "foo"...
 *
 * This file does not use <stdio.h>.  This is because in the wonderful
 * world of distributed parallel, figuring out whose <stdio.h> you're getting
 * and whether or not the names have been secretly scrambled is a complete
 * nightmare.  The solution is to make the user pass in two function pointers
 * that are supposed to behave like vfprintf and fflush, and a void * which
 * acts like a FILE * (when passed to the two functions).  How the caller
 * arranges these functions is not our problem.
 * The down-side is that picky compilers may complain about not having 
 * prototypes for sscanf.  Too bad...
 */
#include <stdarg.h>
#include <string.h>
#include <stdio.h>		/* only for sscanf! */
#include "mpmy.h"
#include "Malloc.h"
#include "Msgs.h"
#include "protos.h"

#ifndef Static
#define Static static
#endif

/* Don't increase MAXNAMES arbitrarily.  If it grows much bigger */
/* than O(100), a different search algorithm would be appropriate. */
#define MAXNAMES 50

/* A substantially larger MAXFILES is probably a mistake.  You'll run out */
/* of file descriptors soon enough anyway. */
#define MAXFILES 12

/* The sum of the lengths of all the NAMES (including terminal null). */
#define NAMEPOOLLENGTH (MAXNAMES*32)

Static int nnames;
Static struct nt_s{
    const char *name;
    int status;
    struct nt_s *next;
} name_tbl[MAXNAMES], *first;

Static int nfiles;
Static struct ft_s{
    void *fp;
    int (*vfprintf_like)(void *, const char *, va_list);
    int (*fflush_like)(void *);
} file_tbl[MAXFILES];

char name_pool[NAMEPOOLLENGTH] ;
char *poolptr = name_pool;
char *poolend = name_pool + NAMEPOOLLENGTH;

Static int look_name(const char *name);
Static int Msg_restriction(const char *arg);

int _Msg_enabled = 1;

Static int _Msg_flushalways = 0;
Static int called_addfile = 0;

int Msg_addfile(void *fp, 
	     int (*vfprintf_like)(void *, const char *, va_list),
	     int (*fflush_like)(void *)){
    if( nfiles == MAXFILES )
	return -1;

    file_tbl[nfiles].fp = fp;
    file_tbl[nfiles].vfprintf_like = vfprintf_like;
    file_tbl[nfiles].fflush_like = fflush_like;
    nfiles++;
    called_addfile = 1;
    return 0;
}

int Msg_delfile(void *fp){
    int i;

    for(i=0; i<nfiles && file_tbl[i].fp != fp; i++) ;

    if( i==nfiles )
	return -1;
    if( i != --nfiles ){
	file_tbl[i] = file_tbl[nfiles];
    }
    return 0;
}    

int Msg_nopen(void){
  return nfiles;
}

int Msg_on(const char *name)
{
    int i;
    i = look_name(name);
    if(i < 0){
	return i;
    }
    name_tbl[i].status = 1;
    Msg_do("Messages turned on for name %s\n", name);
    return 0;
}

int Msg_off(const char *name)
{
    int i;
    i = look_name(name);
    if(i < 0){
	return i;
    }
    name_tbl[i].status = 0;
    Msg_do("Messages turned off for name %s\n", name);
    return 0;
}
    
int Msg_set_enable(int new)
{
    int ret = _Msg_enabled;
    _Msg_enabled = new;
    return ret;
}

int _Msg_test(const char *name)
{
    int i;
    i = look_name(name);
    if(i < 0)
	return 0;
    else
	return name_tbl[i].status;
}

int Msg_flushalways(int new)
{
    int ret = _Msg_flushalways;
    _Msg_flushalways = new;
    return ret;
}

/* Recursion is NOT your friend! */
static int recursion;

int
Msg_do(const char *fmt, ...)
{
    va_list args;
    int i;
    struct ft_s *ft;
    int save;
    int extra = 0;

    /* First, we try to actually prevent recursion */
    save = _Msg_enabled;
    /* And then we try to avoid catastrophe if it happened anyway. */
    if( recursion++ == 0 ){
	for(i=0; i<nfiles; i++){
	    ft = &file_tbl[i];
	    va_start(args, fmt);
	    (*ft->vfprintf_like)(ft->fp, fmt, args);
	    va_end(args);
	    if( _Msg_flushalways && ft->fflush_like)
		(*ft->fflush_like)(ft->fp);
	}
#ifndef NO_STDIO 
	if( nfiles == 0 && !called_addfile ){
	  va_start(args, fmt);
	  vfprintf(stderr, fmt, args);
	  if( _Msg_flushalways )
	    fflush(stderr);
	  va_end(args);
	  extra = 1;
	}
#endif
    }
    --recursion;
    _Msg_enabled = save;
    return nfiles+extra;
}

int
Msg_doalist(const char *fmt, va_list alist)
{
    int i;
    struct ft_s *ft;
    int save;
    int extra = 0;

    save = _Msg_enabled;
    if( recursion++ == 0 ){
	for(i=0; i<nfiles; i++){
	    ft = &file_tbl[i];
	    (*ft->vfprintf_like)(ft->fp, fmt, alist);
	    if( _Msg_flushalways && ft->fflush_like )
		(*ft->fflush_like)(ft->fp);
	}
#ifndef NO_STDIO 
	if( nfiles == 0 && !called_addfile ){
	  vfprintf(stderr, fmt, alist);
	  if( _Msg_flushalways )
	    fflush(stderr);
	  extra = 1;
	}
#endif
    }
    --recursion;
    _Msg_enabled = save;
    return nfiles+extra;
}

void Msg_turnon(const char *msg_turn_on){
    char *copy;
    char *msg_key;

    /* You can turn off msgs with a "nomsgs" or a null string or an */
    /* empty string. */
    if( msg_turn_on == NULL 
       || msg_turn_on[0] == '\0' 
       || strcmp(msg_turn_on, "nomsgs")==0 ){
	Msg_set_enable(0);
	return;
    }	

    copy = strcpy(Malloc(strlen(msg_turn_on)+1), msg_turn_on);
    /* Look for comma or space separated arguments to Msg_on */
    for(msg_key = strtok(copy, " ,\t\n");
	msg_key;
	msg_key = strtok(NULL, " ,\t\n")){
	char *restriction_begin = strchr(msg_key, ':');
	if( restriction_begin ){
	    if( !Msg_restriction(restriction_begin+1) )
		continue;
	    *restriction_begin='\0';
	}
	Msg_on(msg_key);
	Msg_do("Turning on Msgs for \"%s\"\n", msg_key);
	/* Do we need to add "./" to the key in addition??? */
	if( strncmp( __FILE__, "./", 2) == 0 &&
	   strncmp( msg_key, "./", 2 ) != 0 ){
	    char *dot_slash_key = Malloc(strlen(msg_key)+3);
	    /* sprintf might be easier, but we are trying to avoid */
	    /* using stdio. */
	    /* sprintf(dot_slash_key, "./%s", msg_key) */
	    strcpy(dot_slash_key, "./");
	    strcat(dot_slash_key, msg_key);
	    Msg_on(dot_slash_key);
	    Msg_do("Turning on Msgs for \"%s\"\n", dot_slash_key);
	    Free(dot_slash_key);
	}
    }
    Free(copy);
}

int Msg_flush(void)
{
    int i, ret;
    struct ft_s *ft;
    int save;

    save = _Msg_enabled;
    ret = 0;
    if( recursion++ == 0 ){
	for(i=0; i<nfiles; i++){
	    ft = &file_tbl[i];
	    if( ft->fflush_like )
		ret |= (*ft->fflush_like)(ft->fp);
	}
    }
    --recursion;
    _Msg_enabled = save;
    return ret;
}

Static int look_name(const char *name)
{
    int i;
    struct nt_s *last, *this;
    unsigned int len;

    last = NULL;
    this = first;
    if( name == NULL ){
      Warning("look_name(NULL)\n");
      return -1;
    }
    while( this && this->name && strcmp(name, this->name) != 0 ) {
	last = this;
	this = this->next;
    } 
    if( this ){
	if( last ){
	    last->next = this->next;
	    this->next = first;
	    first = this;
	}
	return this - name_tbl;
    }else{
	/* Create a new entry.  Add it to the front. */
	/* NOTE: we never free any of this! */
	if(nnames == MAXNAMES){
	    return -1;
	}
	len = strlen(name)+1;
	if( poolptr + len >= poolend ){
	    return -1;
	}
	i = nnames++;
	strcpy(poolptr, name);

	name_tbl[i].name = poolptr;
	poolptr += len;
	name_tbl[i].next = first;
	first = &name_tbl[i];
	return i;
    }
}

Static int Msg_restriction(const char *arg){
    int first, last;

    if( strchr(arg, '-') ){
	if( sscanf(arg, "%d-%d", &first, &last) != 2 ){
	    Msg_do("Unparseable msg restriction: \"%s\"\n", arg);
	    return 0;
	}
	return MPMY_Procnum() <= last && MPMY_Procnum()>= first ;
    }else{
	if( sscanf(arg, "%d", &first) != 1 ){
	    Msg_do("Unparseable msg restriction: \"%s\"\n", arg);
	    return 0;
	}
	return MPMY_Procnum() == first;
    }
}

#ifdef STANDALONE
#include <stdio.h>
/* Of course, Sun's stdio.h doesn't declare fflush or vfprintf! */
int vfprintf(FILE *, const char *, va_list);
int fflush(FILE *);

main(int argc, char **argv){
    FILE *aux;

    Msg_addfile(stdout, vfprintf, fflush);

    aux = fopen("Msg.out", "w");
    if( aux == NULL ){
	fprintf(stderr, "Couldn't fopen("Msg.out"), errno=%d\n", errno);
	exit(1);
    }
    Msg_addfile(aux, vfprintf, fflush);
    Msg_on("foo");
    Msg("foo", ("Hello world foo seventeen=%d\n", 17));
    Msg("bar", ("Hello bar"));
    Msg_on("Msgs.c");
    Msg_on("bar");
    Msg("bar", ("Hello bar pi=%g\n", 3.14159));
    Msglno("bar", ("What was that value again??... %g\n,", 3.1415));
    Msgf(("This is a Msgf message.\n"));
    Msg_off("foo");
    Msg_off(__FILE__);
    Msgf(("This is a blocked Msgf message.\n"));
    Msg("foo", ("Not seen.\n"));
    exit(0);
}
#endif

