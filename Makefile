PORT ?= $(shell find /dev -name 'ttyACM*' -or -name 'ttyUSB*' 2>/dev/null)
BUILDDIR ?= build

IDF_PATH ?= $(shell pwd)/esp-idf
IDF_TOOLS_PATH ?= $(shell pwd)/esp-idf-tools
IDF_BRANCH ?= release/v5.3
#IDF_COMMIT ?= c57b352725ab36f007850d42578d2c7bc858ed47
IDF_EXPORT_QUIET ?= 1
IDF_GITHUB_ASSETS ?= dl.espressif.com/github_assets

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
	git clone --recursive --branch "$(IDF_BRANCH)" https://github.com/espressif/esp-idf.git "$(IDF_PATH)"
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
	rm -rf "$(BUILDDIR)"

.PHONY: fullclean
fullclean:
	source "$(IDF_PATH)/export.sh" && idf.py fullclean

# Building

.PHONY: build
build:
	source "$(IDF_PATH)/export.sh" && idf.py build

.PHONY: image
image:
	echo TODO


# Hardware

.PHONY: flash
flash: build
	source "$(IDF_PATH)/export.sh" && idf.py flash -p $(PORT)

.PHONY: erase
erase:
	source "$(IDF_PATH)/export.sh" && idf.py erase-flash -p $(PORT)

.PHONY: monitor
monitor:
	source "$(IDF_PATH)/export.sh" && idf.py monitor -p $(PORT)

.PHONY: openocd
openocd:
	source "$(IDF_PATH)/export.sh" && idf.py openocd

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

# Efuse

.PHONY: efuse-summary
efuse-summary:
	source "$(IDF_PATH)/export.sh" && espefuse.py --port $(PORT) --baud 115200 --chip esp32p4 --before no_reset summary

.PHONY: efuse-burn-3-volt-flash
efuse-burn-3-volt-flash:
	source "$(IDF_PATH)/export.sh" && espefuse.py --port $(PORT) --baud 115200 --chip esp32p4 --before no_reset burn_efuse PXA0_TIEH_SEL_0 2

.PHONY: efuse-burn-swap-usb-device-pins
efuse-burn-swap-usb-device-pins:
	source "$(IDF_PATH)/export.sh" && espefuse.py --port $(PORT) --baud 115200 --chip esp32p4 --before no_reset burn_efuse USB_DEVICE_EXCHG_PINS 1

.PHONY: efuse-burn-prototype1
efuse-burn-prototype1: efuse-burn-3-volt-flash efuse-burn-swap-usb-device-pins
