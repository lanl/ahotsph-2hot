/*
    SDF Library for reading Self-Describing Files
    Copyright (C) 1991,1992  John K. Salmon

    Terms and conditions are specified in the file "copyright.h",
    and more precisely in the file COPYING.LIB which you should have
    received with this library.
*/
#ifndef sdfDOTh
#define sdfDOTh
#include <stdarg.h>
#include <sys/types.h>
#define SDF_SEEK_SET 0
#define SDF_SEEK_CUR 1
#define SDF_SEEK_END 2

#define SDF_SYNC 0
#define SDF_ASYNC 1

#define SDF_SINGL 0
#define SDF_MULTI 1

#ifndef INT64_MAX
#if __WORDSIZE == 64
#define INT64_MAX LONG_MAX
#else
/* #define INT64_MAX LLONG_MAX Why doesn't this work? */
#define INT64_MAX 9223372036854775807LL
#endif
#endif

#ifndef sdfprivateDOTh
/* It isn't really this, but I don't want to tell you what it is. */
/* If you believe it's this, then your compiler can check prototypes */
/* for you. */
typedef char *SDF[32];

/* Identical to declaration in SDF-private.h */
enum SDF_type_enum {
    SDF_NOTYPE,
    SDF_CHAR,
    SDF_SHORT,
    SDF_INT,
    SDF_LONG,
    SDF_INT64,
    SDF_FLOAT,
    SDF_DOUBLE,
    SDF_STRING
};
#endif

/* Provided for backwards compatibility.  Not recommended! */
#ifdef SDF_OLD_ENUM_NAMES
#define CHAR SDF_CHAR
#define SHORT SDF_SHORT
#define INT SDF_INT
#define FLOAT SDF_FLOAT
#define DOUBLE SDF_DOUBLE
#define STRING SDF_STRING
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* Two arrays indexed by a type_enum */
extern char *SDFtype_names[];
extern int SDFtype_sizes[];

extern char SDFerrstring[];

int SDFissdf(const char *filename); /* Not guaranteed correct! */
SDF *SDFopen(const char *hdrfname, const char *datafname);
int SDFseekable(SDF *hdr); /* are non-sequential reads allowed? */
int SDFclose(SDF *hdr);
int SDFnvecs(SDF *hdr);
int SDFhasname(const char *name, SDF *hdr);
char **SDFvecnames(SDF *hdr);
int64_t SDFnrecs(const char *name, SDF *hdr);
int SDFarrcnt(const char *name, SDF *hdr);
enum SDF_type_enum SDFtype(const char *name, SDF *hdr);
int SDFseek(const char *name, int64_t offset, int whence, SDF *hdr);
int SDFtell(const char *name, SDF *hdr);
unsigned int SDFcpubyteorder(void);
unsigned int SDFbyteorder(SDF *hdr);
int SDFswap(SDF *hdr);
int SDFnoswap(SDF *hdr);
int SDFisswapping(SDF *hdr);
int SDFsetmaxbufsz(int new);
int SDFrdvecs(SDF *hdr, ...
	     /* char *name, int n, void *address, int stride,  
		... , 
		NULL */ );
int SDFrdvecsv(SDF *hdr, va_list ap);
/* Where is the const supposed to go? */
int SDFrdvecsarr(SDF *hdr, int nreq, char **names, int *ns, void **addresses, int *strides);

int SDFseekrdvecs(SDF *hdr, ...
	     /* char *name, int start, int n, void *addr, int stride,  
		... ,
		NULL */ );
int SDFseekrdvecsv(SDF *hdr, va_list ap);
int SDFseekrdvecsarr(
    SDF *hdr, int nreq, char **names, int64_t *starts, int *ns, void **addresses, int *strides);
void SDFsetiomode(int mode);

/* These two subvert the SDF "abstraction" and tell you about */
/* the actual layout of the file. Are you absolutely sure you need */
/* to call these? */
int64_t SDFfileoffset(const char *name, SDF *hdr);
int64_t SDFfilestride(const char *name, SDF *hdr);

/* These four are harder to write than one might guess. */
/* They're in the library to avoid duplicating code. */
int SDFgetint(SDF *sdfp, char *name, int *value);
int SDFgetint64(SDF *sdfp, char *name, int64_t *value);
int SDFgetfloat(SDF *sdfp, char *name, float *value);
int SDFgetdouble(SDF *sdfp, char *name, double *value);
int SDFgetstring(SDF *sdfp, const char *name, char *string, int size);
#ifdef __cplusplus
}
#endif

/* four macros that call SDFget and bail out if the value isn't there */
#define SDFgetintOrDie(sdfp, name, value)              \
    do {                                               \
        if (SDFgetint(sdfp, name, value))              \
            Error("SDFgetint(\"%s\") failed\n", name); \
    } while (0)

#define SDFgetint64OrDie(sdfp, name, value)              \
    do {                                                 \
        if (SDFgetint64(sdfp, name, value))              \
            Error("SDFgetint64(\"%s\") failed\n", name); \
    } while (0)

#define SDFgetfloatOrDie(sdfp, name, value)              \
    do {                                                 \
        if (SDFgetfloat(sdfp, name, value))              \
            Error("SDFgetfloat(\"%s\") failed\n", name); \
    } while (0)

#define SDFgetdoubleOrDie(sdfp, name, value)              \
    do {                                                  \
        if (SDFgetdouble(sdfp, name, value))              \
            Error("SDFgetdouble(\"%s\") failed\n", name); \
    } while (0)

#define SDFgetstringOrDie(sdfp, name, string, size)     \
    do {                                                \
        if (SDFgetstring(sdfp, name, string, size))     \
            Error("SDFgetstring(\"%s\") failed", name); \
    } while (0)

/* And four more that use a default if the value isn't there */
#define SDFgetintOrDefault(sdfp, name, value, def) \
    do {                                           \
        if (SDFgetint(sdfp, name, value)) {        \
            *value = def;                          \
        }                                          \
    } while (0)

#define SDFgetint64OrDefault(sdfp, name, value, def) \
    do {                                             \
        if (SDFgetint64(sdfp, name, value)) {        \
            *value = def;                            \
        }                                            \
    } while (0)

#define SDFgetfloatOrDefault(sdfp, name, value, def) \
    do {                                             \
        if (SDFgetfloat(sdfp, name, value)) {        \
            *value = def;                            \
        }                                            \
    } while (0)


#define SDFgetdoubleOrDefault(sdfp, name, value, def) \
    do {                                              \
        if (SDFgetdouble(sdfp, name, value)) {        \
            *value = def;                             \
        }                                             \
    } while (0)

#define SDFgetstringOrDefault(sdfp, name, value, size, def) \
    do {                                                    \
        if (SDFgetstring(sdfp, name, value, size)) {        \
            strncpy(value, def, size);                      \
        }                                                   \
    } while (0)

#endif /* sdfDOTh */
