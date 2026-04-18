#include <string.h>

// Write len copies of val into dest.
//void* memset(void *dest, int val, size_t len)
void *memset(void *dest, int c, size_t n)
{
    unsigned char* temp = (unsigned char*) dest;
    for ( ; n != 0; n--) *temp++ = c;
    return dest;
}

void *memcpy(void *dst, const void * src, size_t n)
{
    const char *s;
    char *d;

    s = src;
    d = dst;
    if(s < d && s + n > d){
        s += n;
        d += n;
        while(n-- > 0)
            *--d = *--s;
    } else
        while(n-- > 0)
            *d++ = *s++;

    return dst;
}


int memcmp(const void *cs,const void *ct,size_t count)
{
    const unsigned char *su1, *su2;
    signed char res = 0;

    for( su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
        if ((res = *su1 - *su2) != 0)
            break;
    return res;
}