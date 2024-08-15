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

#include "../../config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "sdlconsole.h"
#include "../input/sdlkeys.h"
#include "../input/mouse.h"
#include "../../timing.h"

#include "display_test.h"
#include "pax_gfx.h"
#include <inttypes.h>
#include "bsp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct keyboard_event {
	bsp_input_t input;
	bool pressed;
} keyboard_event_t;

static QueueHandle_t keyboard_event_queue = NULL;

uint64_t sdlconsole_frameTime[30];
uint32_t sdlconsole_keyTimer;
uint8_t sdlconsole_curkey, sdlconsole_lastKey, sdlconsole_frameIdx = 0, sdlconsole_grabbed = 0, sdlconsole_ctrl = 0, sdlconsole_alt = 0, sdlconsole_doRepeat = 0;
int sdlconsole_curw, sdlconsole_curh;

char* sdlconsole_title;

void sdlconsole_keyRepeat(void* dummy) {
	sdlconsole_doRepeat = 1;
	timing_updateIntervalFreq(sdlconsole_keyTimer, 15);
}

pax_col_t* vga_change_buffer = NULL;

int sdlconsole_init(char *title) {
	sdlconsole_title = title;

	sdlconsole_keyTimer = timing_addTimer(sdlconsole_keyRepeat, NULL, 2, TIMING_DISABLED);

	keyboard_event_queue = xQueueCreate(8, sizeof(keyboard_event_t));

	vga_change_buffer = calloc(1024*1024, sizeof(uint32_t));

	return 0;
}

int sdlconsole_setWindow(int w, int h) {
	return 0;
}

void sdlconsole_setTitle(char* title) { //appends something to the main title, doesn't replace it all

}

void sdlconsole_blit(uint32_t* pixels, int w, int h, int stride) {
	static uint64_t lasttime = 0;
	uint64_t curtime;
	curtime = timing_getCur();

	pax_buf_t* fb = display_test_get_buf();
	
	/*for (uint32_t y = 0; y < h; y++) {
		for (uint32_t x = 0; x < w; x++) {
			uint32_t position = y * 1024 + x;
			pax_col_t color = pixels[position];
			if (vga_change_buffer[position] != color) {
				vga_change_buffer[position] = color;
				pax_set_pixel(fb, color, x, y);
			}
		}
	}*/

	for (uint32_t y = 0; y < h; y++) {
		for (uint32_t x = 0; x < w; x++) {
			uint32_t position = y * (stride/sizeof(uint32_t)) + x;
			pax_col_t color = pixels[position];
			if (vga_change_buffer[position] != color) {
				vga_change_buffer[position] = color;
				pax_set_pixel(fb, color, x, y);
			}
		}
	}

	display_test_flush();
	//vTaskDelay(pdMS_TO_TICKS(10));
	vPortYield();

	lasttime = curtime;
}

void sdlconsole_mousegrab() {
	sdlconsole_ctrl = sdlconsole_alt = 0;
	sdlconsole_grabbed = 0;
}

uint8_t sdlconsole_translateScancode(bsp_input_t input) {
	switch (input) {
		case BSP_INPUT_NONE:
			return 0x00;
		case BSP_INPUT_LEFT:
			return 0x4B;
		case BSP_INPUT_RIGHT:
			return 0x4D;
		case BSP_INPUT_UP:
			return 0x48;
		case BSP_INPUT_DOWN:
			return 0x50;
		case BSP_INPUT_PREV:
			return 0x00;
		case BSP_INPUT_NEXT:
			return 0x00;
		case BSP_INPUT_ACCEPT:
			return 0x00;
		case BSP_INPUT_BACK:
			return 0x00;
		case BSP_INPUT_SPACE:
			return 0x39;
		case BSP_INPUT_KB_TILDA:
			return 0x29;
		case BSP_INPUT_KB_1:
			return 0x02;
		case BSP_INPUT_KB_2:
			return 0x03;
		case BSP_INPUT_KB_3:
			return 0x04;
		case BSP_INPUT_KB_4:
			return 0x05;
		case BSP_INPUT_KB_5:
			return 0x06;
		case BSP_INPUT_KB_6:
			return 0x07;
		case BSP_INPUT_KB_7:
			return 0x08;
		case BSP_INPUT_KB_8:
			return 0x09;
		case BSP_INPUT_KB_9:
			return 0x0A;
		case BSP_INPUT_KB_0:
			return 0x0B;
		case BSP_INPUT_KB_MINUS:
			return 0x0C;
		case BSP_INPUT_KB_EQUALS:
			return 0x0D;
		case BSP_INPUT_KB_LBRAC:
			return 0x00;
		case BSP_INPUT_KB_RBRAC:
			return 0x00;
		case BSP_INPUT_KB_BACKSLASH:
			return 0x2B;
		case BSP_INPUT_KB_SEMICOLON:
			return 0x27;
		case BSP_INPUT_KB_QUOTE:
			return 0x28;
		case BSP_INPUT_KB_COMMA:
			return 0x33;
		case BSP_INPUT_KB_DOT:
			return 0x34;
		case BSP_INPUT_KB_SLASH:
			return 0x35;
		case BSP_INPUT_KB_A:
			return 0x1E;
		case BSP_INPUT_KB_B:
			return 0x30;
		case BSP_INPUT_KB_C:
			return 0x2E;
		case BSP_INPUT_KB_D:
			return 0x20;
		case BSP_INPUT_KB_E:
			return 0x12;
		case BSP_INPUT_KB_F:
			return 0x21;
		case BSP_INPUT_KB_G:
			return 0x22;
		case BSP_INPUT_KB_H:
			return 0x23;
		case BSP_INPUT_KB_I:
			return 0x17;
		case BSP_INPUT_KB_J:
			return 0x24;
		case BSP_INPUT_KB_K:
			return 0x25;
		case BSP_INPUT_KB_L:
			return 0x26;
		case BSP_INPUT_KB_M:
			return 0x32;
		case BSP_INPUT_KB_N:
			return 0x31;
		case BSP_INPUT_KB_O:
			return 0x18;
		case BSP_INPUT_KB_P:
			return 0x19;
		case BSP_INPUT_KB_Q:
			return 0x10;
		case BSP_INPUT_KB_R:
			return 0x13;
		case BSP_INPUT_KB_S:
			return 0x1F;
		case BSP_INPUT_KB_T:
			return 0x14;
		case BSP_INPUT_KB_U:
			return 0x16;
		case BSP_INPUT_KB_V:
			return 0x2F;
		case BSP_INPUT_KB_W:
			return 0x11;
		case BSP_INPUT_KB_X:
			return 0x2D;
		case BSP_INPUT_KB_Y:
			return 0x15;
		case BSP_INPUT_KB_Z:
			return 0x2C;
		case BSP_INPUT_F1:
			return 0x3B;
		case BSP_INPUT_F2:
			return 0x3C;
		case BSP_INPUT_F3:
			return 0x3D;
		case BSP_INPUT_F4:
			return 0x3E;
		case BSP_INPUT_F5:
			return 0x3F;
		case BSP_INPUT_F6:
			return 0x40;
		case BSP_INPUT_F7:
			return 0x41;
		case BSP_INPUT_F8:
			return 0x42;
		case BSP_INPUT_F9:
			return 0x43;
		case BSP_INPUT_F10:
			return 0x44;
		case BSP_INPUT_F11:
			return 0x00;
		case BSP_INPUT_F12:
			return 0x00;
		case BSP_INPUT_NP_0:
			return 0x52;
		case BSP_INPUT_NP_1:
			return 0x4F;
		case BSP_INPUT_NP_2:
			return 0x50;
		case BSP_INPUT_NP_3:
			return 0x51;
		case BSP_INPUT_NP_4:
			return 0x4B;
		case BSP_INPUT_NP_5:
			return 0x4C;
		case BSP_INPUT_NP_6:
			return 0x4D;
		case BSP_INPUT_NP_7:
			return 0x47;
		case BSP_INPUT_NP_8:
			return 0x48;
		case BSP_INPUT_NP_9:
			return 0x49;
		case BSP_INPUT_NP_SLASH:
			return 0x00;
		case BSP_INPUT_NP_STAR:
			return 0x37;
		case BSP_INPUT_NP_MINUS:
			return 0x4A;
		case BSP_INPUT_NP_PLUS:
			return 0x4E;
		case BSP_INPUT_NP_ENTER:
			return 0x1C;
		case BSP_INPUT_NP_PERIOD:
			return 0x53;
		case BSP_INPUT_L_SHIFT:
			return 0x2A;
		case BSP_INPUT_R_SHIFT:
			return 0x36;
		case BSP_INPUT_L_CTRL:
			return 0x1D;
		case BSP_INPUT_R_CTRL:
			return 0x00;
		case BSP_INPUT_L_ALT:
			return 0x38;
		case BSP_INPUT_R_ALT:
			return 0x00;
		case BSP_INPUT_L_SUPER:
			return 0x00;
		case BSP_INPUT_R_SUPER:
			return 0x00;
		case BSP_INPUT_FUNCTION:
			return 0x00;
		case BSP_INPUT_ENTER:
			return 0x1C;
		case BSP_INPUT_BACKSPACE:
			return 0x0E;
		case BSP_INPUT_DELETE:
			return 0x53;
		case BSP_INPUT_ESCAPE:
			return 0x01;
		case BSP_INPUT_TAB:
			return 0x0F;
		case BSP_INPUT_CTXMENU:
			return 0x00;
		case BSP_INPUT_PRINTSCR:
			return 0x00;
		case BSP_INPUT_PAUSE_BREAK:
			return 0x00;
		case BSP_INPUT_INSERT:
			return 0x52;
		case BSP_INPUT_HOME:
			return 0x47;
		case BSP_INPUT_END:
			return 0x4F;
		case BSP_INPUT_PGUP:
			return 0x49;
		case BSP_INPUT_PGDN:
			return 0x51;
		case BSP_INPUT_NUM_LK:
			return 0x45;
		case BSP_INPUT_CAPS_LK:
			return 0x3A;
		case BSP_INPUT_SCROLL_LK:
			return 0x46;
		default:
			return 0x00;
	}
}

int sdlconsole_loop() {
	if (sdlconsole_doRepeat) {
		sdlconsole_doRepeat = 0;
		sdlconsole_curkey = sdlconsole_lastKey;
		return SDLCONSOLE_EVENT_KEY;
	}

 	keyboard_event_t event = {0};
	if (xQueueReceive(keyboard_event_queue, &event, 0) == pdTRUE) {
		if (event.pressed) {
			sdlconsole_curkey = sdlconsole_translateScancode(event.input);
			if (sdlconsole_curkey == 0x00) {
				return SDLCONSOLE_EVENT_NONE;
			} else {
				sdlconsole_lastKey = sdlconsole_curkey;
				timing_updateIntervalFreq(sdlconsole_keyTimer, 2);
				timing_timerEnable(sdlconsole_keyTimer);
				return SDLCONSOLE_EVENT_KEY;
			}
		} else {
			sdlconsole_curkey = sdlconsole_translateScancode(event.input) | 0x80;
			if ((sdlconsole_curkey & 0x7F) == sdlconsole_lastKey) {
				timing_timerDisable(sdlconsole_keyTimer);
			}
			return (sdlconsole_curkey == 0x80) ? SDLCONSOLE_EVENT_NONE : SDLCONSOLE_EVENT_KEY;
		}
	}
	return SDLCONSOLE_EVENT_NONE;
}

uint8_t sdlconsole_getScancode() {
	//debug_log(DEBUG_DETAIL, "curkey: %02X\r\n", sdlconsole_curkey);
	return sdlconsole_curkey;
}

void demo_call(bsp_input_t input, bool pressed) {
	keyboard_event_t event = {
		.input = input,
		.pressed = pressed
	};
	if (keyboard_event_queue == NULL) {
		printf("No queue!?\r\n");
	} else {
		xQueueSend(keyboard_event_queue, &event, 0);
	}
}