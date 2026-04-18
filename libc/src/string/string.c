#include <string.h>
#include <stdint.h>

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

size_t strlcpy(char *dst, const char *src, size_t size){
    size_t len = 0;
    if(size <= 0)
        return len;
    while(--size > 0 && (*dst++ = *src++) != 0){
        len++;
    }
    *dst = 0;
    len++;
    return len;
}

// Like strncpy but guaranteed to NUL-terminate.
//replaced by strlcpy
//char* safestrcpy(char *s, const char *t, int n)
//{
//    char *os;
//
//    os = s;
//    if(n <= 0)
//        return os;
//    while(--n > 0 && (*s++ = *t++) != 0)
//        ;
//    *s = 0;
//    return os;
//}


int strncmp(const char *p, const char *q, size_t n)
{
    while(n > 0 && *p && *p == *q)
        n--, p++, q++;
    if(n == 0)
        return 0;
    return (uint8_t)*p - (uint8_t)*q;
}


int strcmp(const char* s1, const char* s2)
{
    while(*s1 && (*s1==*s2))
        s1++,s2++;
    return *(const unsigned char*)s1-*(const unsigned char*)s2;
}


#ifndef ISKERNEL

//size_t strlen2(const char* str) {
//    size_t len = 0;
//    while (str[len])
//        len++;
//    return len;
//}


#endif //ISKERNEL