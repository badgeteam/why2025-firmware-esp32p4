#/usr/bin/env bash

set -e
set -u

mkdir -p build; cd build

riscv32-unknown-elf-gcc -T ../linkerscript.ld -march=rv32ec -mabi=ilp32e -nostdlib -nodefaultlibs -nostartfiles ../ch32_readmem.S -o ch32_readmem.elf
riscv32-unknown-elf-objcopy -O binary ch32_readmem.elf ch32_readmem.bin

riscv32-unknown-elf-gcc -T ../linkerscript.ld -march=rv32ec -mabi=ilp32e -nostdlib -nodefaultlibs -nostartfiles ../ch32_writemem.S -o ch32_writemem.elf
riscv32-unknown-elf-objcopy -O binary ch32_writemem.elf ch32_writemem.bin

echo "// WARNING: This is a generated file" > ch32_gadgets.c
echo "// clang-format off" >> ch32_gadgets.c
echo "#include <stdint.h>" >> ch32_gadgets.c
echo "#include <stddef.h>" >> ch32_gadgets.c
echo "const uint8_t ch32_readmem[] = {" >> ch32_gadgets.c
xxd -i ch32_readmem.bin | tail -n +2 | head -n +1 >> ch32_gadgets.c
echo "};" >> ch32_gadgets.c
echo "const uint8_t ch32_writemem[] = {" >> ch32_gadgets.c
xxd -i ch32_writemem.bin | tail -n +2 | head -n +1 >> ch32_gadgets.c
echo "};" >> ch32_gadgets.c
