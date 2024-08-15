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
#include <stdlib.h> //for rand()
#include <string.h>
#include "config.h"
#include "debuglog.h"
#include "cpu/cpu.h"
#include "machine.h"
#include "chipset/i8259.h"
#include "chipset/i8253.h"
#include "chipset/i8237.h"
#include "chipset/i8255.h"
#include "ports.h"
#include "modules/video/sdlconsole.h"
#include "modules/video/cga.h"
#include "modules/video/vga.h"
#include <stdbool.h>

typedef void (*port_write8_callback_t)(void* udata, uint32_t portnum, uint8_t value);
typedef uint8_t (*port_read8_callback_t)(void* udata, uint32_t portnum);
typedef void (*port_write16_callback_t)(void* udata, uint32_t portnum, uint16_t value);
typedef uint16_t (*port_read16_callback_t)(void* udata, uint32_t portnum);

#define MAX_PORT_MAPS 32

typedef struct port_map {
	bool active;
	uint32_t start_address;
	uint32_t end_address;
	port_write8_callback_t write8_callback;
	port_read8_callback_t read8_callback;
	port_write16_callback_t write16_callback;
	port_read16_callback_t read16_callback;
	void* udata;
} port_map_t;

static port_map_t port_map[MAX_PORT_MAPS] = {0};

extern MACHINE_t machine;

void port_write(CPU_t* cpu, uint16_t portnum, uint8_t value) {
#ifdef DEBUG_PORTS
	debug_log(DEBUG_DETAIL, "port_write @ %03X <- %02X\r\n", portnum, value);
#endif
	portnum &= 0x0FFF;
	if (portnum == 0x80) {
		debug_log(DEBUG_DETAIL, "Diagnostic port out: %02X\r\n", value);
	}
	for (uint32_t index = 0; index < MAX_PORT_MAPS; index++) {
		if (port_map[index].active == false) {
			printf("Write to unregistered port 0x%" PRIx16 "\r\n", portnum);
			break;
		}
		if (portnum >= port_map[index].start_address && portnum < port_map[index].end_address) {
			if (port_map[index].write8_callback != NULL) {
				port_map[index].write8_callback(port_map[index].udata, portnum, value);
				break;
			}
		}
	}
}

void port_writew(CPU_t* cpu, uint16_t portnum, uint16_t value) {
	portnum &= 0x0FFF;
	if (portnum == 0x80) {
		debug_log(DEBUG_DETAIL, "Diagnostic port out: %04X\r\n", value);
	}

	for (uint32_t index = 0; index < MAX_PORT_MAPS; index++) {
		if (port_map[index].active == false) {
			break;
		}
		if (portnum >= port_map[index].start_address && portnum < port_map[index].end_address) {
			if (port_map[index].write16_callback != NULL) {
				port_map[index].write16_callback(port_map[index].udata, portnum, value);
				return;
			}
		}
	}

	port_write(cpu, portnum, (uint8_t)value);
	port_write(cpu, portnum + 1, (uint8_t)(value >> 8));
}

uint8_t port_read(CPU_t* cpu, uint16_t portnum) {
#ifdef DEBUG_PORTS
	debug_log(DEBUG_DETAIL, "port_read @ %03X\r\n", portnum);
#endif
	portnum &= 0x0FFF;

	for (uint32_t index = 0; index < MAX_PORT_MAPS; index++) {
		if (port_map[index].active == false) {
			printf("Read from unregistered port 0x%" PRIx16 "\r\n", portnum);
			break;
		}

		if (portnum >= port_map[index].start_address && portnum < port_map[index].end_address) {
			if (port_map[index].read8_callback != NULL) {
				return port_map[index].read8_callback(port_map[index].udata, portnum);
			}
		}
	}

	return 0xFF;
}

uint16_t port_readw(CPU_t* cpu, uint16_t portnum) {
	uint16_t ret;
	portnum &= 0x0FFF;

	for (uint32_t index = 0; index < MAX_PORT_MAPS; index++) {
		if (port_map[index].active == false) {
			break;
		}
		if (portnum >= port_map[index].start_address && portnum < port_map[index].end_address) {
			if (port_map[index].read16_callback != NULL) {
				return port_map[index].read16_callback(port_map[index].udata, portnum);
			}
		}
	}

	ret = port_read(cpu, portnum);
	ret |= (uint16_t)port_read(cpu, portnum + 1) << 8;
	return ret;
}

void ports_cbRegister(uint32_t start, uint32_t count, uint8_t (*readb)(void*, uint32_t), uint16_t (*readw)(void*, uint32_t), void (*writeb)(void*, uint32_t, uint8_t), void (*writew)(void*, uint32_t, uint16_t), void* udata) {
	bool success = false;
	for (uint32_t index = 0; index < MAX_PORT_MAPS; index++) {
		if (port_map[index].active) {
			continue;
		}
		success = true;
		port_map[index].active = true;
		port_map[index].start_address = start;
		port_map[index].end_address = start + count;
		port_map[index].write8_callback = writeb;
		port_map[index].read8_callback = readb;
		port_map[index].write16_callback = writew;
		port_map[index].read16_callback = readw;
		port_map[index].udata = udata;
		printf("Registered port from 0x%" PRIx32 ", length = 0x%" PRIx32 "\r\n", start, count);
		break;
	}

	if (!success) {
		printf("ERROR: could not register port in memory map\r\n");
	}
}

void ports_init() {
	for (uint32_t index = 0; index < MAX_PORT_MAPS; index++) {
		port_map[index].active = false;
	}
}
