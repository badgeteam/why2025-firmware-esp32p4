
set(srcs
    src/bsp/input_gpio.c
    src/bsp_color.c
    src/bsp_device.c
    src/bsp_event.c
    src/bsp_keymap.c
    src/bsp_pax.c
    src/bsp.c
)
set(deps driver fatfs)

if(CONFIG_BSP_PAX_INTEGRATION)
    set(srcs ${srcs} src/bsp_pax.c)
endif()

if(CONFIG_BSP_SUPPORT_MIPI_DSI)
    set(srcs ${srcs} src/bsp/disp_mipi_dsi.c)
endif()
if(CONFIG_BSP_SUPPORT_ST7701)
    set(srcs ${srcs} src/bsp/disp_st7701.c)
endif()
if(CONFIG_BSP_SUPPORT_EK79007)
    set(srcs ${srcs} src/bsp/disp_ek79007.c)
endif()
if(CONFIG_BSP_SUPPORT_WHY2025_COPROC)
    set(srcs ${srcs} src/bsp/why2025_coproc.c)
endif()

if(CONFIG_BSP_PLATFORM_WHY2025)
    set(srcs ${srcs} src/init/why2025.c)
elseif(CONFIG_BSP_PLATFORM_P4DEVKIT01 OR CONFIG_BSP_PLATFORM_P4DEVKIT01_ST7701)
    set(srcs ${srcs} src/init/p4devkit.c)
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS
        pub_include
        include
    REQUIRES esp_lcd esp_lcd_ek79007 pax_gfx driver fatfs
)

add_subdirectory(badgelib)
target_link_libraries(${COMPONENT_LIB} PUBLIC badgelib)
