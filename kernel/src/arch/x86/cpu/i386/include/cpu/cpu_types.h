#ifndef _CPU_x86_TYPES_H_
#define _CPU_x86_TYPES_H_


typedef signed char		    int8_t;
typedef short int		    int16_t;
typedef int			        int32_t;
//typedef long                int32_t;
typedef unsigned char		uint8_t;
typedef unsigned short int	uint16_t;
typedef unsigned int		uint32_t;
//typedef unsigned long		uint32_t;
typedef unsigned long		intptr_t;
typedef unsigned long		uintptr_t;

typedef unsigned long long		uint64_t;

typedef int	                ssize_t;

typedef unsigned long		adr_t;
typedef unsigned long		reg_t;     //a register entry, used mainly so define size of register 32it or 64 bit.


#ifndef size_t
    typedef __SIZE_TYPE__   size_t;
#endif

#endif