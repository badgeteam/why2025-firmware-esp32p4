
idf_component_register(
    SRCS
        src/bsp/disp_st7701.c
        src/bsp/disp_mipi_dsi.c
        src/bsp/input_gpio.c
        src/bsp/why2025_coproc.c
        src/init/why2025.c
        src/bsp_color.c
        src/bsp_device.c
        src/bsp_keymap.c
        src/bsp_pax.c
        src/bsp.c
    INCLUDE_DIRS
        pub_include
        include
    REQUIRES pax-gfx driver esp_lcd
)
