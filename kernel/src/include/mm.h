#ifndef _MM_H_
#define _MM_H_

#include "config.h"
#include "types.h"
#include "list.h"

extern size_t PFN_OFFSET ;   // Always 0 on PC platforms.
extern size_t PFN_MIN;      //first pfn, usually 0,
extern size_t PFN_MAX;        //last pfn in system.
extern size_t PFN_DMA16_MIN;
extern size_t PFN_DMA16_MAX;
extern size_t PFN_DMA32_MIN;
extern size_t PFN_DMA32_MAX;
extern size_t PFN_NORMAL_MIN;     //min_low_pfn in linux, marks the first pfn of normal memory
extern size_t PFN_NORMAL_MAX;

#define MMLOG(...)    log_msg(__VA_ARGS__)
//#define MMLOG(...)
//#define MMLOG2(...)    log_msg(__VA_ARGS__)
#define MMLOG2(...)
//#define MMLOG3(...)    log_msg(__VA_ARGS__)
#define MMLOG3(...)


#ifdef NOMMU
  #define P2V(p) ((adr_t)(p))
  #define V2P(v) ((adr_t)(v))
#else
  #define P2V(p) ((adr_t)(p) + (KERNEL_UPPER_HALF_OFFSET))
  #define V2P(v) ((adr_t)(v) - (KERNEL_UPPER_HALF_OFFSET))
#endif

#define PAGESIZE   (1UL << PAGESHIFT)
#define PAGEMASK   (~(PAGESIZE-1))


/*
 * Round UP & DOWN in power of 2
 */
static inline adr_t PAGEALIGN_UP (adr_t base)
{
    return (base+PAGESIZE-1) & PAGEMASK;
}
static inline adr_t PAGEALIGN_DOWN (adr_t base)
{
    return (base) & PAGEMASK;
}

#define PHYS2PFN(v)     (((v)>>PAGESHIFT) - PFN_OFFSET)

#define PFN2PHYS(p)     ( (p+PFN_OFFSET)<<PAGESHIFT )
#define PFN2VIRT(p)     ( P2V((p+PFN_OFFSET)<<PAGESHIFT) )
#define PAGE2PFN(p)    ( ((adr_t)p - (adr_t)pageframemap) / sizeof(page_t) )
#define PAGE2PHYS(p)    ( (PAGE2PFN(p)+PFN_OFFSET)<<PAGESHIFT )
#define PAGE2VIRT(p)    ( P2V((PAGE2PFN(p)+PFN_OFFSET)<<PAGESHIFT) )


//for Buddy algorithm
//hold a linked list with free areas of a specific order
//Order 0 equals a single page (usually 4096 bytes)
typedef struct free_area_struct {
    struct list_head freelist;
} free_area_t;


#define ZONECOUNT   5
#define ZONENORMAL  0  // < 4gb
#define ZONEDMA16     1
#define ZONEDMA32     2   // not used???
#define ZONEHIGH     3
#define ZONEUNKNOWN    4

//memory flags
#define GFP_DMA16       0x01      //GetFreePage from DMA16 zone
#define GFP_DMA32       0x02      //GetFreePage from DMA32 zone
#define MEMBLOCK_FREE  0x8000

#define MMU_NORMAL  0x00
#define MMU_DEVICE  0x01

#define MMU_PTE_PCD  (1<<4)  //PCD bit,


typedef struct zone_struct {
    //    spinlock_t        lock;
    //    unsigned long     free_pages;
    //    unsigned long     pages_min, pages_low, pages_high;
    //    int               need_balance;

    free_area_t       free_area[(ALLOCBUDDY_MAXORDER+1)];

    //    wait_queue_head_t * wait_table;
    //    unsigned long     wait_table_size;
    //    unsigned long     wait_table_shift;

    //    struct pglist_data *zone_pgdat;
    //    struct page        *zone_mem_map;
    //    unsigned long      zone_start_paddr;
    //    unsigned long      zone_start_mapnr;

    char               *name;
    //    unsigned long       size;
} zone_t;

typedef struct page {
    // these must be first (free area handling)
    struct list_head pagelist;              //list of pages used for freelist, usedlist in page_alloc.c
    int order;                              //0=1 page, 1=2pages, 2=4pages
    uint32_t usecount;                         //Usage count, release when decreased to 0
    zone_t *zone;                           //zone this page belongs to.
    int memsegment;                         // idx to bootmem_memmapentry , cxhange this to pointer later on.
} page_t;

extern page_t *pageframemap;
extern void* kernelpagetable;

extern void page_init(page_t *frame) ;
extern void page_debugprint_free(void);
extern void page_free_page_pfn(int pfn);
extern page_t* page_alloc_page(uint32_t flags);
extern page_t* page_alloc_pages(uint32_t flags, unsigned int order);
extern void* page_alloc_pages_virt(uint32_t flags, unsigned int order);

extern void mmu_map_page(void* pagetable,adr_t paddr,adr_t vaddr,uint64_t flags) ;

extern void* kmalloc(size_t size, uint32_t flags) ;
extern void kfree(void* ptr);
extern void kmalloc_debug_walk(void);

#endif