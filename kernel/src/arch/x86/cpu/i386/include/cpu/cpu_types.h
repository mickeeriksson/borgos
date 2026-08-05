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




/* Reverse the bytes of a 16-bit unsigned integer */
#define SWAP_UINT16(x) (((uint16_t)(x) >> 8) | ((uint16_t)(x) << 8))

/* Check the CPU architecture endianness */
//#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define be16_to_cpu(x) SWAP_UINT16(x)
#define cpu_to_be16(x) SWAP_UINT16(x)
//#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
//#define be16_to_cpu(x) (uint16_t)(x)
//#define cpu_to_be16(x) (uint16_t)(x)
//#else
//#error "Unknown architecture endianness"
//#endif


#endif