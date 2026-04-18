#ifndef _STRING_H
#define _STRING_H 1

//#include <sys/cdefs.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//int memcmp(const void*, const void*, size_t);
//void* memcpy(void* __restrict, const void* __restrict, size_t);
//void* memmove(void*, const void*, size_t);
//void* memset(void*, int, size_t);
size_t strlen(const char*);
size_t strlcpy(char *dst, const char *src, size_t size);
char* strcpy(char *dst, const char *src);
int strcmp(const char *str1, const char *str2);
void *memset(void *dest, int c, size_t n);
int memcmp(const void *cs,const void *ct,size_t count);
void *memcpy(void *dst, const void * src, size_t n);

//#ifndef ISKERNEL
//size_t strlen2(const char*);
//#endif //ISKERNEL

#ifdef __cplusplus
}
#endif

#endif
