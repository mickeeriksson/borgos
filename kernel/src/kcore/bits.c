#include "types.h"



uint32_t bits_32_get(uint32_t value,int firstbit,int lastbit) {
    uint32_t mask = (1ULL << (1+lastbit-firstbit))-1;
    uint32_t ret =  (value >> firstbit) & mask;
    return ret;
}

uint32_t bits_32_set(uint32_t regvalue, uint32_t bitvalue,int firstbit,int lastbit) {
    int numbits = (1+lastbit-firstbit);
    uint32_t mask = ((1ULL << numbits) - 1) ;
    uint32_t shifted_mask = mask << firstbit;
    uint32_t cleared = regvalue & ~shifted_mask;
    uint32_t shifted_new_value = (bitvalue & mask) << firstbit;
    uint32_t ret =  cleared | shifted_new_value;
    return ret;
}

uint64_t bits_64_get(uint64_t value,int firstbit,int lastbit) {
    uint64_t mask = (1ULL << (1+lastbit-firstbit))-1;
    uint64_t ret =  (value >> firstbit) & mask;
    return ret;
}

uint64_t bits_64_set(uint64_t regvalue, uint64_t bitvalue,int firstbit,int lastbit) {
    int numbits = (1+lastbit-firstbit);
    uint64_t mask = ((1ULL << numbits) - 1) ;
    uint64_t shifted_mask = mask << firstbit;
    uint64_t cleared = regvalue & ~shifted_mask;
    uint64_t shifted_new_value = (bitvalue & mask) << firstbit;
    uint64_t ret =  cleared | shifted_new_value;
    return ret;
}