
#include "mm.h"
#include "log.h"
#include "error.h"
#include "list.h"
#include "string.h"

extern zone_t  memoryzone[ZONECOUNT];
extern pageframe_t *pageframemap;


void _addToBuddyFreeList(pageframe_t* page,int idx, int order){

    pageframe_t* p;
    pageframe_t* buddypage=0;
    struct list_head *pos;

    zone_t*  zone = page->zone;
    free_area_t* freeArea = zone->free_area;

    //find out index of buddy
    int size = 1<<(order);
    //kprintf("  rest = %d  ==> buddyidx=%d\n",rest,buddyidx);
    int rest = idx % (1<<(order+1));
    //kprintf("  rest = %d\n",rest);
    int buddyidx=0;
    if(rest==0){
        buddyidx = idx+size;
    }else{
        buddyidx = idx-rest;
    }
    //kprintf("  rest = %d  ==> buddyidx=%d  size=%d\n",rest,buddyidx,size);

    if(order>ALLOCBUDDY_MAXORDER){
        PANIC("cant add page with order > MAXORDER\n");
    }

    //remove page from whatever list it belongs to!
    list_del(&(page->pagelist));

    if(order>=ALLOCBUDDY_MAXORDER){
        MMLOG2("MMLOG2 Max Order %d reached just add to freelist \n",order);
        list_add(&(page->pagelist), &(freeArea[order].freelist));
        return;
    }
    if(pageframemap[buddyidx].usecount>0){
        MMLOG2("MMLOG2 Buddy %d is in use count=%d, no need to find it, just add me to freelist \n",buddyidx,pageframemap[buddyidx].usecount);
        list_add(&(page->pagelist), &(freeArea[order].freelist));
        return;
    }

    //find buddy in freelist.
    list_for_each(pos, &(freeArea[order].freelist)){
        p = list_entry(pos, pageframe_t, pagelist);
        //int tidx= __page2idx(p);
        int tidx= PAGE2PFN(p);
        MMLOG3("MMLOG3 Test idx=%d  for page 0x%x  \n",tidx,p);
        if(tidx==buddyidx){
            buddypage=p;
            goto endforeach;
        }
    }
    endforeach:
    if(buddypage!=0){
        //kprintf("Found buddy idx=%d  at (0x%x) \n",p->idx,p);
        list_del(&(buddypage->pagelist));
        if(rest==0){
            //add self to higher order
            page->order++;
            buddypage->order=-1; //this is a tagalong to another page.
            MMLOG2("MMLOG2  Add self %d to higher order (and buddy %d) to order %d\n",idx,buddyidx,order+1);
            _addToBuddyFreeList(page,idx, order+1);
        }else{
            //add buddy to higher order
            buddypage->order++;
            page->order=-1; //this is a tagalong to another page.
            MMLOG2("MMLOG2  Add buddy %d to higher order (and self %d) to order %d\n",buddyidx,idx,order+1);
            _addToBuddyFreeList(buddypage,buddyidx, order+1);
        }

    }else{
        //add to freelist
        MMLOG2("MMLOG2 No buddy %d for %d add to order %d\n",buddyidx,idx,order);
        list_add(&(page->pagelist), &(freeArea[order].freelist));
    }
}

int _findFreePageInBuddy(zone_t* zone, int order){
    //TODO

    pageframe_t* p=(pageframe_t*)NULL;
    pageframe_t* bp=(pageframe_t*)NULL;

    if(order>ALLOCBUDDY_MAXORDER){
        //out of memory
        kprintf("_findFreePageInBuddy order %d > MAXORDER (out of memory)\n",order);
        //return -1;
        PANIC("order > MAXORDER\n");
    }

    free_area_t* freeArea = zone->free_area;

    //pop head
    int isempty = list_empty(&(freeArea[order].freelist));
    //struct list_head *lsth;
    //lsth = &(free_area[order].freelist);
    //MMLOG("_findFreePageInBuddy order=%d listIsEmpty=0x%x head=0x%x next=0x%x\n",order,isempty,lsth,lsth->next);
    if(isempty){
        p=0;
        goto searchdone;
    }
    p = list_entry(freeArea[order].freelist.next, pageframe_t, pagelist);

    searchdone:
    if(p!=NULL){
        //int pidx= __page2idx(p);
        int pidx= PAGE2PFN(p);

        MMLOG("MMLOG Found free page idx=%d for page_t* %#x  order=%d\n",pidx,p, order);
        return pidx;
    }else{
        int higherOrderIdx = _findFreePageInBuddy(zone,order+1);
        if(higherOrderIdx>0){
            p = &pageframemap[higherOrderIdx];
            MMLOG("MMLOG Found higher order index page idx=%d  for page 0x%x  when searching for order = %d\n",higherOrderIdx,p,order);
            //split in half
            int size = 1<<order;
            int baseidx = higherOrderIdx;
            int buddyidx = baseidx+size;
            //kprintf("   split in half to order=%d with idx=%d and idx=%d \n",order,baseidx,buddyidx);
            list_del(&(p->pagelist));
            //add baseidx to right list
            p->order = order;
            list_add(&(p->pagelist), &(freeArea[order].freelist));
            //add buddyidx to right list
            bp = &pageframemap[buddyidx];
            bp->order = order;
            list_add(&(bp->pagelist), &(freeArea[order].freelist));

            return baseidx;
        }else{
            kprintf("NO higher order page ?????\n");
        }
    }

    kprintf("Should never get here???? has page 0x%x \n",p);
    //endforeach:

    return -1;

}


void pageframe_init(pageframe_t *frame) {
    frame->order = 0;       // 1 frame
    frame->usecount = 444;  //not set
    frame->zone = 0;        //not set
    frame->memsegment = 4242; // not set
    INIT_LIST_HEAD(&(frame->pagelist));
}


void pageframe_debugprint_free(void){
    struct list_head *pos;
    log_msg("******************************************\n");

    for(int zi=0;zi<ZONECOUNT;zi++){
        for(int i=0;i<=ALLOCBUDDY_MAXORDER;i++){
            //find buddy in freelist.
            int free=0;
            list_for_each(pos, &(memoryzone[zi].free_area[i].freelist)){
                free++;
            }
            log_msg("***  Zone(%s) Order %d free=%d\n",memoryzone[zi].name,i,free);
        }
    }


    log_msg("******************************************\n");
}

/// free page in upperspace (kernelspace)
void pageframe_free_page_pfn(int pfn)
{
    pageframe_t* page = &pageframemap[pfn];
    int order = page->order;
    zone_t*  zone = page->zone;
    MMLOG("Free page idx:%d  at mem:0x%lx order=%d zone=%s\n",pfn,page,order,zone->name);
    page->usecount--;
    if(page->usecount==0){
        //memmap[p].flags=MMFLG_NORMAL;
        //page->usage="FREE";
        _addToBuddyFreeList(page,pfn,order);
    }else{
        PANIC("Try to free page with count > 0, this might not be a problem, but want to try to trap it for now.... :-) ");
    }
}


//void* _alloc_pages_byorder(uint32_t flags,char* usage,int order)
pageframe_t* _alloc_pages_byorder(uint32_t flags,int order)
{
    pageframe_t* p=0;
    zone_t* zone = &memoryzone[ZONENORMAL];
    if(flags & GFP_DMA16){
        //free_area_t* freeArea = memoryzone[ZONEDMA16].free_area;
        zone = &memoryzone[ZONEDMA16];
    }
    MMLOG("MMLOG Get Free page from zone=%s\n",zone->name);

    //kprintf("Alloc page for use=%s order=%d\n",usage,order);
    int idx = _findFreePageInBuddy(zone,order);
    MMLOG3("MMLOG3 Got free page %d from _findFreePageInBuddy \n",idx);
    if(idx==-1){
        //page_alloc_debugprint_free();
        PANIC("Out of memory \n");
    }
    p = &pageframemap[idx];
    list_del(&(p->pagelist));
    //p->usage=usage;
    p->usecount++;

    return p;

    //void*  kva = (void*) P2V(idx*PAGESIZE);
    //log_msg("Alloc page idx=%d -> kva=0x%x, usage=%s  \n",pageidx,kva, usage);
    //kprintf("Alloc pages idx:%d at mem:0x%x order=%d\n",idx,kva,order);

    //return kva;
}

// Returns pointer to free page in upperspace (kernelspace)
//void* alloc_page(uint32_t flags,char* usage)
pageframe_t* pageframe_alloc_page(uint32_t flags)
{
    pageframe_t* p = _alloc_pages_byorder(flags,0);
    MMLOG3("MMLOG3 Got page %#x from zone %s \n",p,p->zone->name);
    void* newmem = (void*) PAGE2VIRT(p);
    memset(newmem, 0, PAGESIZE);
    return p;
}

pageframe_t* pageframe_alloc_pages(uint32_t flags, unsigned int order){
    pageframe_t* p = _alloc_pages_byorder(flags,order);
    MMLOG3("MMLOG3 Got pages %#x from zone %s \n",p,p->zone->name);
    void* newmem = (void*) PAGE2VIRT(p);
    memset(newmem, 0, PAGESIZE<<order);
    return p;
}
