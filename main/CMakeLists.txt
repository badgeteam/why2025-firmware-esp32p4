idf_component_register(
    SRCS
        "fonts/chakrapetchmedium.c"
        "menus/apps.c"
        "menus/root.c"
        "app.c"
        "appfs_elf.c"
        "main.c"
        "kbelfx.c"
    INCLUDE_DIRS
        "."
    EMBED_FILES
        "ch32_firmware.bin"
)

add_subdirectory(kbelf)
target_link_libraries(${COMPONENT_LIB} PRIVATE kbelf)
