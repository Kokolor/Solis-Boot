gcc -ffreestanding -fshort-wchar -c kernel.c -o kernel.o
ld -T linker.ld -static -Bsymbolic -nostdlib -o kernel.elf kernel.o