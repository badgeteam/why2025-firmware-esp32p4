#!/usr/bin/env python3

# SPDX-License-Identifier: MIT

import argparse, subprocess
from tempfile import NamedTemporaryFile as TmpFile
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import *



def load_symbols(path: str) -> list[tuple[str,str]]:
    fd = open(path, "r")
    lines = fd.readlines()
    fd.close()
    out = []
    for line in lines:
        line = line.strip()
        if line.startswith("#") or not len(line): continue
        if '=' in line:
            out.append((line[:line.index('=')].strip(), line[line.index('=')+1:].strip))
        else:
            out.append((line, line))
    return out

def gen_kbelf_lib(symbols: list[tuple[str,str]], path: str, id: str, kbelf_path: str):
    fd = open(path, "w")
    fd.write("// WARNING: This is a generated file, do not edit it!\n")
    fd.write("// clang-format off\n")
    fd.write("\n")
    fd.write("#include <kbelf.h>\n")
    fd.write("\n")
    for sym in symbols:
        fd.write(f"extern char const symbol_{sym[0]}[] asm(\"{sym[0]}\");\n")
    fd.write("\n")
    fd.write("static kbelf_builtin_sym const symbols[] = {\n")
    for sym in symbols:
        fd.write(f"    {{ .name = \"{sym[1]}\", .vaddr = (size_t) symbol_{sym[0]} }},\n")
    fd.write("};\n")
    fd.write("\n")
    fd.write(f"kbelf_builtin_lib const {id} = {{\n")
    fd.write(f"    .path        = \"{kbelf_path}\",\n")
    fd.write(f"    .symbols_len = {len(symbols)},\n")
    fd.write(f"    .symbols     = symbols,\n")
    fd.write("};\n")
    fd.close()

def asm_lib(symbols: list[tuple[str,str]], out_path: str, assembler: str, asm_flag: list[str]):
    global ldscript
    
    ldscript = TmpFile("w+", suffix=".ld")
    ldscript.write(
    """
    PHDRS {
        code PT_LOAD;
    }

    SECTIONS {
        .text : {
            KEEP(*(.text));
        } :code
    }
    """
    )
    ldscript.flush()
    
    with TmpFile("w+", suffix=".S") as asm:
        asm.write('.text\n')
        asm.write('.option norelax\n')
        asm.write('.option norvc\n')
        for i in range(len(symbols)):
            symbol = symbols[i]
            asm.write(f'.global {symbol[0]}\n')
            asm.write(f'.type {symbol[0]}, %function\n')
            asm.write(f'{symbol[0]}: // {symbol[1]}\n')
            asm.write(f'nop\n')
        asm.flush()
        subprocess.run([
            assembler,
            *asm_flag,
            '-T' + ldscript.name,
            '-nostartfiles', '-nodefaultlibs',
            '-Wl,--build-id=none',
            '-Wl,--no-gc-sections',
            '-shared',
            asm.name,
            '-o', out_path
        ])

def gen_cmake(symbols: list[tuple[str,str]], path: str):
    fd = open(path, "w")
    fd.write("# WARNING: This is a generated file, do not edit it!\n")
    fd.write("add_link_options(\n")
    for sym in symbols:
        fd.write("-Wl,--require-defined={}\n".format(sym[0]))
    fd.write(")\n")
    fd.close()

def get_sym_addr(symtab: SymbolTableSection, symbol_name: str) -> int:
    symbols = symtab.get_symbol_by_name(symbol_name)
    if symbols == None:
        raise LookupError(f"Symbol `{symbol_name}` not found")
    for symbol in symbols:
        if symbol.entry.st_info.bind == "STB_GLOBAL" and symbol.entry.st_shndx == "SHN_ABS":
            return symbol.entry.st_value
        elif symbol.entry.st_info.bind == "STB_GLOBAL" and symbol.entry.st_shndx != 0 and type(symbol.entry.st_shndx) == int:
            return symbol.entry.st_value
        else:
            print(f"Warning: Symbol `{symbol_name}` found but not usable")
            print(f"st_bind: {symbol.entry.st_info.bind}, st_shndx: {symbol.entry.st_shndx}")
    raise LookupError(f"Symbol `{symbol_name}` not found")

def asm_table(symbols: list[tuple[str,str]], elf_path: str, out_path: str, assembler: str, asm_flag: list[str], address: int):
    global ldscript
        
    ldscript = TmpFile("w+", suffix=".ld")
    ldscript.write(
    """
    PHDRS {
        code PT_LOAD;
    }

    SECTIONS {
        . = {};
        .text : {
            KEEP(*(.text));
        } :code
    }
    """.replace('{}', hex(address))
    )
    ldscript.flush()
    
    with open(elf_path, "rb") as elffd:
        elf_file = ELFFile(elffd)
        with TmpFile("w+", suffix=".S") as asm:
            asm.write('.text\n')
            asm.write('.option norelax\n')
            asm.write('.option norvc\n')
            symtab = elf_file.get_section_by_name(".symtab")
            addrs = [get_sym_addr(symtab, x[1]) for x in symbols]
            for i in range(len(symbols)):
                symbol = symbols[i]
                os_vaddr = addrs[i]
                os_vaddr_lo12 = os_vaddr & 0xfff
                os_vaddr_hi20 = os_vaddr >> 12
                if os_vaddr_lo12 & 0x800:
                    os_vaddr_lo12 |= 0xfffff000
                    os_vaddr_hi20 += 1
                asm.write(f'.global {symbol[0]}\n')
                asm.write(f'.type {symbol[0]}, %function\n')
                asm.write(f'.size {symbol[0]}, 8\n')
                asm.write(f'{symbol[0]}: // {symbol[1]} @ {hex(os_vaddr)}\n')
                asm.write(f'lui t0, {hex(os_vaddr_hi20)}\n')
                asm.write(f'jr {hex(os_vaddr_lo12)}(t0)\n')
            asm.flush()
            subprocess.run([
                assembler,
                *asm_flag,
                '-T' + ldscript.name,
                '-nostartfiles', '-nodefaultlibs',
                '-Wl,--build-id=none',
                '-Wl,--no-gc-sections',
                asm.name,
                '-o', out_path
            ])

def gen_ldscript(symbols: list[tuple[str,str]], path: str, address: int):
    fd = open(path, "w")
    fd.write("/* WARNING: This is a generated file, do not edit it! */\n")
    for i in range(len(symbols)):
        sym = symbols[i]
        fd.write("PROVIDE({} = 0x{:08x});\n".format(sym[0], i*8+address))
    fd.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--symbols",    action="store", required=True)
    parser.add_argument("--binary",     action="store")
    parser.add_argument("--cmake",      action="store")
    parser.add_argument("--table",      action="store")
    parser.add_argument("--ldscript",   action="store")
    parser.add_argument("--assembler",  action="store")
    parser.add_argument("--address",    action="store")
    parser.add_argument("--kbelf",      action="store")
    parser.add_argument("--kbelf-id",   action="store", default="exported_syms")
    parser.add_argument("--kbelf-path", action="store", default=None)
    parser.add_argument("--lib",        action="store")
    parser.add_argument("--asm-flag", "-F", action="append")
    args = parser.parse_args()
    symbols = load_symbols(args.symbols)
    if args.kbelf:
        gen_kbelf_lib(symbols, args.kbelf, args.kbelf_id, args.kbelf_path or f"lib{args.kbelf_id}.so")
    if args.lib:
        asm_lib(symbols, args.lib, args.assembler, args.asm_flag)
    if args.cmake:
        gen_cmake(symbols, args.cmake)
    if args.ldscript:
        assert len(args.address)
        gen_ldscript(symbols, args.ldscript, int(args.address, 16))
    if args.table:
        assert len(args.assembler)
        assert len(args.binary)
        assert len(args.address)
        asm_table(symbols, args.binary, args.table, args.assembler, args.asm_flag, int(args.address, 16))
