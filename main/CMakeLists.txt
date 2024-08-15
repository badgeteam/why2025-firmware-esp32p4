file(GLOB_RECURSE xtulator_sources
    xtulator/*.c
    xtulator/chipset/*.c
    xtulator/cpu/cpu.c
    xtulator/modules/audio/*.c
    xtulator/modules/disk/*.c
    xtulator/modules/input/*.c
    xtulator/modules/io/*.c
    xtulator/modules/video/*.c
)


idf_component_register(
    SRCS "main.c" "display_test.c" "fonts/chakrapetchmedium.c" "techdemo.c" "rvswd.c" ${xtulator_sources}
    INCLUDE_DIRS
        "."
        xtulator
        xtulator/chipset
        xtulator/cpu
        xtulator/modules/audio
        xtulator/modules/disk
        xtulator/modules/input
        xtulator/modules/io
        xtulator/modules/video
    EMBED_FILES "ch32_firmware.bin" "splashscreen.png" "boot.snd"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=parentheses -Wno-error=maybe-uninitialized -Wno-error=unused-but-set-variable)