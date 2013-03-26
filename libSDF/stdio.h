/* This file is a replacement for the few parts of stdio.h */
/* that are assumed by the output of lex and yacc. */
/* It is here because lex's output contains #include "stdio.h"  */
/* and we don't want to be contaminated by the system stdio.h */

#ifndef MYSTDIOdotH
#define MYSTDIOdotH
#include "mpmy_io.h"

/* These are replacements for stdio */
#undef getc
#define getc(fp) SDF_Hdrgetc()
/* Putc is used by 'output'.  This is the easiest way to deal with it */
#undef putc
#define putc(c, fp) (Msg_do("%c", c))
#undef FILE
#define FILE MPMYFile
#undef stdin
#define stdin NULL
#undef stdout
#define stdout NULL
#undef stderr
#define stderr NULL
#undef EOF
#define EOF (-1)
#undef BUFSIZ
#define BUFSIZ 512

/* A couple of prototypes that are in stdio are also needed */
#include <stdarg.h>
int sscanf(const char *, const char *, ...);
int vsprintf(char *, const char *, va_list);
int sprintf(char *, const char *, ...);

#endif
