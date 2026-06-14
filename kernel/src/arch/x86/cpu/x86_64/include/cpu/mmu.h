#ifndef __CPU_MMU_H_
#define __CPU_MMU_H_




// Leaf (pte)
typedef struct pagetablentry {
    uint64_t rawflags :9;
    uint64_t avl2 : 3; //avaliable2

    uint64_t base : 36;   //bit 12..47
    uint64_t rsvd_51_48 : 4;
    uint64_t avl3_58_52 : 7;
    uint64_t pk : 4;
    uint64_t xd : 1;
}  __attribute__((packed)) pagetableentry_t;


// use for a subtable lvl4 (pml4),
//  lvl3 (Page Directory Pointer table entry),
//  lvl2 (Page Directory),
//  lvl1 (Page table)
typedef struct subtablentry {
    uint64_t p : 1; //present
    uint64_t rw : 1; //present
    uint64_t us : 1; //present
    uint64_t pwt : 1; //present
    uint64_t pcd : 1; //present
    uint64_t a : 1; //present
    uint64_t avl : 1; //present
    uint64_t ps0 : 1; //present

    uint64_t avl2 : 4; //avaliable2

    uint64_t base : 36;   //bit 12..47
    uint64_t rsvd_51_48 : 4;
    uint64_t ignored_62_52 : 11;
    uint64_t xd : 1;

}  __attribute__((packed)) subtablentry_t;


typedef union pgtblrecord {
    uint64_t raw;
    subtablentry_t pde;
    pagetableentry_t pte;
} pgtblrecord_t;

#endif