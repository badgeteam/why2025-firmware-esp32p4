idf_component_register(
    SRCS
        "fonts/chakrapetchmedium.c"
        "menus/apps.c"
        "menus/root.c"
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
    EMBED_FILES
        "ch32_firmware.bin"
)

# target_compile_definitions(${COMPONENT_LIB} PRIVATE -DKBELF_DEBUG)
