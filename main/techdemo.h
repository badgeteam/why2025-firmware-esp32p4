/*
	MIT License

	Copyright (c) 2022 Julian Scheffers

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#ifndef TECHDEMO_H
#define TECHDEMO_H

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "pax_gfx.h"

/* ================ types ================= */
#define TD_DRAW_NONE    0
#define TD_DRAW_SHAPES  1
#define TD_DRAW_SHIMMER 2
#define TD_DRAW_CURVES  3
#define TD_DRAW_AERO    4
#define TD_DRAW_RAINBOW 5
#define TD_DRAW_POST    6
#define TD_DRAW_PROD    7

#define TD_INTERP_TYPE_INT   0
#define TD_INTERP_TYPE_COL   1
#define TD_INTERP_TYPE_HSV   2
#define TD_INTERP_TYPE_FLOAT 3
#define TD_LINEAR 0
#define TD_EASE 1
#define TD_EASE_IN 2
#define TD_EASE_OUT 3

#define PAX_TD_BUF_TYPE  PAX_BUF_1_PAL
#define PAX_TD_BUF_STR  "PAX_BUF_1_PAL"

// Used to represent time delays in a smaller size.
typedef uint16_t td_delay_t;

struct td_event;
struct td_lerp;
struct td_lerp_list;
struct td_set;

typedef struct td_event     td_event_t;
typedef struct td_lerp      td_lerp_t;
typedef struct td_lerp_list td_lerp_list_t;
typedef struct td_set       td_set_t;
typedef void (*const td_func_t)(size_t planned_time, size_t planned_duration, const void *args);

struct td_event {
	const td_delay_t duration;
	const td_func_t  callback;
	const void     * callback_args;
};

struct td_lerp_list {
	td_lerp_list_t  *prev;
	td_lerp_list_t  *next;
	const td_lerp_t *subject;
	uint32_t         start;
	uint32_t         end;
};

struct td_lerp {
	td_delay_t duration;
	union {
		int   *int_ptr;
		float *float_ptr;
	};
	union {
		int    int_from;
		float  float_from;
	};
	union {
		int    int_to;
		float  float_to;
	};
	uint8_t    type;
	uint8_t    timing;
};

struct td_set {
	size_t       size;
	void        *pointer;
	union {
		uint64_t value;
		float    f_value;
	};
};

/* ============== functions =============== */

// Initialise the tech demo.
// Framebuffer can be a initialised buffer of any type (color is recommended).
// Clipbuffer should be a PAX_TD_BUF_TYPE initialised buffer.
void pax_techdemo_init(pax_buf_t *framebuffer, pax_buf_t *clipbuffer);

// Draws the appropriate frame of the tech demo for the given time.
// Time is in milliseconds after the first frame.
// Returns false when running, true when finished.
bool pax_techdemo_draw(size_t millis);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //TECHDEMO_H
