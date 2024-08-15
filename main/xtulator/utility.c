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
#include <time.h>
#include <errno.h>
#include "config.h"
#include "memory.h"
#include "debuglog.h"

#include "freertos/FreeRTOS.h"

int utility_loadFile(uint8_t* dst, size_t len, char* srcfile) {
	FILE* file;
	if (dst == NULL) {
		printf("loadFile: destination is NULL\r\n'");
		return -1;
	}

	file = fopen(srcfile, "rb");
	if (file == NULL) {
		printf("Failed to open %s\r\n", srcfile);
		return -1;
	}
	if (fread(dst, 1, len, file) < len) {
		fclose(file);
		return -1;
	}
	fclose(file);
	return 0;
}

void utility_sleep(uint32_t ms) {
	vTaskDelay(pdMS_TO_TICKS(ms));
}
