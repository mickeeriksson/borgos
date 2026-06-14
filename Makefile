
x86_64: ARCHDIR = ./src/arch/x86_64
x86_64: MACHDIR = $(ARCHDIR)

qemuq35_64: ARCHDIR = ./src/arch/x86
qemuq35_64: MACHDIR = $(ARCHDIR)/mach-qemu-q35_64

qemuq35_64: kernel-qemuq35_64
#x86_64: kernel-qemuq35_64


all: qemuq35_64
	@echo "ALL DONE!"

clean:
#	@echo "MAKE KERNEL MACHDIR="$(MACHDIR)
#	@cd kernel; make ARCHDIR=$(ARCHDIR) MACHDIR=$(MACHDIR) -f Kernel.mk deepclean

#.PHONY: kernel.elf
kernel-%:
#	export PATH=/root/xgcc/opt/cross/bin:$(PATH)
	@echo "MAKE KERNEL MACHDIR="$(MACHDIR)  "KERNEL="$@
#	@cd kernel; make ARCHDIR=$(ARCHDIR) MACHDIR=$(MACHDIR) -f Kernel.mk --no-print-directory kernel-elf
	@cd kernel; make ARCHDIR=$(ARCHDIR) MACHDIR=$(MACHDIR) -f Kernel.mk kernel-elf
	@echo "MAKE KERNEL "$@ " DONE!"
#	cp ./kernel/kernel.elf /srv/tftp/kernel.elf


setup:	386image

386image:
	mkdir -p /mnt/i386hd
	rm -f i386hd.img
	dd if=/dev/zero of=i386hd.img bs=512k count=100
	parted -s i386hd.img mklabel msdos mkpart primary fat32 1MB 100% set 1 boot on
	losetup -P /dev/loop43 i386hd.img
	mkfs.vfat -v -F32  /dev/loop43p1
	mount /dev/loop43p1 /mnt/i386hd/
	grub-install --target=i386-pc --boot-directory=/mnt/i386hd/boot /dev/loop43
	umount /mnt/i386hd
	losetup -d /dev/loop43


run-pcq35-dbg-image:
	mcopy -v -n -o -i i386hd.img@@1M ./boot/grub2/grub.cfg ::/boot/grub
	mcopy -v -n -o -i i386hd.img@@1M ./grub2-i386-modules/video_cirrus.mod ::/boot/grub/i386-pc
	mcopy -v -n -o -i i386hd.img@@1M ./grub2-i386-modules/video_fb.mod ::/boot/grub/i386-pc
	mcopy -v -n -o -i i386hd.img@@1M ./grub2-i386-modules/vbe.mod ::/boot/grub/i386-pc
	mcopy -v -n -o -i i386hd.img@@1M ./grub2-i386-modules/vga.mod ::/boot/grub/i386-pc
	mcopy -v -n -o -i i386hd.img@@1M ./kernel/build/QEMUQ35_64/kernel.elf ::/boot/kernel.elf
#	mcopy -v -n -o -i i386hd.img@@1M ./kernel/kernel.elf ::/boot/kernel.elf
#	qemu-system-x86_64 -M q35,smm=off  -smp 4 -m 32m -net none  -serial null -serial stdio -drive format=raw,file=i386hd.img  \
#-vga none  -device cirrus-vga

	qemu-system-x86_64 -M q35 -smp 4 -m 64m -net none  -serial stdio -drive format=raw,file=i386hd.img -vga none -device cirrus-vga \
-no-reboot -no-shutdown -S -gdb tcp::1234 #-singlestep #-S -gdb tcp::1234 -D ./qlog.txt



#-device nec-usb-xhci,id=usbxbus    \
#-device usb-kbd,bus=usbxbus.0,port=1 \
#-device usb-mouse,bus=usbxbus.0,port=2 \
#-device usb-mouse,bus=usbxbus.0,port=3 \
-no-reboot -no-shutdown -S -gdb tcp::1234 #-singlestep #-S -gdb tcp::1234 -D ./qlog.txt

run: run-pcq35-dbg-image