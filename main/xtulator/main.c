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
#include <string.h>
#include "config.h"
#include "timing.h"
#include "memory.h"
#include "ports.h"
#include "machine.h"
#include "utility.h"
#include "debuglog.h"
#include "cpu/cpu.h"
#include "chipset/i8259.h"
#include "modules/disk/biosdisk.h"
#include "modules/video/sdlconsole.h"
#include "modules/audio/sdlaudio.h"
#ifdef USE_NE2000
#include "modules/io/pcap-win32.h"
#endif
#include "freertos/FreeRTOS.h"

char* usemachine = "generic_xt"; //default

char title[64]; //assuming 64 isn't safe if somebody starts messing with STR_TITLE and STR_VERSION

uint64_t ops = 0;
uint32_t baudrate = 115200, ramsize = 640, instructionsperloop = 100, cpuLimitTimer;
uint8_t videocard = 0xFF, showMIPS = 0;
volatile uint8_t goCPU = 1, limitCPU = 0;
volatile double speed = 0;

volatile uint8_t running = 1;

MACHINE_t machine;

void optimer(void* dummy) {
	ops /= 10000;
	if (showMIPS) {
		debug_log(DEBUG_INFO, "%llu.%llu MIPS          \r", ops / 10, ops % 10);
	}
	ops = 0;
}

void cputimer(void* dummy) {
	goCPU = 1;
}

void setspeed(double mhz) {
	if (mhz > 0) {
		speed = mhz;
		instructionsperloop = (uint32_t)((speed * 1000000.0) / 140000.0);
		limitCPU = 1;
		debug_log(DEBUG_INFO, "[MACHINE] Throttling speed to approximately a %.02f MHz 8088 (%lu instructions/sec)\r\n", speed, instructionsperloop * 10000);
		timing_timerEnable(cpuLimitTimer);
	}
	else {
		speed = 0;
		instructionsperloop = 100;
		limitCPU = 0;
		timing_timerDisable(cpuLimitTimer);
	}
}

void xtulator_main() {

	sprintf(title, "%s v%s pre alpha", STR_TITLE, STR_VERSION);

	printf("%s (c)2020 Mike Chambers\r\n", title);
	printf("[A portable, open source 80186 PC emulator]\r\n\r\n");

	ports_init();
	timing_init();
	memory_init();

	machine.pcap_if = -1;

	// To-Do: put args in machine

	biosdisk_insert(&machine.CPU, 2, "/sdcard/hd0.img");

	if (sdlconsole_init(title)) {
		debug_log(DEBUG_ERROR, "[ERROR] SDL initialization failure\r\n");
		return;
	}

	if (sdlaudio_init(&machine)) {
		debug_log(DEBUG_INFO, "[WARNING] SDL audio initialization failure\r\n");
	}

	if (machine_init(&machine, usemachine) < 0) {
		debug_log(DEBUG_ERROR, "[ERROR] Machine initialization failure\r\n");
		return;
	}

	if (bootdrive == 0xFF) {
		if (biosdisk[2].inserted) {
			bootdrive = 0x80;
		}
		else {
			bootdrive = 0x00;
		}
	}

	timing_addTimer(optimer, NULL, 10, TIMING_ENABLED);
	cpuLimitTimer = timing_addTimer(cputimer, NULL, 10000, TIMING_DISABLED);
	if (speed > 0) {
		setspeed(speed);
	}
	while (running) {
		static uint32_t curloop = 0;
		if (limitCPU == 0) {
			goCPU = 1;
		}
		if (goCPU) {
			cpu_interruptCheck(&machine.CPU, &machine.i8259);
			cpu_exec(&machine.CPU, instructionsperloop);
			ops += instructionsperloop;
			goCPU = 0;
		}
		timing_loop();
		sdlaudio_updateSampleTiming();
		if (++curloop == 100) {
			switch (sdlconsole_loop()) {
			case SDLCONSOLE_EVENT_KEY:
				machine.KeyState.scancode = sdlconsole_getScancode();
				machine.KeyState.isNew = 1;
				i8259_doirq(&machine.i8259, 1);
				break;
			case SDLCONSOLE_EVENT_QUIT:
				running = 0;
				break;
			case SDLCONSOLE_EVENT_DEBUG_1:
				break;
			case SDLCONSOLE_EVENT_DEBUG_2:
				break;
			}


			curloop = 0;

			vPortYield();
		}
	}

	return;
}
