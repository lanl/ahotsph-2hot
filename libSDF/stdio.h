/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

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
