#include "error.h"
#include "config.h"
#include "mm.h"
#include "spinlock.h"

#define MAX_GET_FREE_PAGE_TRIES 4

/*
 * A block header. This is in front of every malloc-block, whether free or not.
 * from linux 1.2
 */
struct memblock_hdr {
    uint32_t flags;
    union {
        uint32_t size;
        struct memblock_hdr *next;
    } bh;
}__attribute__((packed,aligned(16)));
//make it align(16) so that all kmallocs will be paragraph aligned

/*
 * The page descriptor is at the front of every page that malloc has in use.
 */
struct page_hdr {
    struct page_hdr *next;
    struct memblock_hdr *firstfree;
    uint16_t order;
    uint16_t nfree;
}__attribute__((packed,aligned(16)));
//make it align(16) so that all kmallocs will be paragraph aligned

/*
 * A size descriptor describes a specific class of malloc sizes.
 * Each class of sizes has its own freelist.
 * from linux 1.2
 */
struct size_descriptor {
    struct page_hdr *firstfree;
    //struct page_hdr *dmafree; /* DMA-able memory */
    uint32_t size;
    uint32_t nblocks;
    uint32_t nmallocs;
    uint32_t nfrees;            //number of kfree called to this entry
    uint32_t nbytesmalloced;
    uint32_t npages;
    uint32_t gfporder; /* number of pages in the area required */
    uint32_t ntotal;
    uint32_t nfree;
}__attribute__((packed));

//struct spinlock slablock;

// 32-4096
#define SLABSIZECOUNT   8
/*
 * For now it is unsafe to allocate bucket sizes between n & n=16 where n is
 * 4096 * any power of two
 *  * from linux 1.2
 *
 *  remove 16 bytes (size of page_hdr) for 32bit , 20 bytes for 64bit os
 */
struct size_descriptor defaultsizes[] = {
         { NULL, 32,127, 0,0,0,0, 0,0,0 }
        ,{ NULL,  64, 63, 0,0,0,0, 0,0,0 }
        ,{ NULL,  128, 31, 0,0,0,0, 0,0,0 }
        ,{ NULL,  256-(16/16), 16, 0,0,0,0, 0,0,0 }
        ,{ NULL,  512-(16/8),  8, 0,0,0,0, 0,0,0 }
        ,{ NULL, 1024-(16/4),  4, 0,0,0,0, 0,0,0 }
        ,{ NULL, 2048-(16/2),  2, 0,0,0,0, 0,0,0 }
        ,{ NULL, 4096-16,  1, 0,0,0,0, 0,0,0 }
        ,{ NULL,    0,  0, 0,0,0,0, 0,0,0 }

/*
          { NULL, NULL,  32,127, 0,0,0,0, 0,0,0 }
        ,{ NULL, NULL,  64, 63, 0,0,0,0, 0,0,0 }
        ,{ NULL, NULL, 128, 31, 0,0,0,0, 0,0,0 }
        ,{ NULL, NULL, 256-(16/16), 16, 0,0,0,0, 0,0,0 }
        ,{ NULL, NULL, 512-(16/8),  8, 0,0,0,0, 0,0,0 }
        ,{ NULL, NULL,1024-(16/4),  4, 0,0,0,0, 0,0,0 }
        ,{ NULL, NULL,2048-(16/2),  2, 0,0,0,0, 0,0,0 }
        ,{ NULL, NULL,4096-16,  1, 0,0,0,0, 0,0,0 }
//        ,{ NULL, NULL,8192-16,  1, 0,0,0,0, 1,0,0 }
//        ,{ NULL, NULL,16384-16,  1, 0,0,0,0, 2,0,0 }
//        ,{ NULL, NULL,32768-16,  1, 0,0,0,0, 3,0,0 }
//        ,{ NULL, NULL,65536-16,  1, 0,0,0,0, 4 ,0,0}
//        ,{ NULL, NULL,131072-16,  1, 0,0,0,0, 5,0,0 }
        ,{ NULL, NULL,   0,  0, 0,0,0,0, 0,0,0 }

 */

};


struct zone_slab {
    struct size_descriptor sizes[SLABSIZECOUNT+1];
};

struct zone_slab zoneslabs[ZONECOUNT];

#define NBLOCKS(order)          ((uint32_t)sizes[order].nblocks)
#define BLOCKSIZE(order)        ((uint32_t)sizes[order].size)
#define AREASIZE(order)		    ((uint32_t)PAGESIZE<<(sizes[order].gfporder))


SPINLOCK(slablock);

void kmalloc_calc_sizes(void){
    kprintf("Calc sizes in kmallac_slab\n");

    int grosssize=0;
    int pagenetsize=0;
    int blocksize=0;
    int count=0;
    struct size_descriptor *sizes;

    for(int zi=0;zi<ZONECOUNT;zi++){
        log_msg("\nCalc sizes in kmallac_slab for zone=%d\n",zi);
        sizes = zoneslabs[zi].sizes;
        for(int i=0;i<SLABSIZECOUNT;i++){
            grosssize=(1<<(i+5));
            //MMLOG("Calc size for size=%d, grosssize=%d, pageorder=%d \n",i, grosssize,pageorder);
            MMLOG("Calc size for size=%d, grosssize=%d,  \n",i, grosssize);
            pagenetsize = PAGESIZE - sizeof(struct page_hdr);
            if(grosssize>=PAGESIZE) {
                //only one page
                count = 1;
                blocksize = grosssize - sizeof(struct page_hdr);
            }else if(grosssize>=256){
                count = PAGESIZE / grosssize;
                blocksize=pagenetsize / count;
                MMLOG("Adjust blocksize from %#x to %#x\n",blocksize,(blocksize & 0xFFF0));
                blocksize = blocksize & 0xFFF0; //make blocksize 16 byte adjusted

            }else{
                count = pagenetsize / grosssize;
                blocksize=grosssize;
            }
            MMLOG("    size=%d, count=%d\n",blocksize, count);
            sizes[i].size=blocksize;
            sizes[i].nblocks=count;
            //sizes[i].gfporder=pageorder;
        }

    }

}

void kmalloc_init(void) {
    kmalloc_calc_sizes();

    /*
 * Check the static info array. Things will blow up terribly if it's
 * incorrect. This is a late "compile time" check.....
 * from linux 1.2
 */
    log_msg("Size of page_hdr %d \n", sizeof(struct page_hdr));
    //log_msg("Page align mask %x \n", PAGEALIGNMASK);
    log_msg("Page align mask 0x%lx \n", PAGEMASK);

    struct size_descriptor *sizes;

    int order;
    for(int zi=0;zi<ZONECOUNT;zi++){
        sizes = zoneslabs[zi].sizes;
        for (order = 0;BLOCKSIZE(order);order++)
        {
            //log_msg ("Test order %d \n",order);
            if ((NBLOCKS (order)*BLOCKSIZE(order) + sizeof (struct page_hdr)) > AREASIZE(order)) {
                //if ((NBLOCKS (order)*BLOCKSIZE(order) ) > AREASIZE(order)) {
                log_msg("Cannot use %d bytes out of %d in order = %d block mallocs\n",
                         (int) (NBLOCKS (order) * BLOCKSIZE(order) +
                                sizeof (struct page_hdr)),
                         (int) AREASIZE(order),
                         BLOCKSIZE (order));
                PANIC("This only happens if someone messes with kmalloc");
            }
        }

    }

    log_msg("Init Slab(kmalloc) [OK]\n");
}

int get_order (struct size_descriptor *sizes,uint32_t size)
{
    int order;
    /* Add the size of the header */
    size += sizeof (struct memblock_hdr);
    for (order = 0;BLOCKSIZE(order);order++)
        if (size <= BLOCKSIZE (order))
            return order;
    return -1;
}

void* kmalloc(size_t size, uint32_t flags) {
    struct size_descriptor *sizes;
    MMLOG(">>>>kmalloc (KERNEL)  size=%d, flags=%#x\n",size,flags);

    sizes = zoneslabs[ZONENORMAL].sizes;
    if(flags & GFP_DMA16){
        sizes = zoneslabs[ZONEDMA16].sizes;
        MMLOG("kmalloc GFP_DMA16\n");
        //PANIC("kmalloc of DMA16 not enabled yet!");
    }
    if(flags & GFP_DMA32){
        PANIC("kmalloc of DMA32 not enabled yet!");
    }


    int order = get_order (sizes,size);
    if (order < 0) {
        log_msg ("kmalloc of too large a block (%d bytes) flags=0x%x \n",(int) size,flags);
        PANIC("kmalloc of large page");
        return (NULL);
    }


    uint32_t tries = MAX_GET_FREE_PAGE_TRIES;
    //initlock(&slablock, "slab");
    spinlock(&slablock);
    while (tries --)    {
        struct page_hdr *page = sizes[order].firstfree;
        if(page){
            struct memblock_hdr *block = page->firstfree;

            if(block->flags & MEMBLOCK_FREE){
                //free
                MMLOG("found (1) free block at 0x%x  for size %d \n",block,(uint32_t)size);
                //log_msg("kmalloc found free block at 0x%x,flags=0x%x  for size=%d \n",block,block->flags,(uint32_t)size);
                page->firstfree = block->bh.next;
                //log_msg("kmalloc next free is now block at 0x%x \n",page->firstfree);

                page->nfree--;

                //check if page is empty
                if(!page->nfree){
                    sizes[order].firstfree = page->next;
                    page->next = NULL;
                    //page is lost? Where to keep full pages?
                }
                sizes[order].nmallocs++;
                sizes[order].nbytesmalloced += size;
                sizes[order].nfree--;

                block->flags = (block->flags | flags) & ~(MEMBLOCK_FREE)  ;
                block->bh.size = size;
                //release(&slablock,"slab");
                spinunlock(&slablock);
                void *memptr = block+1;
                MMLOG("found (2) free block at 0x%x for size %d returns ptr 0x%x \n",block,(size_t)size,(adr_t)memptr);
                //log_msg("kmalloc found free block at 0x%x for size %d returns ptr 0x%x , flags=0x%x , use=%s \n",block,(uint32_t)size,(uint32_t)memptr,block->flags,use);

                return memptr; /* Pointer arithmetic: increments past header */

            }else{
                //log_msg("page at 0x%x order=%d for size=%d , use=%s\n",page, page->order,size,use );
                //log_msg("block at 0x%x order=%d , flags=0x%x \n",block, block->flags);

                PANIC("page->firstfree is not free! ");
            }

        }

        //get a new page.
        page_t* pptr = page_alloc_page(flags);
        page = (struct page_hdr *) PAGE2VIRT(pptr);
        if (!page) {
            PANIC("Unable to get new page for malloc \n");
        }
        MMLOG("Got new page %x to use for %d byte mallocs.... \n",page,sizes[order].size);
        //log_msg("Got new page %x to use for %d byte mallocs.... \n",page,sizes[order].size);
        sizes[order].npages++;
        sizes[order].ntotal += sizes[order].nblocks;
        sizes[order].nfree += sizes[order].nblocks;


        struct memblock_hdr *p = (void*)  ((uint8_t *)page)+ sizeof(struct page_hdr) ;

        //kprintf("memblock_hdr=0x%x %d \n", p, ((uint8_t*)p-(uint8_t*)page)) ;
        //log_msg("memblock_hdr=0x%x %d \n", p, ((uint8_t*)p-(uint8_t*)page)) ;

        int i;
        for (i=sizes[order].nblocks;i>1; i--,p=p->bh.next)
        {
            //log_msg("set memblock_hdr=0x%x as free  \n",(uint32_t)p ) ;
            p->flags = p->flags | MEMBLOCK_FREE;
            //log_msg("set next memblock_hdr=0x%x as  \n",((uintptr_t)p+ BLOCKSIZE(order)) ) ;
            p->bh.next = (struct memblock_hdr *) ((uintptr_t)p+ BLOCKSIZE(order));
        }
        //log_msg("set last memblock_hdr=0x%x as free  \n",(uint32_t)p ) ;
        /* Last block: */
        p->flags = p->flags | MEMBLOCK_FREE;
        p->bh.next = NULL;

        p = (void*)  ((uint8_t*)page)+ sizeof(struct page_hdr) ;
        page->order = order;
        page->nfree = sizes[order].nblocks;
        page->firstfree = p;

        page->next = sizes[order].firstfree;
        sizes[order].firstfree = page;

    }
    spinunlock(&slablock);
    PANIC("Unable to do malloc \n");
    return NULL;
}

void kfree(void* ptr)
{
    struct size_descriptor *sizes;



    sizes = zoneslabs[ZONENORMAL].sizes;


    //kprintf("free ptr at 0x%x  \n",ptr ) ;
    //log_msg("kfree free ptr at 0x%x  \n",ptr ) ;
    struct memblock_hdr *p = (void *) ((adr_t)ptr)-sizeof(struct memblock_hdr);
    //kprintf("free memblock at 0x%x size = %d \n",(uint32_t)p,p->bh.size ) ;
    //log_msg("kfree free memblock at 0x%x bh.size = %d \n",(uint32_t)p,p->bh.size ) ;

    //DO some sanity checking???
    if(p->flags & MEMBLOCK_FREE){
        PANIC("Try to free a FREE block ?? \n");
    }

    if(p->flags & GFP_DMA16){
        MMLOG("kfree DMA16 at = %#lx \n",ptr ) ;
        sizes = zoneslabs[ZONEDMA16].sizes;
        //PANIC("kmalloc of DMA16 not enabled yet!");
    }
    if(p->flags & GFP_DMA32){
        PANIC("kmalloc free of DMA32 not enabled yet!");
    }


    //get page
    //struct page_hdr *page = (void*) ((uintptr_t)p & PAGEALIGNMASK);
    struct page_hdr *page = (void*) ((adr_t)p & PAGEMASK);

    //kprintf("free block in page 0x%x \n",page ) ;
    //log_msg("kfree free block in page 0x%x \n",page ) ;

    uint16_t order = page->order;

    //DO some more sanity checking on page
    //TODO


    uint32_t size = p->bh.size;

    p->flags = p->flags | MEMBLOCK_FREE;

    //initlock(&slablock, "slab");
    spinlock(&slablock);
    p->bh.next = page->firstfree;
    page->firstfree = p;
    page->nfree++;
    if (page->nfree == 1) {
        //put back page to free list
        if (page->next) {
            //printk ("Page %p already on freelist dazed and confused....\n", page);
        }
        else {
            page->next = sizes[order].firstfree;
            sizes[order].firstfree = page;
        }
    }
    sizes[order].nfrees++;      /* Noncritical (monitoring) admin stuff */
    sizes[order].nbytesmalloced -= size;
    sizes[order].nfree++;

    //release(&slablock,"slab");
    spinunlock(&slablock);
   // printf("free done! \n" ) ;
}

void kmalloc_debug_walk(void){
    struct size_descriptor *sizes;
    for(int zi=0;zi<ZONECOUNT;zi++){
        log_msg("\nkmalloc_debug_walk for zone=%d\n",zi);
        sizes = zoneslabs[zi].sizes;
        for(int order=0;order<SLABSIZECOUNT;order++){
            log_msg(" order=%d, size=%d, nfrees=%d, bytesmalloced=%d, npages=%d , total=%d, free=%d, pageorder=%d\n", order,sizes[order].size, sizes[order].nfrees, sizes[order].nbytesmalloced, sizes[order].npages, sizes[order].ntotal, sizes[order].nfree, sizes[order].gfporder);
        }
    }
}
