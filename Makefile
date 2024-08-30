PORT ?= /dev/ttyACM0

IDF_PATH ?= $(shell cat .IDF_PATH 2>/dev/null || echo `pwd`/esp-idf)
IDF_TOOLS_PATH ?= $(shell cat .IDF_TOOLS_PATH 2>/dev/null || echo `pwd`/esp-idf-tools)
IDF_BRANCH ?= release/v5.3
#IDF_COMMIT ?= c57b352725ab36f007850d42578d2c7bc858ed47
IDF_EXPORT_QUIET ?= 1
IDF_GITHUB_ASSETS ?= dl.espressif.com/github_assets
MAKEFLAGS += --silent

SHELL := /usr/bin/env bash

export IDF_TOOLS_PATH
export IDF_GITHUB_ASSETS

# General targets

.PHONY: all
all: build flash

.PHONY: install
install: flash

# Preparation

.PHONY: prepare
prepare: submodules sdk

.PHONY: submodules
submodules:
	git submodule update --init --recursive

.PHONY: sdk
sdk:
	rm -rf "$(IDF_PATH)"
	rm -rf "$(IDF_TOOLS_PATH)"
	git clone --recursive --branch "$(IDF_BRANCH)" https://github.com/espressif/esp-idf.git "$(IDF_PATH)" --depth=1 --shallow-submodules
	#cd "$(IDF_PATH)"; git checkout "$(IDF_COMMIT)"
	cd "$(IDF_PATH)"; git submodule update --init --recursive
	cd "$(IDF_PATH)"; bash install.sh all
	source "$(IDF_PATH)/export.sh" && idf.py --preview set-target esp32p4
	git checkout sdkconfig

.PHONY: menuconfig
menuconfig:
	source "$(IDF_PATH)/export.sh" && idf.py menuconfig

# Cleaning

.PHONY: clean
clean:
	rm -rf "build"

.PHONY: fullclean
fullclean:
	source "$(IDF_PATH)/export.sh" && idf.py fullclean

# Building

build/badge_export_symbols.cmake: tools/exported.txt tools/symbol_export.py
	mkdir -p build
	./tools/symbol_export.py --address 0x43F80000 \
	    --symbols tools/exported.txt \
		--cmake build/badge_export_symbols.cmake \
		--ldscript build/badge_jump_table.ld

badgesdk: build/badge_export_symbols.cmake $(PAX_HDRS) $(shell find -name '*.h' -or -name '*.hpp')
	rm -rf badgesdk/include
	mkdir -p badgesdk/ld
	mkdir -p badgesdk/include/gui
	cp components/badge-bsp/pub_include/* badgesdk/include/
	cp components/pax-gfx/core/include/* badgesdk/include/
	cp components/pax-gfx/gui/include/* badgesdk/include/
	cp build/badge_jump_table.ld badgesdk/ld/			
	cp tools/badgesdk.ld badgesdk/ld/linker.ld

.PHONY: build
build:
	source "$(IDF_PATH)/export.sh" >/dev/null && \
	idf.py build && \
	echo Building jump tables && \
	./tools/symbol_export.py --address 0x43F80000 \
		--symbols tools/exported.txt --binary build/why2025-firmware-esp32p4.elf \
		--table build/badge_jump_table.elf --assembler riscv32-esp-elf-gcc && \
	riscv32-esp-elf-objcopy -O binary build/badge_jump_table.elf build/badge_jump_table.bin

.PHONY: image
image:
	echo TODO


# Hardware

.PHONY: flash
flash: build
	source "$(IDF_PATH)/export.sh" && \
	idf.py flash -p $(PORT) && \
	esptool.py \
		-b 921600 --port $(PORT) \
		write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
		0x210000 build/badge_jump_table.bin

.PHONY: appfs
appfs:
	source "$(IDF_PATH)/export.sh" && \
	esptool.py \
		-b 921600 --port $(PORT) \
		write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
		0x110000 appfs.bin

.PHONY: erase
erase:
	source "$(IDF_PATH)/export.sh" && idf.py erase-flash -p $(PORT)

.PHONY: monitor
monitor:
	source "$(IDF_PATH)/export.sh" && idf.py monitor -p $(PORT)

.PHONY: openocd
openocd:
	source "$(IDF_PATH)/export.sh" && idf.py openocd

.PHONY: gdb
gdb:
	source "$(IDF_PATH)/export.sh" && idf.py gdb

# Tools

.PHONY: size
size:
	source "$(IDF_PATH)/export.sh" && idf.py size

.PHONY: size-components
size-components:
	source "$(IDF_PATH)/export.sh" && idf.py size-components

.PHONY: size-files
size-files:
	source "$(IDF_PATH)/export.sh" && idf.py size-files

# Formatting

.PHONY: format
format:
	find main/ -iname '*.h' -o -iname '*.c' -o -iname '*.cpp' | xargs clang-format -i
