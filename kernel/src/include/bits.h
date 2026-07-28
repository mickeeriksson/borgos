#ifndef _BITS_H_
#define _BITS_H_

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif


extern uint32_t bits_32_get(uint32_t value, int firstbit, int lastbit) ;
extern uint32_t bits_32_set(uint32_t value, uint32_t bitvalue, int firstbit, int lastbit) ;

extern uint64_t bits_64_get(uint64_t value, int firstbit, int lastbit) ;
extern uint64_t bits_64_set(uint64_t regvalue, uint64_t bitvalue,int firstbit,int lastbit);

#ifdef __cplusplus
}
#endif


#endif