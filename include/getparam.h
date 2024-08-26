#ifndef GetparamDOTh
#define GetparamDOTh

#include <string.h>

#include "mpmy.h"

/* These are tags to use when calling bcast.  Do they help? Who knows */
#define BCAST_INT 0x5413
#define BCAST_STR1 0x5613
#define BCAST_STR2 0x5813
#define BCAST_LONG1 0x5a13
#define BCAST_LONG2 0x5c13
#define BCAST_DBL 0x5e13
#define BCAST_FLOAT 0x6013

#define Getiparam(prompt, value)                             \
    {                                                        \
        if (MPMY_Procnum() == 0) {                           \
            fprintf(stderr, "Enter %s (integer): ", prompt); \
            fflush(stderr);                                  \
            scanf("%d", &value);                             \
            fprintf(stderr, "%d\n", value);                  \
        }                                                    \
        MPMY_BcastTag(&value, 1, MPMY_INT, 0, BCAST_INT);    \
    }

#define Getsparam(prompt, value)                             \
    {                                                        \
        int len;                                             \
        if (MPMY_Procnum() == 0) {                           \
            fprintf(stderr, "Enter %s (string): ", prompt);  \
            fflush(stderr);                                  \
            scanf("%s", value);                              \
            fprintf(stderr, "%s\n", value);                  \
            len = strlen(value) + 1;                         \
        }                                                    \
        MPMY_BcastTag(&len, 1, MPMY_INT, 0, BCAST_STR2);     \
        MPMY_BcastTag(value, len, MPMY_CHAR, 0, BCAST_STR2); \
    }

#define Getlparam(prompt, value)                                                      \
    {                                                                                 \
        int len;                                                                      \
        if (MPMY_Procnum() == 0) {                                                    \
            fprintf(stderr, "Enter %s (string): ", prompt);                           \
            fflush(stderr);                                                           \
            /* Keep trying until we get a line with something other than a newline */ \
            while (fgets(value, sizeof(value), stdin) && value[0] == '\n')            \
                ;                                                                     \
            fprintf(stderr, "%s\n", value);                                           \
            len = strlen(value) + 1;                                                  \
        }                                                                             \
        MPMY_BcastTag(&len, 1, MPMY_INT, 0, BCAST_LONG1);                             \
        MPMY_BcastTag(value, len, MPMY_CHAR, 0, BCAST_LONG2);                         \
    }

#define Getdparam(prompt, value)                             \
    {                                                        \
        if (MPMY_Procnum() == 0) {                           \
            fprintf(stderr, "Enter %s (double): ", prompt);  \
            fflush(stderr);                                  \
            scanf("%lf", &value);                            \
            fprintf(stderr, "%.15g\n", value);               \
        }                                                    \
        MPMY_BcastTag(&value, 1, MPMY_DOUBLE, 0, BCAST_DBL); \
    }

#define Getfparam(prompt, value)                              \
    {                                                         \
        if (MPMY_Procnum() == 0) {                            \
            fprintf(stderr, "Enter %s (float): ", prompt);    \
            fflush(stderr);                                   \
            scanf("%f", &value);                              \
            fprintf(stderr, "%g\n", value);                   \
        }                                                     \
        MPMY_BcastTag(&value, 1, MPMY_FLOAT, 0, BCAST_FLOAT); \
    }

#endif /* GetparamDOTh */
