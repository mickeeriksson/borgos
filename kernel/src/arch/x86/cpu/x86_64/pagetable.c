#include "error.h"
#include "log.h"
#include "mm.h"
#include "cpu/mmu.h"
// A lv4 x86_64 pagetable

void* pagetable_alloc() {
    // allocated a new empty table
    void* tbl = page_alloc_pages_virt(0, 0);
    if (tbl==0) {
        PANIC("page_alloc_pages_virt failed\n");
    }
    //clear pagetable with empty entries
    uint64_t* entries = (uint64_t*)tbl;
    for (int i=0;i<512;i++) {
        entries[i]=0;
    }
    return tbl;
}

void* pagetable_init(void) {
    void* lvl4table = pagetable_alloc();

    return lvl4table;

}

static inline adr_t _pagetable_subtblentry_getbase(subtablentry_t* entry)
{
    return P2V( ((adr_t)entry->base)<<12);
}

static inline void _pagetable_subtblentry_setbase(subtablentry_t* entry,adr_t base)
{
    entry->base = (base >> 12) & 0x000FFFFFFFFF; // 36 bit adress
    //log_msg("Base is now 0x%lx\n",entry->base);
}

static inline void _pagetable_subtblentry_setrawflags(subtablentry_t* entry,uint64_t flags)
{
    *((uint64_t*) entry) = *((uint64_t*) entry) & (~0xFFFF000000000FFF);
    *((uint64_t*) entry) = *((uint64_t*) entry) | flags;
}

pgtblrecord_t* pagetable_walk_getsubtable(pgtblrecord_t* table, uint64_t parenttbllvl, uint64_t idx,uint64_t tblcreateflags) {

   if (idx > 512) {
       PANIC("Index > 512 not allowed for pagetable\n");
   }
   subtablentry_t* entry = (subtablentry_t*) &table[idx];
   if (entry->p==0) {
       //not present, allocate
       pgtblrecord_t* tbl = pagetable_alloc();
       //log_msg("Sub table not present, allocate new at 0x%lx\n",tbl);
       _pagetable_subtblentry_setbase(entry,V2P(tbl));
       _pagetable_subtblentry_setrawflags(entry,tblcreateflags);
       //log_msg("  --- Lvl %d pagetable at 0x%lx\n",(parenttbllvl-1),tbl);
       //volatile uint64_t* raw = (uint64_t*) entry;
       //uint64_t value =
       //*raw = ((uint64_t) V2P(tbl)) | tblcreateflags;

       //*((uint64_t*) entry) = ((uint64_t) V2P(tbl)) & tblcreateflags;
       //log_msg("Entry is now 0x%lx\n",*raw);
   }else {
       //log_msg("  parentlvl %d, idx=%d, Sub table present, entry = 0x%lx\n",parenttbllvl,idx,*((uint64_t*) entry));
   }
   return (pgtblrecord_t*) _pagetable_subtblentry_getbase(entry);
}


pgtblrecord_t* pagetable_walk_getlvl1table(pgtblrecord_t* lvl4table, adr_t vaddr,uint64_t tblcreateflags) {

    uint64_t lvl2idx = (vaddr >> (12+9)) & 0x1FF;
    uint64_t lvl3idx = (vaddr >> (12+9+9)) & 0x1FF;
    uint64_t lvl4idx = (vaddr >> (12+9+9+9)) & 0x1FF;

    pgtblrecord_t* lvl3table = pagetable_walk_getsubtable(lvl4table,4,lvl4idx,tblcreateflags);
    pgtblrecord_t* lvl2table = pagetable_walk_getsubtable(lvl3table,3,lvl3idx,tblcreateflags);
    pgtblrecord_t* lvl1table = pagetable_walk_getsubtable(lvl2table,2,lvl2idx,tblcreateflags);

    //log_msg("  lvl4table        at 0x%lx\n",lvl4table);
    //log_msg("  lvl3table idx=%d at 0x%lx\n",lvl4idx,lvl3table);
    //log_msg("  lvl2table idx=%d at 0x%lx\n",lvl3idx,lvl2table);
    //log_msg("  lvl1table idx=%d at 0x%lx\n",lvl2idx,lvl1table);
    //log_msg("size of pgtblrecord_t = %d\n",sizeof(pgtblrecord_t));
    //log_msg("size of pagetableentry_t = %d\n",sizeof(pagetableentry_t));

    return lvl1table;
}

