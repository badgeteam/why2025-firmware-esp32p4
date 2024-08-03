
/* SPDX-License-Identifier: MIT */

OUTPUT_FORMAT("elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
	xip    (RX)  : ORIGIN = 0x42000000, LENGTH = 32M
	psram  (RWX) : ORIGIN = 0x48000000, LENGTH = 64M
	sram   (RWX) : ORIGIN = 0x4FF00000, LENGTH = 640K
}

PHDRS
{
	codeseg   PT_LOAD;
	rodataseg PT_LOAD;
	dataseg   PT_LOAD;
}

SECTIONS
{
	/DISCARD/ : { *(.note) *(.note.*) *(.eh_frame) *(.eh_frame.*) }

	/* Import jump table addresses. */
	INCLUDE badge_jump_table.ld

	/* Physical memory. */
	__start_xip    = 0x42000000;
	__stop_xip     = 0x44000000;
	__start_psram  = 0x48000000;
	__stop_psram   = 0x48020000;
	__start_sram   = 0x4FF00000;
	__stop_sram    = 0x4FFA0000;
	
	__section_alignment = 16;
	
	/* ESP image header. */
	. = __start_xip;
	.esphdr : AT(0) {
		BYTE(0xE9);			/* Magic byte. */
		BYTE(3);			/* Segment count. */
		BYTE(0x02);			/* SPI mode. */
		BYTE(0x1f);			/* SPI speed/size. */
		LONG(_start);		/* Entrypoint. */
		BYTE(0xee);			/* WP pin state. */
		BYTE(0x00);			/* Drive settings. */
		BYTE(0x00);
		BYTE(0x00);
		SHORT(0x0012);		/* Chip (ESP32-P4). */
		BYTE(0x00);			/* (deprecated) */
		SHORT(0x0000);		/* Min chip rev. */
		SHORT(0xffff);		/* Max chip rev. */
		LONG(0x00000000);	/* (reserved) */
		BYTE(0x00);			/* SHA256 appended (not appended). */
	} >xip :codeseg
	
	/* ESP image segment 0: code. */
	.espseg.0 : AT(LOADADDR(.esphdr) + SIZEOF(.esphdr)) {
		LONG(__start_text);
		LONG(__stop_text - __start_text);
	} >xip :codeseg
	
	__start_text = .;
	.text : AT(LOADADDR(.espseg.0) + SIZEOF(.espseg.0)) {
		*(.text) *(.text.*)
		. = ALIGN(4);
	} >xip :codeseg
	__stop_text = .;
	
	/* ESP image segment 1: rodata. */
	.espseg.1 : AT(LOADADDR(.text) + SIZEOF(.text)) {
		LONG(__start_rodata);
		LONG(__stop_rodata - __start_rodata);
	} >xip :codeseg
	
	__start_rodata = .;
	.got : AT(LOADADDR(.espseg.1) + SIZEOF(.espseg.1)) {
		*(.got.plt) *(.igot.plt) *(.got) *(.igot)
	} >xip :rodataseg
	.plt : AT(LOADADDR(.got) + SIZEOF(.got)) {
		*(.plt) *(.iplt)
	} >xip :rodataseg
	.rodata : AT(LOADADDR(.plt) + SIZEOF(.plt)) {
		. = ALIGN(__section_alignment);
		*(.rodata) *(.rodata.*)
		. = ALIGN(4);
	} >xip :rodataseg
	__stop_rodata = .;
	
	/* ESP image segment 2: data. */
	.espseg.2 : AT(LOADADDR(.rodata) + SIZEOF(.rodata)) {
		LONG(__start_data);
		LONG(__stop_data - __start_data);
	} >xip :rodataseg
	
	. = __start_psram;
	__start_data = .;
	.data : AT(LOADADDR(.espseg.2) + SIZEOF(.espseg.2)) {
		. = ALIGN(__section_alignment);
		*(.data) *(.data.*)
		. = ALIGN(__section_alignment);
	} >psram :dataseg
	__start_ss = .;
	.sdata : AT(LOADADDR(.data) + SIZEOF(.data)) {
		*(.sdata) *(.sdata.*)
		*(.srodata) *(.srodata.*)
		. = ALIGN(4);
	} >psram :dataseg
	__stop_data = .;
	
	/* Zero-initialized */
	__start_bss = .;
	.sbss : {
		. = ALIGN(__section_alignment);
		*(.sbss) *(.sbss.*)
	} >psram :NONE
	__stop_ss = .;
	.bss : {
		. = ALIGN(__section_alignment);
		*(.bss) *(.bss.*) *(COMMON)
	} >psram :NONE
	__stop_bss = .;
	/* PROVIDE( __global_pointer$ = (__stop_ss + __start_ss) / 2 ); */
	
	/* Uninitialized */
	. = ALIGN(__section_alignment);
	__start_noinit = .;
	.noinit : {
		*(.noinit) *(.noinit.*)
	} >psram :NONE
	__stop_noinit = .;
}
