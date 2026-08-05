#include "cpu/gdt.h"
#include "log.h"
#include "cpu.h"
#include "error.h"
#include "mm.h"

gdt_entry_64_t bootgdt[GDTSIZE];    //boot gdt
gdtr_ptr_t gdtr;                //boot gdt ptr

void* per_cpu_gdtpage;          //per cpu gdt


gdtr_ptr_t gdt_sgdt (void) {
    gdtr_ptr_t gdtr;
    __asm__ volatile ("sgdt %0" : "=m" (gdtr));

    log_msg("GDT Basadress: 0x%lx\n", (unsigned long long)gdtr.base);
    log_msg("GDT Size:     0x%x\n", gdtr.size);

    return gdtr;
}

void gdt_lgdt (adr_t gdtptr_virtaddr) {
    __asm__ volatile("lgdt (%0)" : : "r" (gdtptr_virtaddr));
}

void clang_flush_gdt64_ext(adr_t gdt_ptr_addr, uint64_t code_sel, uint64_t data_sel) {
    __asm__ volatile(
        // 1. Ladda GDT-pekaren in i GDTR-registret
        "lgdt (%0)\n"

        // 2. Uppdatera alla datasegmentregister med data_sel
        "mov %2, %%rax\n"
        "mov %%rax, %%ds\n"
        "mov %%rax, %%es\n"
        "mov %%rax, %%fs\n"
        "mov %%rax, %%gs\n"
        "mov %%rax, %%ss\n"

        // 3. Utför en Far Return (lretq) för att ladda om CS och RIP
        "pushq %1\n"               // Tryck ned den valda kodselektorn (CS)
        "leaq 1f(%%rip), %%rax\n"  // Hämta RIP-relativ adress för etiketten '1'
        "pushq %%rax\n"            // Tryck ned måladressen (RIP) på stacken
        "lretq\n"                  // Poppar RIP och CS samtidigt

        "1:\n"                     // Hit hoppar lretq
        :
        : "r" (gdt_ptr_addr),        // %0
          "r" (code_sel),            // %1
          "r" (data_sel)             // %2
        : "rax", "cc", "memory"      // Berätta för Clang vilka register/resurser vi ändrar
    );
}

void gdt_debug_GDT_entry_64(gdt_entry_64_t* entry) {
    int access = entry->type | entry->s << 4 | entry->dpl << 5 | entry->p << 7;
    log_msg("    Access: 0x%x\n", access);
    log_msg("    Access (type): 0x%0x\n", entry->type);
    log_msg("    Access (s)   : 0x%x\n", entry->s);
    log_msg("    Access (dpl) : 0x%x\n", entry->dpl);
    log_msg("    Access (p)   : 0x%x\n", entry->p);
}


void gdt_debugCurrentGDT(void){
    gdtr_ptr_t gdtr;
    gdtr = gdt_sgdt ();

    int entrysize = sizeof(gdt_entry_64_t);
    int tblsize = (gdtr.size+1)/entrysize;
    gdt_entry_64_t* gdttable = (gdt_entry_64_t*) gdtr.base;


    gdt_entry_64_t* entry;
    for (int i = 0; i < tblsize; i++) {
        entry = &(gdttable[i]);
        uint64_t entry64raw = *((uint64_t*) &(gdttable[i]));

        log_msg("GDT Entry: %d/%d at 0x%lx  entry : 0x%lx\n", i,tblsize,entry,entry64raw);
        gdt_debug_GDT_entry_64(entry);
    }
    /*
    let gp : GDTPointer = asm_sgdt();

    let entries = (gp.limit+1)/8;
    for i in 0..entries{
        let eadr = gp.base + (i*8) as u64;
        let ptr = eadr as *const u64;
        let entry = unsafe{ &*ptr};
        debug!("GDT {} = {:#X} ({})",i,entry,entry);
        let access = (entry >> 40) & 0xFF;
        let flags = (entry >> 52) & 0x0F;
        debug!("  > flags={:#X} access={:#X} ",flags,access);

    }
    */
    log_msg("\n");
    //let _x=3;
}

void gdt_flush_gdt64_current_cpu() {
    cpu_t* cpup = CURRENTCPU;
    clang_flush_gdt64_ext((adr_t) &cpup->archcpu.gdtptr, 0x10 ,0x20);
    log_msg("\nLoaded new GDT Table for CPU:%d\n",cpup->cpuid);
    //gdt_debugCurrentGDT();
}

void gdt_setup_gdt(gdt_entry_64_t *gdt) {


    for (int i = 0; i < GDTSIZE; i++) {
        uint64_t* entry64raw = ((uint64_t*) &(gdt[i]));
        *entry64raw = 0;
        entry64raw++;
        *entry64raw = 0;

        //gdt_entry_64_t *entry = &gdt[i];
        //log_msg("Nulling GDT Entry: %d/%d at 0x%lx  entry : 0x%lx\n", i,GDTSIZE,entry,*entry);
    }
    //Entry 0 is null descriptor

    //Entry 1 is KERNEL CODE long descriptor
    gdt_entry_64_t* kcode64 = &gdt[1];
    kcode64->lim_15_0 = 0xFFFF;
    kcode64->base_15_0 = 0x0000;
    kcode64->base_23_16 = 0x00;
    kcode64->type = (1<<1) | (1<<3) | (1<<0); //rw(1) + execute(3) + accesed(0)
    kcode64->s = 1; //1=Code or Data segment
    kcode64->dpl = 0; //Kernel (highest level)
    kcode64->p = 1; //Is Present
    kcode64->lim_19_16 = 0xF;
    kcode64->avl = 0; //unsused
    kcode64->l = 1; //Long Mode
    kcode64->db = 0; //not set for long mode code segment even if it is 32bit+
    kcode64->g = 1; //Granularity 1= limmit is in 4k granularity
    kcode64->base_31_24 = 0x00;
    kcode64->base_32_63 = 0x00000000;
    kcode64->reserved_31_0 = 0x00000000;
    log_msg("SET GDT Entry: %d/%d at 0x%lx  entry : 0x%lx\n",1,GDTSIZE,kcode64,*kcode64);


    //Entry 2 is KERNEL DATA long descriptor
    gdt_entry_64_t* kdata64 = &gdt[2];
    kdata64->lim_15_0 = 0xFFFF;
    kdata64->base_15_0 = 0x0000;
    kdata64->base_23_16 = 0x00;
    kdata64->type = (1<<1) | (1<<0); //rw(1) + accesed(0)
    kdata64->s = 1; //1=Code or Data segment
    kdata64->dpl = 0; //Kernel (highest level)
    kdata64->p = 1; //Is Present
    kdata64->lim_19_16 = 0xF;
    kdata64->avl = 0; //unsused
    kdata64->l = 0; //Long Mode (not set for 64 data segment)
    kdata64->db = 1; //32bit+ segment
    kdata64->g = 1; //Granularity 1= limmit is in 4k granularity
    kdata64->base_31_24 = 0x00;
    kdata64->base_32_63 = 0x00000000;
    kdata64->reserved_31_0 = 0x00000000;
    log_msg("SET GDT Entry: %d/%d at 0x%lx  entry : 0x%lx\n",2,GDTSIZE,kdata64,*kdata64);
}

void gdt_init_per_cpu(void) {
    log_msg("gdt_init_per_cpu\n");
    size_t sizepercpu = GDTSIZE*sizeof(gdt_entry_64_t);
    log_msg("  size of  gdt_entry_64_t = %d\n", sizeof(gdt_entry_64_t));
    log_msg("  size per cpu = %d\n", sizepercpu);
    log_msg("  count per page = %d\n", PAGESIZE / sizepercpu);
    log_msg("  rest = %d\n", PAGESIZE % sizepercpu);

    if (GDTSIZE*16*16 != PAGESIZE) {
        PANIC("Current allocation is based on 16cpu gdt's per PAGE");
    }

    size_t gdtsize = cpu_numcores*sizepercpu;
    log_msg("  need size for all gdt's = %d\n", gdtsize);
    size_t porder = size_to_pageorder(gdtsize);
    log_msg("  porder = %d\n", porder);

    per_cpu_gdtpage = page_alloc_pages_virt(ZONENORMAL, porder);

    // build per cpu GDT
    for (int i = 0; i < cpu_numcores; i++) {
        cpu_t* cpup = &cpu[i];
        log_msg("Build GDT for CPU %d\n",cpup->cpuid);
        size_t offset = sizepercpu*i;
        gdt_entry_64_t* gdtp = per_cpu_gdtpage+offset;
        log_msg(" gdt_entry_64_t* = %#lx \n",gdtp);
        cpup->archcpu.per_cpu_gdt = gdtp;

        gdt_setup_gdt(gdtp);
        cpup->archcpu.gdtptr.size = 3*sizeof(gdt_entry_64_t)-1;
        cpup->archcpu.gdtptr.base = (uint64_t) gdtp;
        log_msg("  \n",gdtp);
    }

    return;
}

void gdt_init(void) {
    log_msg("Init GDT\n");
    log_msg("previous GDT Table:\n");

    gdt_debugCurrentGDT();

    gdt_setup_gdt(&bootgdt[0]);

    /*
    for (int i = 0; i < GDTSIZE; i++) {
        uint64_t* entry64raw = ((uint64_t*) &(gdt[i]));
        *entry64raw = 0;
        entry64raw++;
        *entry64raw = 0;

        gdt_entry_64_t *entry = &(gdt[i]);
        log_msg("GDT Entry: %d/%d at 0x%lx  entry : 0x%lx\n", i,GDTSIZE,entry);
    }


    //Entry 0 is null descriptor

    //Entry 1 is KERNEL CODE long descriptor
    gdt_entry_64_t* kcode64 = &gdt[1];
    kcode64->lim_15_0 = 0xFFFF;
    kcode64->base_15_0 = 0x0000;
    kcode64->base_23_16 = 0x00;
    kcode64->type = (1<<1) | (1<<3) | (1<<0); //rw(1) + execute(3) + accesed(0)
    kcode64->s = 1; //1=Code or Data segment
    kcode64->dpl = 0; //Kernel (highest level)
    kcode64->p = 1; //Is Present
    kcode64->lim_19_16 = 0xF;
    kcode64->avl = 0; //unsused
    kcode64->l = 1; //Long Mode
    kcode64->db = 0; //not set for long mode code segment even if it is 32bit+
    kcode64->g = 1; //Granularity 1= limmit is in 4k granularity
    kcode64->base_31_24 = 0x00;
    kcode64->base_32_63 = 0x00000000;
    kcode64->reserved_31_0 = 0x00000000;


    //Entry 2 is KERNEL DATA long descriptor
    gdt_entry_64_t* kdata64 = &gdt[2];
    kdata64->lim_15_0 = 0xFFFF;
    kdata64->base_15_0 = 0x0000;
    kdata64->base_23_16 = 0x00;
    kdata64->type = (1<<1) | (1<<0); //rw(1) + accesed(0)
    kdata64->s = 1; //1=Code or Data segment
    kdata64->dpl = 0; //Kernel (highest level)
    kdata64->p = 1; //Is Present
    kdata64->lim_19_16 = 0xF;
    kdata64->avl = 0; //unsused
    kdata64->l = 0; //Long Mode (not set for 64 data segment)
    kdata64->db = 1; //32bit+ segment
    kdata64->g = 1; //Granularity 1= limmit is in 4k granularity
    kdata64->base_31_24 = 0x00;
    kdata64->base_32_63 = 0x00000000;
    kdata64->reserved_31_0 = 0x00000000;
    */

    gdtr.size = 3*sizeof(gdt_entry_64_t)-1;
    gdtr.base = (uint64_t) &bootgdt[0];

    gdtr_ptr_t *dbggdtr = &gdtr;
    log_msg("gdt_ptr: 0x%lx\n", dbggdtr);


    //gdt_lgdt ((adr_t) &gdtr);
    //gdt_flush();
    clang_flush_gdt64_ext((adr_t) &gdtr, 0x10 ,0x20);
    /*
    S_GDT.prep(|| GDT::new() );
    unsafe{
        let gdt : &mut GDT = S_GDT.unsafe_mut();
        gdt.table[1] = GDT_DESC_KERNELCODE_64;
        gdt.table[2] = GDT_DESC_KERNELDATA_64;
        gdt.table[3] = GDT_DESC_KERNELCODE_32;

        let gdtadr = gdt as *const _ as u64;

        let gdtptr: &GDTPointer = &GDTPointer { limit: 31, base: gdtadr, };
        asm_lgdt(gdtptr);
        //let _x=3;
    }
    asm_gdtflushdataseg(0x10); //select KDATA64
    */
    log_msg("\nNEW GDT Table:\n");
    gdt_debugCurrentGDT();
}
