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

main/kbelf_lib.c: tools/exported.txt tools/symbol_export.py
	mkdir -p build
	./tools/symbol_export.py \
		--symbols tools/exported.txt \
		--kbelf main/kbelf_lib.c \
		--kbelf-path libbadge.so

badgesdk: $(shell find -name '*.h' -or -name '*.hpp')
	rm -rf badgesdk
	cp -r sdk-files badgesdk
	mkdir -p badgesdk/include
	cp -r components/badge-bsp/pub_include/* badgesdk/include/
	cp -r components/badge-bsp/badgelib/include/* badgesdk/include/
	cp -r components/pax_gfx/core/include/* badgesdk/include/
	cp -r components/pax_gfx/gui/include/* badgesdk/include/
	source "$(IDF_PATH)/export.sh" && \
	./tools/symbol_export.py \
		--symbols tools/exported.txt \
		--assembler riscv32-esp-elf-gcc \
		--lib badgesdk/ld/libbadge.so \
		-F=-mabi=ilp32f -F=-march=rv32imafc

.PHONY: build
build: main/kbelf_lib.c
	source "$(IDF_PATH)/export.sh" >/dev/null && idf.py build

.PHONY: image
image:
	echo TODO


# Hardware

.PHONY: flash
flash: build
	source "$(IDF_PATH)/export.sh" && \
	idf.py flash -p $(PORT)

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
