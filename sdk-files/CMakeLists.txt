
# WARNING: This license applies only to this file, not all of the firmware!
# SPDX-License-Identifier: CC0-1.0
# Copyright © 2024, Badge.team

if(DEFINED linkage)
    set(badge_linkerscript ${linkage}.ld)
else()
    set(badge_linkerscript flash_app.ld)
endif()

set(badge_library_path ${CMAKE_CURRENT_LIST_DIR}/ld/)
add_compile_options(
    -march=rv32imafc -mabi=ilp32f
    -mno-relax
    -shared -fPIC
)
add_link_options(
    -nostartfiles -nodefaultlibs
    -L ${badge_library_path}
    -T ${badge_linkerscript}
    -l badge
    -shared
    -z max-page-size=256
)
