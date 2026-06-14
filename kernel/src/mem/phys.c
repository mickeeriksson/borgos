#include "error.h"
#include "bootmem.h"
#include "mm.h"

extern adr_t bootimage_start; //This should be a phys address
extern adr_t bootimage_end; //This should be a phys address
extern struct bootmem_info bootmeminfo;

page_t *pageframemap;

size_t PFN_OFFSET = 0;   // Always 0 on PC platforms.
size_t PFN_MIN = 0;
size_t PFN_MAX = 0;
size_t PFN_DMA16_MIN = 0;
size_t PFN_DMA16_MAX = 255;
size_t PFN_DMA32_MIN = 0;
size_t PFN_DMA32_MAX = 0;
size_t PFN_NORMAL_MIN = 0;
size_t PFN_NORMAL_MAX = 0;


zone_t  memoryzone[4];


int  _is_aligned_to_order(int pfn, int order) {
    adr_t mask = (PAGESIZE<<order)-1;
    adr_t phys = PFN2PHYS(pfn);
    adr_t rest = phys & mask;
    //let _x1=1;
    if (rest == 0) {
        return order;
    }
    return 0;
}

int _testordertofree(int pfn,int max_pgd_pfn) {
    int p = pfn;
    int maxordercount = 1<<ALLOCBUDDY_MAXORDER;
    //log_msg("maxordercount=%d  from ALLOCBUDDY_MAXORDER=%d\n",maxordercount,ALLOCBUDDY_MAXORDER);

    page_t *page = &pageframemap[pfn];
    page_t *testpage;
    while (p<(pfn+maxordercount) && (p<=PFN_MAX) && (p<max_pgd_pfn)) {
        testpage = &pageframemap[p];
        if (page->zone != testpage->zone) {
            break;
        }
        if (page->memsegment != testpage->memsegment) {
            break;
        }
        if (testpage->usecount>0) {
            break;
        }
        if (p>PFN_MAX) {
            break;
        }
        if (p>max_pgd_pfn) {
            break;
        }
        p++;
    }
    //log_msg("test p=%d \n",p);

    int possible = p-pfn;
    int retorder = 0;

    if ((possible >= 64) && (_is_aligned_to_order(pfn, 6)>0)){
        retorder= 6;
    }
    else if ((possible >= 32) && (_is_aligned_to_order(pfn, 5)>0)){
        retorder= 5;
    }
    else if ((possible >= 16) && (_is_aligned_to_order(pfn, 4)>0)){
        retorder= 4;
    }
    else if ((possible >= 8) && (_is_aligned_to_order(pfn, 3)>0)){
        retorder= 3;
    }
    else if ((possible >= 4) && (_is_aligned_to_order(pfn, 2)>0)){
        retorder= 2;
    }
    else if ((possible >= 2) && (_is_aligned_to_order(pfn, 1)>0)){
        retorder= 1;
    }else{
        retorder= 0;
    }

    //log_msg("test p=%d possible=%d, retorder=%d\n",p,possible,retorder);

    return retorder;

}

zone_t* phys_zone_for_pfn(int pfn) {
    //zone_t *zone;
    if ( (pfn>=PFN_NORMAL_MIN) && (pfn<=PFN_NORMAL_MAX) ) {
        return &memoryzone[ZONENORMAL];
    }
    if ( (pfn>PFN_NORMAL_MAX) && (pfn<=PFN_MAX) ) {
        return &memoryzone[ZONEHIGH];
    }
    if ( (pfn>=PFN_DMA16_MIN) && (pfn<=PFN_DMA16_MAX) ) {
        return &memoryzone[ZONEDMA16];
    }
    //if ( (pfn>=PFN_NORMAL_MIN) && (pfn<=PFN_NORMAL_MAX) ) {
    return &memoryzone[ZONEUNKNOWN];
    //}

}


void phys_pagemap_init(size_t pfn_offset, size_t pfn_count) {
    size_t pagemapsize = pfn_count * sizeof(page_t);
    adr_t ptr = (adr_t) bootmem_alloc_virt(pagemapsize,PAGESIZE);

    pageframemap = (void*) ptr;

    //init all framnes
    for (size_t p=PFN_MIN;p<=PFN_MAX;p++) {
        page_init(&pageframemap[p]);
        //pageframemap[p].order = 0; //not set
        //pageframemap[p].usecount = 444; //not set
        pageframemap[p].zone = phys_zone_for_pfn(p); //not set
        //pageframemap[p].memsegment = 4242; //not set
    }

    adr_t bootimg_start = bootimage_start;
    adr_t bootimg_end = bootimage_end;

    int kpages = 0;
    size_t pfn_kernel_min = PHYS2PFN(bootimg_start);
    size_t pfn_kernel_max = PHYS2PFN(bootimg_end) ;
    adr_t nextptr = (adr_t) bootmem_alloc_phys(1,1) ;
    size_t pfn_bootmem_min = PHYS2PFN(bootmeminfo.addr) ;
    size_t pfn_bootmem_max = PHYS2PFN(nextptr) ;
    log_msg("pfn_kernel_min=%d, pfn_kernel_max=%d  pfn_bootmem_min=%d, pfn_bootmem_max=%d\n",pfn_kernel_min,pfn_kernel_max,pfn_bootmem_min,pfn_bootmem_max);

    for (size_t p=pfn_kernel_min;p<(pfn_kernel_max+1);p++) {
        pageframemap[p].usecount = 999; //never release
        //et page: &mut PageFrame = pagemap_page(p);
        //page.count=999;
        kpages+=1;
    }
    log_msg("pagemap at=%x\n kpages=%d, pfn_kernel_min=%d, pfn_kernel_max=%d\n",ptr,kpages,pfn_kernel_min,pfn_kernel_max);


    int kernelpages=0;
    int bootmempages=0;
    int freepages=0;
    int unknownpages=0;
    int reservedpages=0;

    //MARK FREE
    struct bootmem_memmapentry *mapentry = bootmem_memmapentry;
    int mapentryidx=0;
    while (mapentry->type != BOOTMEMTYPE_END) {
        adr_t addr = mapentry->addr;
        adr_t end = mapentry->addr+mapentry->size-1;

        if (mapentry->type == BOOTMEMTYPE_FREE)  {
            for (adr_t p=addr;p<end;p+=PAGESIZE) {
                int pfn = PHYS2PFN(p);
                if (pfn<=PFN_MAX) {
                    //log_msg("Mark pageframe %d as free\n",pfnidx);
                    if (pfn>=pfn_kernel_min && pfn<=pfn_kernel_max) {
                        //kernel image
                    }
                    else if (pfn>=pfn_bootmem_min && pfn<=pfn_bootmem_max) {
                        //bootmem allocator
                    }
                    else {
                        pageframemap[pfn].usecount = 0; //free
                        pageframemap[pfn].memsegment = mapentryidx;
                        freepages++;
                    }
                }else {
                    //skip
                }
            }
        }
        mapentryidx++;
        mapentry++;
    }

    //MARK RESERVED
    mapentry = bootmem_memmapentry;
    mapentryidx=0;
    while (mapentry->type != BOOTMEMTYPE_END) {
        adr_t addr = mapentry->addr;
        adr_t end = mapentry->addr+mapentry->size-1;

        if (mapentry->type == BOOTMEMTYPE_FREE)  {
            for (adr_t p=addr;p<end;p+=PAGESIZE) {
                int pfn = PHYS2PFN(p);
                if (pfn<=PFN_MAX) {
                    //log_msg("Mark pageframe %d as free\n",pfnidx);
                    if (pfn>=pfn_kernel_min && pfn<=pfn_kernel_max) {
                        log_msg("K");
                        kernelpages++;
                        pageframemap[pfn].usecount = 999; //never release
                        pageframemap[pfn].memsegment = mapentryidx;
                    }
                    else if (pfn>=pfn_bootmem_min && pfn<=pfn_bootmem_max) {
                        log_msg("A");
                        bootmempages++;
                        pageframemap[pfn].usecount = 998; //never release
                        pageframemap[pfn].memsegment = mapentryidx;
                    }
                    else {
                        //log_msg(".");
                        //pageframemap[pfn].usecount = 0; //free
                        //pageframemap[pfn].memsegment = mapentryidx;
                        //freepages++;
                    }
                }else {
                    //skip
                }
            }
        }else if (mapentry->type == BOOTMEMTYPE_UNKNOWN)  {
            log_msg("?");
            /*
            for (adr_t p=addr;p<end;p+=PAGESIZE) {
                int pfn = PHYS2PFN(p);
                if (pfn<=PFN_MAX) {
                    //log_msg("Mark pageframe %d as unkown\n",pfnidx);
                    pageframemap[pfn].usecount = 777; //never release
                    pageframemap[pfn].memsegment = mapentryidx;
                    unknownpages++;
                }else {
                    //skip
                }
            }*/
        }else if (mapentry->type == BOOTMEMTYPE_RESERVED)  {
            for (adr_t p=addr;p<end;p+=PAGESIZE) {
                int pfn = PHYS2PFN(p);
                if (pfn<=PFN_MAX) {
                    //log_msg("Mark pageframe %d as reserved\n",pfnidx);
                    pageframemap[pfn].usecount = 888; //never release
                    pageframemap[pfn].memsegment = mapentryidx;
                    reservedpages++;
                }else {
                    //skip
                }
            }
        }
        mapentryidx++;
        mapentry++;
    }


    log_msg("\n");
    log_msg("kpages        = %d\n",kpages);
    log_msg("kernelpages   = %d\n",kernelpages);
    log_msg("bootmempages  = %d\n",bootmempages);
    log_msg("freepages     = %d\n",freepages);
    log_msg("reservedpages = %d\n",reservedpages);
    log_msg("unknownpages  = %d\n",unknownpages);
}


void phys_freepages_low(void) {
    //free pages < boot pgtbl, ie below 4gb
    //We cant free pages above 4gb before switch to new pagetable, since they are not mapped by boot.S

    int p = 0;
    int freed_pages =0;
    while (p<=PFN_MAX && p<=MAXBOOTPGD_PFN) {
        page_t *page = &pageframemap[p];
        if ((page->order < 1) && (page->usecount < 1)) {
            int freeorder = _testordertofree(p,MAXBOOTPGD_PFN);  //set PFN_MAX when init high mem
            //log_msg("freeorder=%d\n",freeorder);
            page->order = freeorder;
            page->usecount=1; //decreased by one, when freed.

            page_free_page_pfn(p);

            int skippages = (1<<freeorder)-1;
            freed_pages = freed_pages + skippages +1;

            if (p < 512){
                //first 10 lines
                log_msg("FREE (first) LOWMEM PFN=%d order=%d, next p=%d, memseg=%d\n",p,freeorder,(p+skippages+1),page->memsegment);
            }
            if (p==512){
                log_msg("   ... SKIP debug output\n");
            }
            if ((p > PFN_MAX-128) || (p > MAXBOOTPGD_PFN-128) ){
                //last 10 lines
                log_msg("FREE (last) LOWMEM PFN=%d order=%d, next p=%d, memseg=%d\n",p,freeorder,(p+skippages+1),page->memsegment);
            }
            p=p+skippages;
        }
        p++;
    }
    log_msg("freed_pages = %d\n",freed_pages);
    log_msg("p = %d\n",p);
}

void phys_freepages_high(void) {
    //free pages > boot pgtbl, ie above 4gb

    int p = MAXBOOTPGD_PFN+1;
    int freed_pages =0;
    while (p<=PFN_MAX) {
        page_t *page = &pageframemap[p];
        if ((page->order < 1) && (page->usecount < 1)) {
            int freeorder = _testordertofree(p,PFN_MAX);  //set PFN_MAX when init high mem
            //log_msg("freeorder=%d\n",freeorder);
            page->order = freeorder;
            page->usecount=1; //decreased by one, when freed.

            page_free_page_pfn(p);

            int skippages = (1<<freeorder)-1;
            freed_pages = freed_pages + skippages +1;

            /*
            if (p < 512){
                //first 10 lines
                log_msg("FREE (first) LOWMEM PFN=%d order=%d, next p=%d, memseg=%d\n",p,freeorder,(p+skippages+1),page->memsegment);
            }
            if (p==512){
                log_msg("   ... SKIP debug output\n");
            }*/
            if ((p > PFN_MAX-128) ){
                //last 10 lines
                log_msg("FREE (last) HIGHMEM PFN=%d order=%d, next p=%d, memseg=%d\n",p,freeorder,(p+skippages+1),page->memsegment);
            }
            p=p+skippages;
        }
        p++;
    }
    log_msg("freed_pages (HIGH) = %d\n",freed_pages);
    log_msg("p = %d\n",p);
}

void phys_init_memoryzones(){
    for(int zi=0;zi<ZONECOUNT;zi++){
        switch (zi) {
            case ZONENORMAL:
                memoryzone[zi].name = "NORMAL";
                log_msg("set zone %d at adr=%#lx to name %s\n",zi,&memoryzone[zi],memoryzone[zi].name);
                break;
            case ZONEDMA16:
                memoryzone[zi].name = "DMA16";
                log_msg("set zone %d at adr=%#lx to name %s\n",zi,&memoryzone[zi],memoryzone[zi].name);
                break;
            case ZONEDMA32:
                memoryzone[zi].name = "DMA32";
                log_msg("set zone %d at adr=%#lx to name %s\n",zi,&memoryzone[zi],memoryzone[zi].name);
                break;
            case ZONEHIGH:
                memoryzone[zi].name = "HIGH";
                log_msg("set zone %d at adr=%#lx to name %s\n",zi,&memoryzone[zi],memoryzone[zi].name);
                break;
            default:
                memoryzone[zi].name = "UNKNOWN";
                log_msg("set zone %d at adr=%#lx to name %s\n",zi,&memoryzone[zi],memoryzone[zi].name);
                break;
        }
        for(int i=0;i<=ALLOCBUDDY_MAXORDER;i++){
            //log_msg("Init freelist zone(%s) for order %d = %d pages, (size = %dKb)\n",memoryzone[zi].name,i,(0x01<<i),((0x01<<i)*PAGESIZE)/1024);
            INIT_LIST_HEAD(&(memoryzone[zi].free_area[i].freelist));
        }

    }
}

void phys_init(void) {
    log_msg("Init memoryzones\n");
    phys_init_memoryzones();


    //build contig memory hole where chunk > 1meg
    size_t contig_low  = 0;
    size_t contig_high  = 0;
    size_t top_high  = 0;
    adr_t normaltop = (adr_t)4*1024*1024*1024;

    struct bootmem_memmapentry *ptr = bootmem_memmapentry;
    struct bootmem_memmapentry *largestnormal = NULL;  //largest memsize below 4gb
    struct bootmem_memmapentry *top = NULL;  //highest memory including high >4gb

    //find largest continious memomory > 1mb
    while (ptr->type != BOOTMEMTYPE_END) {
        if ((ptr->type == BOOTMEMTYPE_FREE) && (ptr->size > (1024*1024))) {
            if ((largestnormal == NULL) || ((ptr->size < largestnormal->size) && (largestnormal->addr < normaltop))) {
                largestnormal = ptr;
            }
            if ((top == NULL) || (ptr->addr > top->addr)) {
                top = ptr;
            }
            /*
            contig_low=ptr->addr;
            contig_high=ptr->addr+ptr->size-1;

            contig_low=PAGEALIGN_UP(contig_low);
            contig_high=PAGEALIGN_DOWN(contig_high);

            log_msg("  contig_low : aligned=0x%x   raw=0x%x\n",contig_low,ptr->addr);
            log_msg("  contig_high: aligned=0x%x   raw=0x%x\n",contig_high,(ptr->addr+ptr->size-1));
            break;*/
        }
        ptr++;
    }

    if (largestnormal==NULL) {
        PANIC("UNABLE TO FIND NORMAL MEMORY");
    }
    if (top==NULL) {
        PANIC("UNABLE TO FIND TOP MEMORY");
    }

    contig_low=largestnormal->addr;
    contig_high=largestnormal->addr+largestnormal->size-1;
    top_high=top->addr+top->size-1;

    contig_low=PAGEALIGN_UP(contig_low);
    contig_high=PAGEALIGN_DOWN(contig_high);
    top_high=PAGEALIGN_DOWN(top_high);

    log_msg("  contig_low : aligned=0x%lx   raw=0x%lx\n",contig_low,largestnormal->addr);
    log_msg("  contig_high: aligned=0x%lx   raw=0x%lx\n",contig_high,(largestnormal->addr+largestnormal->size-1));
    log_msg("  top_high   : aligned=0x%lx   raw=0x%lx\n",top_high,(top->addr+top->size-1));




    size_t pfn_offset = 0 ;
    size_t pfn_count = 0 ;

    //PFN_MAX = (largestnormal->addr+largestnormal->size-1) / PAGESIZE;
    PFN_MAX = top_high / PAGESIZE;
    PFN_NORMAL_MIN = contig_low / PAGESIZE;
    PFN_NORMAL_MAX = contig_high / PAGESIZE;
    log_msg("PFN_MIN=%d\nPFN_MAX=%d\nPFN_NORMAL_MIN=%d\nPFN_NORMAL_MAX=%d\n",PFN_MIN,PFN_MAX,PFN_NORMAL_MIN,PFN_NORMAL_MAX);

    pfn_offset = PFN_OFFSET;
    pfn_count =  (PFN_MAX+1 - PFN_MIN) - PFN_OFFSET;

    log_msg("pfn_offset = %d, pfn_count = %d\n",pfn_offset,pfn_count);
    phys_pagemap_init( pfn_offset, pfn_count);

    //page_debugprint_free();
    //build the buddy structure
    phys_freepages_low();

    //page_debugprint_free();
    //page_t* page = page_alloc_pages(GFP_DMA16, 2);
    //page_t* page2 = page_alloc_pages(0, 2);
    //log_msg("page1=0x%lx, page2=0x%lx\n",page,page2);
    page_debugprint_free();

    //while (1);
}