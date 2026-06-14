#include "types.h"
#include "log.h"
#include "multiboot2.h"
#include <string.h>
#include "vfb.h"
#include "error.h"
#include "bootmem.h"
#include "mm.h"

//extern adr_t bootmem_start_address;

#define MULTIBOOT_TAG_TYPE_ACPI_OLD 14
struct multiboot_tag_acpiold {
    multiboot_uint32_t type;
    multiboot_uint32_t size;
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
};

adr_t mbi_addr;

void multiboot2_debugprint(adr_t addr) {
    log_msg("multiboot2 addr: 0x%x\n", addr);

    uint32_t mbisize = *((uint32_t*) addr);
    uint32_t mbireserved = *((uint32_t*) (addr+4));

    log_msg("multiboot2 size: 0x%x\n", mbisize);
    log_msg("multiboot2 reserved: 0x%x\n", mbireserved);

    struct multiboot_tag* tag;
    // addr + 8 hoppar över total_size och reserved
    for (tag = (struct multiboot_tag *)(addr + 8);tag->type != 0;tag = (struct multiboot_tag *)((char *)tag + ((tag->size + 7) & ~7))) {
        log_msg("tag type: %d (0x%x) len: 0x%x\n", tag->type,tag->type,tag->size);
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR: // Boot command line
                //struct multiboot_tag_load_base_addr* tag_load_base_addr=(multiboot_tag_load_base_addr*) tag;
                //log_msg("multiboot2 load base addr: 0x%x\n", tag_load_base_addr->load_base_addr);
                log_msg("  MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR\n");
                struct multiboot_tag_load_base_addr* tag_load_base_addr=(struct multiboot_tag_load_base_addr*) tag;
                log_msg("    multiboot2 load base addr:        0x%x\n", tag_load_base_addr->load_base_addr);
                break;
            case MULTIBOOT_TAG_TYPE_CMDLINE: // Boot command line
                log_msg("  MULTIBOOT_TAG_TYPE_CMDLINE\n");
                struct multiboot_tag_string* tag_cmdline=(struct multiboot_tag_string*) tag;
                log_msg("    multiboot2 cmd line:              %s\n", tag_cmdline->string);

                break;
            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME: // Memory map
                log_msg("  MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME\n");
                struct multiboot_tag_string* tag_bootloadername=(struct multiboot_tag_string*) tag;
                log_msg("    multiboot2 Bootloadername:        %s\n", tag_bootloadername->string);
                break;
            case MULTIBOOT_TAG_TYPE_APM:
                log_msg("  MULTIBOOT_TAG_TYPE_APM\n");
                //struct multiboot_tag_apm* tag_apm=(struct multiboot_tag_apm*) tag;
                break;
            case MULTIBOOT_TAG_TYPE_MMAP:
                log_msg("  MULTIBOOT_TAG_TYPE_MMAP\n");
                struct multiboot_tag_mmap* tag_mmap=(struct multiboot_tag_mmap*) tag;
                log_msg("    multiboot2 mmap->entry_size:      %d\n", tag_mmap->entry_size);
                log_msg("    multiboot2 mmap->entry_version:   %d\n", tag_mmap->entry_version);
                break;
            case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
                log_msg("  MULTIBOOT_TAG_TYPE_ELF_SECTIONS\n");
                //struct multiboot_tag_apm* tag_apm=(struct multiboot_tag_apm*) tag;
                break;
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                log_msg("  MULTIBOOT_TAG_TYPE_BASIC_MEMINFO\n");
                //struct multiboot_tag_apm* tag_apm=(struct multiboot_tag_apm*) tag;
                break;
            case MULTIBOOT_TAG_TYPE_BOOTDEV:
                log_msg("  MULTIBOOT_TAG_TYPE_BOOTDEV\n");
                //struct multiboot_tag_apm* tag_apm=(struct multiboot_tag_apm*) tag;
                break;
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                log_msg("  MULTIBOOT_TAG_TYPE_FRAMEBUFFER\n");
                struct multiboot_tag_framebuffer* tag_fb=(struct multiboot_tag_framebuffer*) tag;
                log_msg("    multiboot2 fb->framebuffer_addr:  0x%x\n", tag_fb->common.framebuffer_addr);
                log_msg("    multiboot2 fb->framebuffer_pitch: %d\n", tag_fb->common.framebuffer_pitch);
                log_msg("    multiboot2 fb->framebuffer_width: %d\n", tag_fb->common.framebuffer_width);
                log_msg("    multiboot2 fb->framebuffer_height:%d\n", tag_fb->common.framebuffer_height);
                log_msg("    multiboot2 fb->framebuffer_bpp:   %d\n", tag_fb->common.framebuffer_bpp);
                log_msg("    multiboot2 fb->framebuffer_type:  %d\n", tag_fb->common.framebuffer_type);

                if (tag_fb->common.framebuffer_type==MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
                    log_msg("    multiboot2 fb->red_field_position:   %d\n", tag_fb->framebuffer_red_field_position);
                    log_msg("    multiboot2 fb->red_mask_size:        %d\n", tag_fb->framebuffer_red_mask_size);
                    log_msg("    multiboot2 fb->green_field_position: %d\n", tag_fb->framebuffer_green_field_position);
                    log_msg("    multiboot2 fb->green_mask_size:      %d\n", tag_fb->framebuffer_green_mask_size);
                    log_msg("    multiboot2 fb->blue_field_position:  %d\n", tag_fb->framebuffer_blue_field_position);
                    log_msg("    multiboot2 fb->blue_mask_size:       %d\n", tag_fb->framebuffer_blue_mask_size);

                }
                break;
            case MULTIBOOT_TAG_TYPE_ACPI_OLD:
                log_msg("  MULTIBOOT_TAG_TYPE_ACPI_OLD\n");
                struct multiboot_tag_acpiold* tag_acpiold=(struct multiboot_tag_acpiold*) tag;
                log_msg("    RsdtAddress: 0x%lx\n", tag_acpiold->RsdtAddress);
                break;
            default:
                log_msg("  UNKNOWN MULTIBOOT2 TAG type:%d\n", tag->type);
        }
    }
}

struct multiboot_tag *multiboot2_gettag(uint32_t tagtype) {
    struct multiboot_tag* tag;
    // addr + 8 hoppar över total_size och reserved
    for (tag = (struct multiboot_tag *)(mbi_addr + 8);tag->type != 0;tag = (struct multiboot_tag *)((char *)tag + ((tag->size + 7) & ~7))) {
        if (tag->type == tagtype) {
            log_msg("found match on tag type: %d (0x%x) len: 0x%x\n", tag->type,tag->type,tag->size);
            return tag;
        }
    }
    return 0;
}

void multiboot2_verify_freemem(void) {
    struct multiboot_tag *basicmemtag = multiboot2_gettag(MULTIBOOT_TAG_TYPE_BASIC_MEMINFO) ;
    if (basicmemtag!=0) {
        log_msg("GOT MULTIBOOT_TAG_TYPE_BASIC_MEMINFO\n");
        struct multiboot_tag_basic_meminfo* meminfo = (struct multiboot_tag_basic_meminfo*) basicmemtag;
        log_msg("multiboot2 basic meminfo lower:  %d Kb\n", meminfo->mem_lower);
        log_msg("multiboot2 basic meminfo upper:  %d Kb\n", meminfo->mem_upper);
    }

    struct multiboot_tag *mmaptag = multiboot2_gettag(MULTIBOOT_TAG_TYPE_MMAP) ;
    if (mmaptag!=0) {
        log_msg("GOT MULTIBOOT_TAG_TYPE_MMAP\n");
        struct multiboot_tag_mmap* mmap=(struct multiboot_tag_mmap*) mmaptag;
        log_msg("multiboot2 mmap->entry_size:      %d\n", mmap->entry_size);
        log_msg("multiboot2 mmap->entry_version:   %d\n", mmap->entry_version);

        int i=0;
        for (i=0;(i*mmap->entry_size < mmap->size);i++) {
            log_msg("multiboot2 mmap->entry[%d]: \n", i);
            log_msg("    addr: 0x%x \n", mmap->entries[i].addr);
            log_msg("    len: 0x%x \n", mmap->entries[i].len);
            log_msg("    type: 0x%x \n", mmap->entries[i].type);

            //kprintf("multiboot2 mmap->entry[%d]: \n", i);
            //kprintf("    addr: 0x%x \n", mmap->entries[i].addr);
            //kprintf("    len: 0x%x \n", mmap->entries[i].len);
            //kprintf("    type: 0x%x \n", mmap->entries[i].type);
        }
    }

}


void multiboot2_relocate(adr_t new_mbi) {
    //test if any modules is put before mbi?
    log_msg("TODO, MULTIBOOT2, Test if any modules is located before multiboot2 info\n");
    //If so skip relocate and just set free mem to last of mbi and mods

    //adr_t new_mbi = bootmem_start_address;
    uint32_t mbisize = *((uint32_t*) mbi_addr);
    size_t len = mbisize;
    memcpy((void*) new_mbi  , (void*)mbi_addr , len);
    mbi_addr = new_mbi;
    //bootmem_start_address = bootmem_start_address+len;

    //relocate modules
    log_msg("TODO, MULTIBOOT2, Relocate Modules\n");
    //bootmem_start_address = last of the copied modules.

}

RESULT multiboot2_set_boot_vfb(struct fb_info* vfb) {
    struct multiboot_tag_framebuffer *fbtag = (struct multiboot_tag_framebuffer *) multiboot2_gettag(MULTIBOOT_TAG_TYPE_FRAMEBUFFER);
    if (fbtag!=0) {
        vfb->fbaddr = fbtag->common.framebuffer_addr;
        vfb->fbwidth = fbtag->common.framebuffer_width;
        vfb->fbheight = fbtag->common.framebuffer_height;
        vfb->fbpitch = fbtag->common.framebuffer_pitch;
        vfb->fbbpp = fbtag->common.framebuffer_bpp;
        if (fbtag->common.framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT) {
            log_msg("Set VFB to type VFB_TEXT\n");
            vfb->type = VFB_TEXT;
        }else if (fbtag->common.framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
            log_msg("Set VFB to type VFB_RGB\n");
            vfb->type = VFB_RGB;
        }else {
            log_msg("UNKNWON FRAMBUFFERTYPE %d\n",fbtag->common.framebuffer_type);
        }
        return OK;
    }
    vfb->type = 0;
    return ERROR;
}

RESULT multiboot2_set_bootmem(struct bootmem_info* bootmem,adr_t bootimage_end) {
    struct multiboot_tag *mmaptag = multiboot2_gettag(MULTIBOOT_TAG_TYPE_MMAP) ;
    if (mmaptag!=0) {
        log_msg("GOT MULTIBOOT_TAG_TYPE_MMAP\n");
        struct multiboot_tag_mmap* mmap=(struct multiboot_tag_mmap*) mmaptag;
        log_msg("multiboot2 mmap->entry_size:      %d\n", mmap->entry_size);
        log_msg("multiboot2 mmap->entry_version:   %d\n", mmap->entry_version);

        int i=0;
        for (i=0;(i*mmap->entry_size < mmap->size);i++) {
            if (mmap->entries[i].type==MULTIBOOT_MEMORY_AVAILABLE) {
                log_msg("multiboot2 FREEMEM mmap->entry[%d]: \n", i);
                log_msg("    addr: 0x%x \n", mmap->entries[i].addr);
                log_msg("    len: 0x%x \n", mmap->entries[i].len);

                if (mmap->entries[i].len > (1024*1024)) {
                    //is memory larger then 1mb?
                    adr_t start = mmap->entries[i].addr;
                    size_t len = mmap->entries[i].len;
                    adr_t end = start + len-1;
                    log_msg("Found acceptable bootmem at addr: 0x%x-0x%x len=%dKb \n", start,end,len/(1024) );

                    adr_t bootmemstart = start;


                    if ((bootimage_end >= start) && (bootimage_end < end)){
                        log_msg(" kernel is in this range adjust to bootimage_end at addr: 0x%x\n", bootimage_end );
                        bootmemstart = bootimage_end;
                        //align to 4096
                        bootmemstart = PAGEALIGN_UP(bootmemstart);
                        //bootmemstart = (bootmemstart & ~0xFFF) + 0x1000;
                        log_msg(" new aligned bootmemstart is at addr: 0x%x\n", bootmemstart );

                    }

#ifdef BOOTMEM_STARTMB
                    if (end > (BOOTMEM_STARTMB*1024*1024 + 8*1024*1024 )){
                        // at least 8mb free over BOOTMEM_STARTMB
                        // keep first 16mb free for DMA transfer
                        bootmemstart = BOOTMEM_STARTMB*1024*1024; // start at 16mb
                        if (bootimage_end >= bootmemstart) {
                            PANIC("BootImage INSIDE bootmemstart set by BOOTMEM_STARTMB");
                        }
                        log_msg("  BOOTMEM_STARTMB set to %d, adjust to BOOTMEM_STARTMB at addr: 0x%x\n", BOOTMEM_STARTMB, bootmemstart );
                    }
#endif

                    bootmemstart = PAGEALIGN_UP(bootmemstart);
                    bootmem->addr = bootmemstart;
                    bootmem->heaptop = bootmemstart;
                    bootmem->maxsize = (start + len) - bootmemstart;
                    log_msg("SET bootmem at 0x%x, maxsize=0x%x\n",bootmem->addr,bootmem->maxsize);

                    uint32_t mbisstart = mbi_addr;
                    uint32_t mbisize = *((uint32_t*) mbi_addr);
                    log_msg("original multiboot2 record at addr: 0x%x with size %d\n", mbisstart,mbisize );

                    //if ((mbisstart >= bootmemstart) && (mbisstart < end)){
                        //log_msg(" multiboot2 structure is in this range relocate!!\n" );

                        //align to 4096
                        //bootmem_start_address = (bootmem_start_address & ~0xFFF) + 0x1000;
                        //log_msg("new bootmem_start_address = 0x%x\n", bootmem_start_address);
                        multiboot2_relocate(bootmemstart);
                        mbisstart = mbi_addr;
                        mbisize = *((uint32_t*) mbi_addr);
                        log_msg("relocated multiboot2 record at addr: 0x%x with size %d\n", mbisstart,mbisize );

                        //log_msg(" multiboot2 structure is in this range adjust to mbi end at addr: 0x%x\n", mbisstart+mbisize );
                        //bootmemstart = mbisstart+mbisize;
                        bootmem->heaptop = mbisstart+mbisize;
                    //}

                    //test for multiboot modules
                    log_msg("TODO test for multiboot modules\n");



                    return OK;
                }
            }
        }


    }
    return ERROR;
}

RESULT multiboot2_set_bootmem_map(struct bootmem_info* bootmem) {
    struct multiboot_tag *mmaptag = multiboot2_gettag(MULTIBOOT_TAG_TYPE_MMAP) ;
    if (mmaptag!=0) {
        log_msg("GOT MULTIBOOT_TAG_TYPE_MMAP\n");
        struct multiboot_tag_mmap* mmap=(struct multiboot_tag_mmap*) mmaptag;
        log_msg("multiboot2 mmap->entry_size:      %d\n", mmap->entry_size);
        log_msg("multiboot2 mmap->entry_version:   %d\n", mmap->entry_version);

        //build bootmem memory map
        int multiboot2_entries = (mmap->size / mmap->entry_size);
        size_t bootmemMapSize = (multiboot2_entries +1) * sizeof(struct bootmem_memmapentry);
        log_msg("multiboot2_entries=%d\n",multiboot2_entries);
        log_msg("bootmemMapSize=%d (0x%x)\n",bootmemMapSize,bootmemMapSize);

        log_msg("bootmemMapSize=%d (0x%x)\n",bootmemMapSize,bootmemMapSize);
        bootmem_memmapentry = bootmem_alloc(bootmemMapSize);

        int i=0;
        for (i=0;(i*mmap->entry_size < mmap->size);i++) {
            struct multiboot_mmap_entry *mmapentry = &mmap->entries[i];
            log_msg("parse entry=%d\n",mmapentry->type);
            bootmem_memmapentry[i].addr = mmap->entries[i].addr;
            bootmem_memmapentry[i].size = mmap->entries[i].len;
            switch (mmap->entries[i].type) {
                case MULTIBOOT_MEMORY_AVAILABLE:
                    bootmem_memmapentry[i].type = BOOTMEMTYPE_FREE;
                    break;
                default:
                    bootmem_memmapentry[i].type = BOOTMEMTYPE_UNKNOWN;
                    break;
            }
        }
        bootmem_memmapentry[i].type = BOOTMEMTYPE_END;
        return OK;
    }
    return ERROR;
}

void multiboot2_init(adr_t addr) {
    mbi_addr = addr;
    multiboot2_debugprint(mbi_addr);

    /*
    if ( (mbi_addr & 0xFFFFFFFF) - (bootmem_start_address & 0xFFFFFFFF) > 256) {
        log_msg("Memory hole to mboot2 = %d relocate!\n", (mbi_addr-bootmem_start_address));
        multiboot2_relocate();
        log_msg("************* NEW MBI ***************\n");
        multiboot2_debugprint(mbi_addr);
    }*/



}
