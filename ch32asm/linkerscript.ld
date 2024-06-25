OUTPUT_ARCH( "riscv" )
OUTPUT_FORMAT( "elf32-littleriscv" )
ENTRY(_start)

PHDRS {
    codeseg   PT_LOAD;
    rodataseg PT_LOAD;
    dataseg   PT_LOAD;
}

SECTIONS {
    /DISCARD/ : { *(.note.gnu.build-id) }
    
    . = 0x00000000;
    .text : { *(.text) } :codeseg
    .rodata : { *(.rodata) } :rodataseg
    .data : { *(.data) } :dataseg
    .bss : { *(.bss) } :NONE
}