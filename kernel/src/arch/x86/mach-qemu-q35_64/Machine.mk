MACHNAME = QEMUQ35_64
CPUDIR = $(ARCHDIR)/cpu/x86_64

#AS = x86_64-linux-gnu-gcc
AS = clang
#CC = x86_64-linux-gnu-gcc
CC = clang
#LD = clang
LD = ld.lld
CPP = cpp
OBJCOPY = x86_64-linux-gnu-objcopy

#NO REDZONE, not needed for ARM, only used by GCC fpr x86-64?
#

#CFLAGS = -Wall -Werror -O0  -std=c11 -ffreestanding -nostdlib -mno-red-zone -mcmodel=large -mno-abm -Wno-unused-parameter -Wno-unused-variable -nostartfiles -mgeneral-regs-only $(DQEMU) -ggdb -c
#ASFLAGS = $(DQEMU) -ggdb -c
##LDFLAGS = -melf_i386
#LDFLAGS = -melf_x86_64

CFLAGS = -target x86_64-none-elf -Wall -Werror  -Wno-unused-but-set-variable -Wno-division-by-zero -O0  -std=c11 -ffreestanding -nostdlib -mcmodel=large -static -mno-red-zone -mgeneral-regs-only $(DQEMU) -ggdb -c
#-Wno-unused-parameter -nostartfiles

ASFLAGS = -target x86_64-none-elf -static $(DQEMU) -ggdb -c

LDFLAGS = -nostdlib

KERNEL_CSRC := $(wildcard $(MACHDIR)/*.c) $(wildcard $(CPUDIR)/*.c)
KERNEL_ASMSRC = $(wildcard $(ARCHDIR)/*.S) $(wildcard $(MACHDIR)/*.S) $(wildcard $(CPUDIR)/*.S)
#KERNEL_ASMSRC := $(KERNEL_ASMSRC)
