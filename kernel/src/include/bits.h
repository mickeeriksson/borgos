#ifndef _BITS_H_
#define _BITS_H_

#include "types.h"

extern uint64_t bits_64_get(uint64_t value, int firstbit, int lastbit) ;
extern uint64_t bits_64_set(uint64_t regvalue, uint64_t bitvalue,int firstbit,int lastbit);

#endif