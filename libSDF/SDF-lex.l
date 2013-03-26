%{
/*
    SDF Library for reading Self-Describing Files
    Copyright (C) 1991,1992  John K. Salmon

    Terms and conditions are specified in the file "copyright.h",
    and more precisely in the file COPYING.LIB which you should have
    received with this library.
*/
#include <stdlib.h>
/* This define means we don't have to link with the lex library! */
#define YY_SKIP_YYWRAP
#define yywrap() 1

/* This one is ugly.  
   Lex's output contains fprintf(yyout,  of various error messages.  
   We try here to redirect them to a Msg_doalist. */
#include <stdarg.h>
#include "Msgs.h"
#undef fprintf
#define fprintf MYFprintf
void MYFprintf(int *junk, const char *fmt, ...){
    va_list alist;
    va_start(alist, fmt);
    Msg_doalist(fmt, alist);
    va_end(alist);
}

#ifdef FLEX_SCANNER
/* Flex DOES have a much better way to handle non-standard input strategies.
   This doesn't have to be this twisted to work with Flex, but if we are
   going to continue to support lex, it's easiest to just follow along */
#define YY_INPUT(buf, result, maxsize) \
  { \
  int c = SDF_Hdrgetc(); \
  /* Msgf(("YY_INPUT:  c=%c\n", c)); */\
  result = (c==EOF) ? YY_NULL : (buf[0] = c, 1); \
  }
/* Flex also has some new-and-different behavior AFTER YY_INPUT has returned
   NULL.  In particular, all subsequent calls to yylex will immediately
   return NULL unless yyrestart is called.  If we were always using Flex,
   I could attach this to the BEGIN rule, but I can't do that with lex,
   so I have to call it from outside, e.g., in SDFyyprepare. */
void SDFlexprepare(void){
    yyrestart(NULL);
}
#else /* Not FLEX_SCANNER */
void SDFlexprepare(void){}
#endif
%}
White [ \f\t\n]
Dig [0-9]
Alpha [A-Za-z_]
Hex [A-Fa-f0-9]
Octal [0-7]
Alphanum [A-Za-z0-9_]
Expon [Ee]{Sign}{Dig}+
Punct "="|"["|"]"|"{"|"}"|";"|","
Sign [-+]?
%%
#.*SDF-EOH.* {SDFlineno++; return EOHDR;} /* Terminate the header on a comment line */
#.*	{SDFlineno++;} /* Otherwise, skip comments from '#' to eol. */
{White} {if(yytext[0] == '\n') SDFlineno++;} /* skip white space. count lines*/
"/*"	{ int c;
	  loop:
	  while((c=input()) != '*') {if(c=='\n') SDFlineno++;}
	  switch(input()){
		case '/': break;
		case '*': unput('*');
		default: goto loop;
	  }
	} /* Skip normal C comments. (no nesting) */
char	{yylval.type = SDF_CHAR; return TYPE;}
short	{yylval.type = SDF_SHORT; return TYPE;}
int	{yylval.type = SDF_INT; return TYPE;}
long	{yylval.type = SDF_LONG; return TYPE;}
int64_t	{yylval.type = SDF_INT64; return TYPE;}
float	{yylval.type = SDF_FLOAT; return TYPE;}
double	{yylval.type = SDF_DOUBLE; return TYPE;}
struct	{return STRUCT;}
unsigned ; /* WARNING.  The keyword 'unsigned' is skipped entirely. */
parameter	{return PARAMETER;}
byteorder {yylval.valueparam = BYTEORDER; return VALUEPARAM;}
{Alpha}{Alphanum}* {yylval.string = Malloc(yyleng+1);
	strcpy(yylval.string, (char *)yytext);
#if YYDEBUG
	if(yydebug)
	    printf("lex-malloc(%d) at 0x%lx\n", 
		   yyleng+1, (unsigned long)yylval.string);
#endif
	return NAME;}
\"[^"]*\"	{
	/* strings extend to the next double-quote. */
	/* there are no escapes, and no exceptions. */
	/* We fiddle with the yyleng so we only get the contents in yylval. */
	/* Newlines embedded in strings will be missed by SDFlineno! */
	    yylval.constant.u.stringval = Malloc(yyleng-1);
	    strncpy(yylval.constant.u.stringval, (char *)yytext+1, yyleng-2);
	    yylval.constant.u.stringval[yyleng-2] = '\0';
	    yylval.constant.type = SDF_STRING;
#if YYDEBUG
	    if(yydebug)
	        printf("lex-malloc(%d) = 0x%lx\n", yyleng-1, 
		    (unsigned long)yylval.constant.u.stringval);
#endif
	    return CONST;}
{Sign}{Dig}+"."{Dig}*({Expon})? |
{Sign}{Dig}*"."{Dig}+({Expon})? |
{Sign}{Dig}+{Expon} {
	yylval.constant.type = SDF_DOUBLE;
	sscanf((char *)yytext, "%lf", &yylval.constant.u.doubleval); 
	return CONST;}
0x{Hex}+  {
	yylval.constant.type = SDF_INT64;
#if __WORDSIZE==64
        sscanf((char *)yytext+2, "%lx", &yylval.constant.u.int64val);
#else
        sscanf((char *)yytext+2, "%llx", &yylval.constant.u.int64val);
#endif
	return CONST;}
0{Octal}+ {
	yylval.constant.type = SDF_INT64;
#if __WORDSIZE==64
	sscanf((char *)yytext+1, "%lo", &yylval.constant.u.int64val);
#else
	sscanf((char *)yytext+1, "%llo", &yylval.constant.u.int64val);
#endif
	return CONST;}
{Sign}{Dig}+	{
	yylval.constant.type = SDF_INT64;
#if __WORDSIZE==64
	sscanf((char *)yytext, "%ld", &yylval.constant.u.int64val);
#else
	sscanf((char *)yytext, "%lld", &yylval.constant.u.int64val);
#endif
	return CONST;}
{Punct} {return yytext[0];}
. {yyerror("lexer confusion on char: <%c>, line: %d\n", (yytext[0])?yytext[0]:'?',
	SDFlineno); return LEXERROR;}
