idf_component_register(
    SRCS
        "fonts/chakrapetchmedium.c"
        "app.c"
        "appelf.c"
        "main.c"
        "kbelfx.c"
        "kbelf_lib.c"
        "kbelf/src/port/riscv.c"
        "kbelf/src/kbelf_dyn.c"
        "kbelf/src/kbelf_file.c"
        "kbelf/src/kbelf_inst.c"
        "kbelf/src/kbelf_reloc.c"
        "kbelf/src/kbelf.c"
    INCLUDE_DIRS
        "."
        "kbelf/include"
)

# target_compile_definitions(${COMPONENT_LIB} PRIVATE -DKBELF_DEBUG)
