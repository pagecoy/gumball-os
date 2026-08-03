#!/bin/bash
set -e

echo "Assembling boot/boot.s..."
gcc -m32 -c boot/boot.s -o boot.o

echo "Compiling kernel/arch/gdt.c..."
gcc -m32 -c kernel/arch/gdt.c -o gdt.o -std=gnu99 -ffreestanding -fno-stack-protector -fno-pic -O2 -Wall -Wextra -Ikernel

echo "Compiling kernel/drivers/term.c..."
gcc -m32 -c kernel/drivers/term.c -o term.o -std=gnu99 -ffreestanding -fno-stack-protector -fno-pic -O2 -Wall -Wextra -Ikernel

echo "Compiling kernel/arch/idt.c..."
gcc -m32 -c kernel/arch/idt.c -o idt.o -std=gnu99 -ffreestanding -fno-stack-protector -fno-pic -O2 -Wall -Wextra -Ikernel

echo "Compiling kernel/drivers/keyboard.c..."
gcc -m32 -c kernel/drivers/keyboard.c -o keyboard.o -std=gnu99 -ffreestanding -fno-stack-protector -fno-pic -O2 -Wall -Wextra -Ikernel

echo "Compiling kernel/shell/shell.c..."
gcc -m32 -c kernel/shell/shell.c -o shell.o -std=gnu99 -ffreestanding -fno-stack-protector -fno-pic -O2 -Wall -Wextra -Ikernel

echo "Compiling kernel/fs/fs.c..."
gcc -m32 -c kernel/fs/fs.c -o fs.o -std=gnu99 -ffreestanding -fno-stack-protector -fno-pic -O2 -Wall -Wextra -Ikernel

echo "Compiling kernel/kernel.c..."
gcc -m32 -c kernel/kernel.c -o kernel.o -std=gnu99 -ffreestanding -fno-stack-protector -fno-pic -O2 -Wall -Wextra -Ikernel

echo "Linking..."
ld -m elf_i386 -T linker.ld -o gumball.bin boot.o kernel.o term.o idt.o keyboard.o gdt.o shell.o fs.o -nostdlib

echo "Verifying multiboot..."
grub-file --is-x86-multiboot gumball.bin && echo "  -> valid"

if [ ! -f disk.img ]; then
    echo "Creating 1MB dummy disk.img..."
    dd if=/dev/zero of=disk.img bs=1024 count=1024
fi

echo "Building bootable ISO..."
mkdir -p isodir/boot/grub
cp gumball.bin isodir/boot/gumball.bin
cp disk.img isodir/boot/disk.img

cat > isodir/boot/grub/grub.cfg << 'GRUBCFG'
menuentry "Gumball" {
    multiboot /boot/gumball.bin
    module /boot/disk.img
}
GRUBCFG
grub-mkrescue -o gumball.iso isodir

echo ""
echo "Done. gumball.iso is ready."
echo "Test it:   qemu-system-i386 -cdrom gumball.iso"