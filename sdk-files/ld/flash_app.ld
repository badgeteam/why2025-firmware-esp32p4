
/* WARNING: This license applies only to this file, not all of the firmware! */
/* SPDX-License-Identifier: CC0-1.0 */
/* Copyright © 2024, Badge.team */

OUTPUT_FORMAT("elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(main)

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
	dynseg    PT_DYNAMIC;
	dataseg   PT_LOAD;
}

SECTIONS
{
	/* Physical memory. */
	__start_xip    = 0x42000000;
	__stop_xip     = 0x44000000;
	__start_psram  = 0x48000000;
	__stop_psram   = 0x48020000;
	__start_sram   = 0x4FF00000;
	__stop_sram    = 0x4FFA0000;
	
	/* ESP image header. */
	. = __start_xip + 0x100;
	/* Code */
	.text BLOCK(0x100) : {
		*(.text) *(.text.*)
	} >xip :codeseg
	.plt : {
		*(.plt) *(.iplt)
	}
	
	/* Read-only data */
	.rodata : {
		*(.rodata) *(.rodata.*)
	} >xip :rodataseg
	/* Dynamic ELF information */
	.dynsym : {
		*(.dynsym)
	} >xip :rodataseg
	.dynstr : {
		*(.dynstr)
	} >xip :rodataseg
	.rela.dyn : {
		*(.rela.dyn)
	} >xip :rodataseg
	.hash : {
		*(.rela.dyn)
	} >xip :rodataseg
	
	/* Global offset tables */
	. = __start_psram;
	.got BLOCK(0x100) : {
		*(.got.plt) *(.igot.plt) *(.got) *(.igot)
	} >psram :dataseg
	
	/* Read/write data */
	.data : {
		*(.data) *(.data.*)
	} >psram :dataseg
	.dynamic : {
		*(.dynamic) *(.dynamic.*)
	} >psram :dataseg :dynseg
	.sdata : {
		*(.sdata) *(.sdata.*)
		*(.srodata) *(.srodata.*)
	} >psram :dataseg
	
	/* Zero-initialized */
	.sbss : {
		*(.sbss) *(.sbss.*)
	} >psram :dataseg
	.bss : {
		*(.bss) *(.bss.*) *(COMMON)
	} >psram :dataseg
	
	/* Uninitialized */
	.noinit : {
		*(.noinit) *(.noinit.*)
	} >psram :NONE
}
