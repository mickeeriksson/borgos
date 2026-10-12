#SRCDIR = ./src

include ../kernel/$(MACHDIR)/Machine.mk

#OBJDIR = build/$(MACHNAME)/obj
TARGETDIR = build/$(MACHNAME)
INITRDROOT = $(TARGETDIR)/initrd_root
#LDFILE = $(MACHDIR)/kernel.ld.in
#LDFILE = $(MACHDIR)/kernel.x86_64.ld.in
#RUST_OSLIB = target/x86_64-firstos/debug/libfirstos.a
#RUST_TARGETDIR = target/x86_64-firstos/debug/
#CRATE_LIBS = target/x86_64-firstos/debug/deps/libmultiboot2-d01a924e37b18677.rlib
#LDPT = -n --gc-sections
#LDPT = -n

#-Cforce-frame-pointers=yes = force framepointer to get stacktrace working. same as no-omit-frame-pointer in C
#export RUSTFLAGS = -C link-arg=-Tlinker.ld -Cforce-frame-pointers=yes


.PHONY: clean initrd.cpio

echo:
	@echo "(In initrd.mk) MACHDIR:" $(MACHDIR)
#	@echo "(In Kernel.mk) KERNEL_ASMSRC:" $(KERNEL_ASMSRC)
#	@echo "(In Kernel.mk) KERNEL_CSRC:" $(KERNEL_CSRC)
#	@echo "(In Kernel.mk) LIBK_CSRC:" $(LIBK_CSRC)
#	@echo "(In Kernel.mk) KERNEL_OBJS:" $(KERNEL_OBJS)
#	@echo "(In Kernel.mk) LIBCPP_CPPSRC:" $(LIBCPP_CPPSRC)
#	@echo "(In Kernel.mk) OBJDIRS:" $(OBJDIRS)


deepclean:
#	@echo "DEEPCLEAN!"
#	rm -rf target/x86_64-firstos
#	#rm -rf target/x86_64-firstos/debug/incremental
#	rm -rf target/debug/incremental
#	@echo "" #make space between targets....

fastclean:
	@echo "FASTCLEAN!"
#	#rm -rf target/x86_64-firstos
#	#rm -rf target/x86_64-firstos/debug/incremental
#	#rm -rf target/debug/incremental
#	#rm -rf $(OBJDIR)
	rm -rf $(TARGETDIR)/*
	rm -rf $(TARGETDIR)
	#@rm -rf  $(LIBK_OBJS)
#	mkdir --parents $(OBJDIRS)
	mkdir --parents $(TARGETDIR)
	@echo "" #make space between targets....
#	xargo clean


#$(OBJDIR)/trampoline.bin:
#	$(AS) $(ASFLAGS) -o $(OBJDIR)/trampoline_tmp.o $(CPUDIR)/smp_trampoline/trampoline.S
#	$(LD)  -Ttext 0x0 --oformat binary -o $@ $(OBJDIR)/trampoline_tmp.o

#$(OBJDIR)/trampoline_blob.o: $(OBJDIR)/trampoline.bin
#	llvm-objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $< $@

#kernel-lib:
#	RUST_TARGET_PATH=$(shell pwd) xargo -v build --target x86_64-firstos

#kernel-elf: echo clean $(KERNEL_OBJS) kernel-lib
#kernel-elf: echo clean kernel-lib
initrd: echo fastclean
	mkdir --parents $(INITRDROOT)
	cp initrd.mk $(INITRDROOT)/.
	cd $(INITRDROOT); find . | cpio -o -H newc > ../initrd.cpio
#	$(CPP) -E -P $(LDFILE) -o $(TARGETDIR)/kernel.ld
#	$(CPP) -E -P $(LDFILE) -o kernel.ld

#	RUST_TARGET_PATH=$(shell pwd) cargo build --verbose --target x86_64-firstos
#	cp  $(RUST_TARGETDIR)firstos kernel.elf
#	$(OBJCOPY) -O binary $(TARGETDIR)/kernel.elf $(TARGETDIR)/kernel.bin
#	$(CPP) -E -P $(LDFILE) -o $(TARGETDIR)/kernel.ld
#	$(LD) $(LDFLAGS) -T $(TARGETDIR)/kernel.ld $(LDFLAGS) -o $(TARGETDIR)/kernel.elf $(KERNEL_OBJS)
#	$(OBJCOPY) -O binary $(TARGETDIR)/kernel.elf $(TARGETDIR)/kernel.bin
