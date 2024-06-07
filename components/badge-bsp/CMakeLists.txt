
idf_component_register(
    SRCS
        src/driver/why2025_coproc.c
        src/init/why2025.c
        src/bsp_keymap.c
        src/bsp.c
    INCLUDE_DIRS
        include
    REQUIRES pax-gfx driver
)
