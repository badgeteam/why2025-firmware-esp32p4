/*
  XTulator: A portable, open-source 80186 PC emulator.
  Copyright (C)2020 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "config.h"
#include "cpu/cpu.h"
#include "modules/video/cga.h"
#include "modules/video/vga.h"
#include "utility.h"
#include "memory.h"
#include <stdbool.h>
#include <inttypes.h>

#define MAX_MEMORY_AREAS 128

typedef void (*memory_write_callback_t)(void* udata, uint32_t addr, uint8_t value);
typedef uint8_t (*memory_read_callback_t)(void* udata, uint32_t addr);

typedef struct memory_area {
	bool active;
	uint32_t start_address;
	uint32_t end_address;
	uint8_t* write_pointer;
	uint8_t* read_pointer;
	memory_write_callback_t write_callback;
	memory_read_callback_t read_callback;
	void* udata;
} memory_area_t;

static memory_area_t memory_map[MAX_MEMORY_AREAS] = {0};

void cpu_write(CPU_t* cpu, uint32_t addr32, uint8_t value) {
	addr32 &= MEMORY_MASK;

	for (uint32_t index = 0; index < MAX_MEMORY_AREAS; index++) {
		if (memory_map[index].active == false) {
			break;
		}
		if (addr32 >= memory_map[index].start_address && addr32 < memory_map[index].end_address) {
			uint32_t offset = addr32 - memory_map[index].start_address;
			if (memory_map[index].write_pointer != NULL) {
				memory_map[index].write_pointer[offset] = value;
			} else if (memory_map[index].write_callback != NULL) {
				memory_map[index].write_callback(memory_map[index].udata, addr32, value);
			} else {
				printf("Write to unhandled memory 0x%" PRIx32 "\r\n", addr32);
			}
			return;
		}
	}

	printf("Write to unmapped memory 0x%" PRIx32 "\r\n", addr32);
}

uint8_t cpu_read(CPU_t* cpu, uint32_t addr32) {
	addr32 &= MEMORY_MASK;

	for (uint32_t index = 0; index < MAX_MEMORY_AREAS; index++) {
		if (memory_map[index].active == false) {
			break;
		}
		if (addr32 >= memory_map[index].start_address && addr32 < memory_map[index].end_address) {
			if (memory_map[index].read_pointer != NULL) {
				uint32_t offset = addr32 - memory_map[index].start_address;
				return memory_map[index].read_pointer[offset];
			} else if (memory_map[index].read_callback != NULL) {
				return memory_map[index].read_callback(memory_map[index].udata, addr32);
			} else {
				printf("Read from unhandled memory 0x%" PRIx32 "\r\n", addr32);
			}
			break;
		}
	}

	printf("Read from unmapped memory 0x%" PRIx32 "\r\n", addr32);
	return 0xFF;
}

void memory_mapRegister(uint32_t start, uint32_t count, uint8_t* readb, uint8_t* writeb) {
	bool success = false;
	for (uint32_t index = 0; index < MAX_MEMORY_AREAS; index++) {
		if (memory_map[index].active) {
			continue;
		}
		success = true;
		memory_map[index].active = true;
		memory_map[index].start_address = start;
		memory_map[index].end_address = start + count;
		memory_map[index].write_pointer = writeb;
		memory_map[index].read_pointer = readb;
		memory_map[index].write_callback = NULL;
		memory_map[index].read_callback = NULL;
		memory_map[index].udata = NULL;
		break;
	}
	if (!success) {
		printf("ERROR: could not register area in memory map\r\n");
	}
}

void memory_mapCallbackRegister(uint32_t start, uint32_t count, uint8_t(*readb)(void*, uint32_t), void (*writeb)(void*, uint32_t, uint8_t), void* udata) {
	bool success = false;
	for (uint32_t index = 0; index < MAX_MEMORY_AREAS; index++) {
		if (memory_map[index].active) {
			continue;
		}

		success = true;
		memory_map[index].active = true;
		memory_map[index].start_address = start;
		memory_map[index].end_address = start + count;
		memory_map[index].write_pointer = NULL;
		memory_map[index].read_pointer = NULL;
		memory_map[index].write_callback = writeb;
		memory_map[index].read_callback = readb;
		memory_map[index].udata = udata;
		printf("Mapped memory from 0x%" PRIx32 ", length = 0x%" PRIx32 "\r\n", start, count);
		break;
	}
	if (!success) {
		printf("ERROR: could not register area in memory map\r\n");
	}
}

int memory_init() {
	for (uint32_t index = 0; index < MAX_MEMORY_AREAS; index++) {
		memory_map[index].active = false;
	}
	return 0;
}
