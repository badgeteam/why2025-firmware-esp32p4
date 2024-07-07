#!/usr/bin/env python3

# SPDX-License-Identifier: MIT

import sys
from elftools.elf.elffile import ELFFile
from ar import Archive

assert(__name__) == "__main__"

if len(sys.argv) != 2:
    print("Usage: symbol_list.py [file.elf]")
    exit(1)

def dump_elf_syms(fd):
    file   = ELFFile(fd)
    symtab = file.get_section_by_name(".symtab")
    assert(symtab)
    for i in range(1, symtab.num_symbols()):
        sym = symtab.get_symbol(i)
        if sym.entry.st_info.bind == "STB_GLOBAL" and (sym.entry.st_shndx == "SHN_ABS" or (sym.entry.st_shndx != 0 and type(sym.entry.st_shndx) == int)):
            print(sym.name)

fd = open(sys.argv[1], "rb")
try:
    archive = Archive(fd)
    for entry in archive.entries:
        fd0 = archive.open(entry, "rb")
        dump_elf_syms(fd0)
        fd0.close()
except:
    dump_elf_syms(fd)
