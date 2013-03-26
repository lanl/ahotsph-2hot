#include <string.h>

void bcopy(void *b1, void *b2, int length){
    memmove(b2, b1, length);
}

