cd ../src/

gcc -c main.c -o main.o \
-fno-stack-protector \
-fpic \
-fshort-wchar \
-mno-red-zone \
-I /usr/lib/headers \
-I /usr/include/efi \
-I /usr/lib/gnu-efi \
-I /usr/lib/ \
-I /usr/lib/headers/x86_64 \
-DEFI_FUNCTION_WRAPPER

ld main.o \
/usr/lib/crt0-efi-x86_64.o \
-nostdlib \
-znocombreloc \
-T /usr/lib/elf_x86_64_efi.lds \
-shared \
-Bsymbolic \
-L /usr/lib \
-l:libgnuefi.a \
-l:libefi.a \
-o main.so

objcopy -j .text \
-j .sdata \
-j .data \
-j .dynamic \
-j .dynsym \
-j .rel \
-j .rela \
-j .reloc \
--target=efi-app-x86_64 \
main.so \
main.efi

mkdir mnt
qemu-img create solis.img 4G
mkfs.fat -n 'DISK' -s 2 -f 2 -R 32 solis.img
sudo losetup -P /dev/loop0 solis.img
sudo mount /dev/loop0 mnt/
sudo mkdir mnt/efi/
sudo mkdir mnt/efi/boot/
sudo cp main.efi mnt/efi/boot/bootx64.efi
sudo cp zap-light16.psf mnt/
sudo cp ../bootloaderData/bootconfig.sboot mnt/
cd ../kernel/
./build.sh
cd ../src/
sudo cp ../kernel/kernel.elf mnt/
sudo umount mnt/
sudo losetup -d /dev/loop0
qemu-system-x86_64 -machine q35 -drive if=pflash,format=raw,readonly,file=/usr/share/edk2/x64/OVMF.fd -drive format=raw,file=solis.img
rm solis.img
rmdir mnt
rm main.so main.o main.efi ../kernel/kernel.o ../kernel/kernel.elf