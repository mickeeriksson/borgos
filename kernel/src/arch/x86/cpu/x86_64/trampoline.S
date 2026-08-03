/* ============================================================
 * trampoline.S — AP-trampolin, x86_64
 * Fast länkad adress: 0x8000 (real mode -> protected mode -> long mode)
 *
 * VIKTIGT — denna kod är INTE positionsoberoende:
 *   - Alla adresser (GDT-bas, far-jump-mål, parameterblock) är
 *     länktidskonstanter baserade på VMA 0x8000.
 *   - Filen får kompileras in i kärnan på en helt annan länkadress
 *     (kärnans .trampoline-sektion sätts till VMA=0x8000 / LMA=där
 *     kärnan faktiskt lägger blobben, via linkerscript — se nedan).
 *   - Kärnan MÅSTE memcpy:a blobben till FYSISK adress 0x8000 innan
 *     INIT/SIPI skickas till AP:n. Om den ligger någon annanstans när
 *     CPU:n börjar exekvera stämmer inte adresserna.
 *   - SIPI-vektorn måste vara 0x08 (CS = vektor<<8 = 0x0800,
 *     IP = 0000 => CS*16 = 0x8000).
 *
 * Layout (absolut fysisk adress):
 *   0x8000  jmp real_start  (+ padding)
 *   0x8008  param_cr3    (8 byte)  - PML4-bas att ladda in i CR3
 *   0x8010  param_stack  (8 byte)  - RSP att använda i long mode
 *   0x8018  param_entry  (8 byte)  - 64-bitars C-funktion att hoppa till
 *   0x8020  real_start:  16-bit kod börjar här
 *
 * Kärnans startkod ska (i denna ordning):
 *   1. Skriva param_cr3/param_stack/param_entry på 0x8008/0x8010/0x8018
 *      i KÄLLBLOBBEN (eller efter kopiering, direkt på 0x8000+offset).
 *   2. memcpy(0x8000, trampoline_lma, trampoline_end - trampoline_start).
 *   3. Skicka INIT + SIPI(vektor 0x08) till AP:n.
 * ============================================================ */

.section .trampoline, "ax"
.code16
.global trampoline_start
.global trampoline_end
.align 4096

trampoline_start:
    jmp     1f              /* måste hoppa förbi parameterblocket:      */
                             /* CPU:n börjar exekvera exakt vid offset 0 */
    .align 8

/* ---------------- Parameterblock (fasta fysiska adresser 0x8008/0x8010/0x8018) ---------------- */
param_cr3:
    .quad 0
param_stack:
    .quad 0
param_entry:
    .quad 0

    .align 8
1:
/* ================= 16-bit realmode (kör alltid från fysisk 0x8000) ================= */
real_start:
    cli
    xor     %ax, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %ss

    /* Ingen körtidsberäkning av basadress behövs längre — koden är
       länkad för att exekvera exakt på 0x8000, och DS=0 ger flat
       16-bitsadressering som matchar de absoluta adresserna direkt
       (0x8000 ryms inom ett enda real-mode-segment). */

    lgdtl   gdt_ptr          /* gdt_ptr:s bas-fält är nu en länktidskonstant */

    mov     %cr0, %eax
    or      $1, %eax         /* PE */
    mov     %eax, %cr0

    /* Direkt far jump med länktidskonstant segment+offset — ingen
       indirektion eller runtime-patchning av farptr behövs längre */
    ljmp    $0x08, $prot_start

/* ---------------- GDT ---------------- */
    .align 8
gdt_start:
    .quad 0x0000000000000000   /* null */
    .quad 0x00CF9A000000FFFF   /* 0x08: code32, flat */
    .quad 0x00CF92000000FFFF   /* 0x10: data32, flat */
    .quad 0x00AF9A000000FFFF   /* 0x18: code64 */
gdt_end:

gdt_ptr:
    .word   gdt_end - gdt_start - 1
    .long   gdt_start           /* absolut adress, känd vid länktid (VMA 0x8000) */

/* ================= 32-bit protected mode ================= */
    .code32
prot_start:
    mov     $0x10, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %ss
    mov     %ax, %fs
    mov     %ax, %gs

    /* Aktivera PAE (krävs för long mode) */
    mov     %cr4, %eax
    or      $0x20, %eax
    mov     %eax, %cr4

    /* Absolut adressering funkar direkt nu — ingen EBX-korrigering
       behövs eftersom vi vet exakt var koden kör (0x8000). */
    mov     param_cr3, %eax
    mov     %eax, %cr3

    /* Sätt EFER.LME */
    mov     $0xC0000080, %ecx
    rdmsr
    or      $0x100, %eax
    wrmsr

    /* Slå på paging -> CPU går in i long mode (compatibility submode) */
    mov     %cr0, %eax
    or      $0x80000000, %eax
    mov     %eax, %cr0

    /* Direkt far jump till 64-bit-entry, länktidskonstant */
    ljmp    $0x18, $long_start

/* ================= 64-bit long mode ================= */
    .code64
long_start:
    xor     %ax, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %ss
    mov     %ax, %fs
    mov     %ax, %gs

    /* RIP-relativt även här — funkar oavsett var blobben faktiskt
       kompileras in, eftersom RIP alltid är den verkliga fysiska
       exekveringsadressen (0x8000 + offset) vid körning. */
    lea     param_stack(%rip), %rax
    mov     (%rax), %rsp

    lea     param_entry(%rip), %rax
    mov     (%rax), %rax

    /* Hoppa till C-koden, t.ex. void ap_main(void) */
    subq $8, %rsp  //eftersom vi gör en jmp istf en call så behöver vi justera stackpekaren så att den blir 16bytesalignad -8 bytes 
    jmp     *%rax

trampoline_end:
