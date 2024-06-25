idf_component_register(
    SRCS "main.c" "display_test.c" "fonts/chakrapetchmedium.c" "techdemo.c" "rvswd.c"
    INCLUDE_DIRS "."
    EMBED_FILES "ch32_firmware.bin"
)
