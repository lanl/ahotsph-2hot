#ifndef _PQsortDOTh
#define _PQsortDOTh

#include <stddef.h>
#include "timers.h"
#include "key.h"

/* 
What does pqsort return?  It's more than just a sorted
list.  It also "knows" about the choices it made for where to 'split'
the data.  This is returned in the arrays lokey and hikey.  In
particular, lokey[0] and hikey[0] are guaranteed bounds
on the keys that reside in this processor.    lokey[doc] and
hikey[doc] contain the max and min keys in the entire system.
*/ 

typedef struct {
    void *data;
    int nobj;
    int size;
    float median_tol;
    float loadbal_target;
    int proc_order;
    int method;
    int log;
    void *(*realloc_like)(void *, size_t);
    Key_t hikey, lokey;
} sortresult_t;

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
extern Timer_t PQSortTm;
extern Timer_t SortTm;

void pqsortsetup(sortresult_t *decompp, void *bp,
		int nobj, int size, float median_tol,
		void *(*realloc_like)(void *, size_t));

void pqsortsetup_order(sortresult_t *decompp, void *bp,
		int nobj, int size, float median_tol, int proc_order,
		void *(*realloc_like)(void *, size_t));

void *pqsort(sortresult_t *decompp,
	    float (*weight)(const void *), Key_t (*getkey)(const void *));

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* It's really annoying that explicit casts are needed to pass functions */
/* that take void* (i.e., generic pointer) arguments */
typedef float (*pq_wgtproto)(const void *);
typedef Key_t (*pq_keyproto)(const void *);

#endif /* _PQsortDOTh */

