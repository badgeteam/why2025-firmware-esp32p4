# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "0.0.1")


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if(NOT EXISTS build/badge_export_symbols.cmake)
    execute_process(COMMAND make -C ${CMAKE_CURRENT_LIST_DIR} build/badge_export_symbols.cmake)
endif()
include(build/badge_export_symbols.cmake)
project(why2025-firmware-esp32p4)
