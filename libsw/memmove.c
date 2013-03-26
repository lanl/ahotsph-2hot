#include <stddef.h>

void 
*memmove(void *s1, const void *s2, size_t n)
{
#if 0 /* Assume the caller knew this when he decided to use memmove */
    if ((char *)s1 >= (char *)s2 + n || (char *)s1 + n <= (char *)s2){
	return memcpy(s1, s2, n);
    }
#endif
    if(((unsigned long)s1)%sizeof(int)==0 && 
       ((unsigned long)s2)%sizeof(int)==0 && 
       n%sizeof(int)==0){
	/* Everything is alligned.  We can use int assignment. */
	int *ip1 = s1;
	const int *ip2 = s2;
	n /= sizeof(int);
	if( ip1 < ip2 ){
	    while(n--)
		*ip1++ = *ip2++;
	}else{
	    ip2 += n;
	    ip1 += n;
	    while(n--)
		*--ip1 = *--ip2;
	}
    }else{
	char *cp1 = s1;
	const char *cp2 = s2;
	if(cp1 < cp2) {
	    while(n--)
		*cp1++ = *cp2++;
	}else{
	    cp2 += n;
	    cp1 += n;
	    while(n--)
		*--cp1 = *--cp2;
	}
    }
    return s1;
}
