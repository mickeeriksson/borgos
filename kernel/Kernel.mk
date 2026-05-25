SRCDIR = ./src

include $(MACHDIR)/Machine.mk

OBJDIR = build/$(MACHNAME)/obj
TARGETDIR = build/$(MACHNAME)
#LDFILE = $(MACHDIR)/kernel.ld.in
LDFILE = $(MACHDIR)/kernel.x86_64.ld.in
#RUST_OSLIB = target/x86_64-firstos/debug/libfirstos.a
#RUST_TARGETDIR = target/x86_64-firstos/debug/
#CRATE_LIBS = target/x86_64-firstos/debug/deps/libmultiboot2-d01a924e37b18677.rlib
#LDPT = -n --gc-sections
#LDPT = -n

#-Cforce-frame-pointers=yes = force framepointer to get stacktrace working. same as no-omit-frame-pointer in C
#export RUSTFLAGS = -C link-arg=-Tlinker.ld -Cforce-frame-pointers=yes

KERNEL_CSRC := $(KERNEL_CSRC) \
$(wildcard $(SRCDIR)/boot/*.c) \
$(wildcard $(SRCDIR)/kcore/*.c) \
$(wildcard $(SRCDIR)/driver/*.c) \
$(wildcard $(SRCDIR)/mem/*.c) \
$(wildcard $(SRCDIR)/driver/video/*.c)

LIBCDIR = ./../libc/src

LIBK_CSRC := $(LIBK_CSRC) \
$(wildcard $(LIBCDIR)/stdio/*.c) \
$(wildcard $(LIBCDIR)/string/*.c)

KERNEL_OBJS := $(KERNEL_ASMSRC:$(SRCDIR)/%.S=$(OBJDIR)/%.o) \
               $(KERNEL_CSRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o) \
               $(LIBK_CSRC:$(LIBCDIR)/%.c=$(OBJDIR)/%.o)

#INCLUDES=-I$(MACHDIR)/include/ -I$(ARCHDIR)/include/ -I$(CPUDIR)/include/ -I./include -I$(LIBCDIR)/include/
INCLUDES=-I$(MACHDIR)/include/ -I$(ARCHDIR)/include/ -I$(CPUDIR)/include/ -I$(SRCDIR)/include -I$(LIBCDIR)/include/

#DUMMY1:=$(shell mkdir --parents $(KERNEL_OBJS)/..)
#OBJDIRS = $(subst /,/,$(sort $(dir $(KERNEL_OBJS))))
OBJDIRS = $(subst /,/,$(sort $(dir $(KERNEL_OBJS))))

.PHONY: clean kernel-elf kernel-lib

echo:
	@echo "(In Kernel.mk) MACHDIR:" $(MACHDIR)
	@echo "(In Kernel.mk) KERNEL_ASMSRC:" $(KERNEL_ASMSRC)
	@echo "(In Kernel.mk) KERNEL_CSRC:" $(KERNEL_CSRC)
	@echo "(In Kernel.mk) LIBK_CSRC:" $(LIBK_CSRC)
	@echo "(In Kernel.mk) KERNEL_OBJS:" $(KERNEL_OBJS)

deepclean:
	@echo "DEEPCLEAN!"
	rm -rf target/x86_64-firstos
	#rm -rf target/x86_64-firstos/debug/incremental
	rm -rf target/debug/incremental
	@echo "" #make space between targets....

fastclean:
	@echo "FASTCLEAN!"
#	#rm -rf target/x86_64-firstos
#	#rm -rf target/x86_64-firstos/debug/incremental
#	#rm -rf target/debug/incremental
	rm -rf $(OBJDIR)
	rm -rf $(TARGETDIR)/*
	rm -rf $(TARGETDIR)
	#@rm -rf  $(LIBK_OBJS)
	mkdir --parents $(OBJDIRS)
	mkdir --parents $(TARGETDIR)
	@echo "" #make space between targets....
#	xargo clean

#LIBC FILES
$(OBJDIR)/%.o: $(LIBCDIR)/%.c
	$(CC) $(INCLUDES) $(CFLAGS)   $< -o $@
	@echo "" #make space between compiles....

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(INCLUDES) $(CFLAGS)   $< -o $@
	@echo "" #make space between compiles....

$(OBJDIR)/%.o: $(SRCDIR)/%.S
	$(AS) $(ASFLAGS) $(INCLUDES) $< -o $@
	@echo "" #make space between compiles....

#kernel-lib:
#	RUST_TARGET_PATH=$(shell pwd) xargo -v build --target x86_64-firstos

#kernel-elf: echo clean $(KERNEL_OBJS) kernel-lib
#kernel-elf: echo clean kernel-lib
kernel-elf: echo fastclean $(KERNEL_OBJS)
#	$(CPP) -E -P $(LDFILE) -o $(TARGETDIR)/kernel.ld
#	$(CPP) -E -P $(LDFILE) -o kernel.ld

#	RUST_TARGET_PATH=$(shell pwd) cargo build --verbose --target x86_64-firstos
#	cp  $(RUST_TARGETDIR)firstos kernel.elf
#	$(OBJCOPY) -O binary $(TARGETDIR)/kernel.elf $(TARGETDIR)/kernel.bin
	$(CPP) -E -P $(LDFILE) -o $(TARGETDIR)/kernel.ld
	$(LD) $(LDFLAGS) -T $(TARGETDIR)/kernel.ld $(LDFLAGS) -o $(TARGETDIR)/kernel.elf $(KERNEL_OBJS)
#	$(OBJCOPY) -O binary $(TARGETDIR)/kernel.elf $(TARGETDIR)/kernel.bin
