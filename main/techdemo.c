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

#include "techdemo.h"

#include "pax_shaders.h"
#include "pax_shapes.h"

#include <esp_system.h>
#include <esp_err.h>
#include <esp_log.h>
#include <string.h>
#include <malloc.h>

static const char *TAG = "pax-techdemo";

/* ============== el sponsor ============== */

// The sponsor text.
static char      *sponsor_text;
// The color of the sponsor text.
static pax_col_t  sponsor_col;
// The position of the sponsor text.
static float      sponsor_text_x;
// The position of the sponsor text.
static float      sponsor_text_y;
// The opacity of the sponsor text and logo.
static int        sponsor_alpha;

/* ======= choreographed varialbes ======== */

// Next event in the choreography.
static size_t     current_event;

// Scaling applied to the clip buffer.
static float      clip_scaling;
// Panning (translation) applied to the clip buffer (in parts of width).
static float      clip_pan_x;
// Panning (translation) applied to the clip buffer (in parts of height).
static float      clip_pan_y;
// Whether to overlay the clip buffer.
static bool       overlay_clip;

// Color used for text overlay.
static pax_col_t  text_col;
// String used for text overlay.
static char      *text_str;
// Point size used for text overlay.
static float      text_size;

// Whether to show the three shapes.
static int        to_draw;
// Additional parameters for drawing.
static float      angle_0;
// Additional parameters for drawing.
static float      angle_1;
// Additional parameters for drawing.
static float      angle_2;
// Additional parameters for drawing.
static float      angle_3;
// Additional parameters for drawing.
static float      angle_4;
// Additional parameters for drawing.
static float      angle_5;

// Scaling applied to the buffer.
static float      buffer_scaling;
// Panning (translation) applied to the buffer (in parts of width).
static float      buffer_pan_x;
// Panning (translation) applied to the buffer (in parts of height).
static float      buffer_pan_y;
// Whether to apply the background color.
static bool       use_background;
// Background applied to the buffer.
static pax_col_t  background_color;

// Linked list of interpolations.
static td_lerp_list_t *lerps = NULL;

/* ================ config ================ */

// Framebuffer to use.
static pax_buf_t *buffer = NULL;
// Clip buffer complementary to the framebuffer.
static pax_buf_t *clip_buffer = NULL;

// Width of the frame./
static int width;
// Height of the frame.
static int height;

// Whether or not the tech demo was initialised.
static bool is_initialised = false;
// Whether or not the initialisation warning was given.
static bool warning_made = false;
// Last time passed to pax_techdemo_draw.
static size_t current_time;
// Planned time for the next event.
static size_t planned_time;
// Palette used for the clip buffer.
static pax_col_t palette[2] = {
	0xffffffff,
	0xffffffff,
};

// Initialise the tech demo.
// Framebuffer can be a initialised buffer of any type (color is recommended).
// Clipbuffer should be a PAX_BUF_1_GREY initialised buffer.
void pax_techdemo_init(pax_buf_t *framebuffer, pax_buf_t *clipbuffer) {
	if (!framebuffer) {
		ESP_LOGE(TAG, "`framebuffer` must not be NULL.");
	} else if (!clipbuffer) {
		ESP_LOGE(TAG, "`clipbuffer` must not be NULL.");
	} else if (pax_buf_get_width(framebuffer) != pax_buf_get_width(clipbuffer) || pax_buf_get_height(framebuffer) != pax_buf_get_height(clipbuffer)) {
		ESP_LOGW(TAG, "`clipbuffer` must be the same size as `framebuffer` (%dx%d vs %dx%d).",
			pax_buf_get_width(clipbuffer), pax_buf_get_height(clipbuffer), pax_buf_get_width(framebuffer), pax_buf_get_height(framebuffer)
		);
	} else {
		// Sanity checks.
		if (!PAX_IS_COLOR(framebuffer->type)) {
			ESP_LOGW(TAG, "`framebuffer` should be initialised with color.");
		}
		if (clipbuffer->type != PAX_TD_BUF_TYPE) {
			ESP_LOGW(TAG, "`clipbuffer` should be initialised as PAX_TD_BUF_TYPE (" PAX_TD_BUF_STR ").");
		}
		// Palette checks.
		if (PAX_IS_PALETTE(clipbuffer->type)) {
			clipbuffer->pallette = palette;
			if (clipbuffer->type == PAX_BUF_1_PAL) {
				clipbuffer->pallette_size = 2;
			} else {
				clipbuffer->pallette_size = 4;
			}
		}
		// Copy it over.
		buffer      = framebuffer;
		clip_buffer = clipbuffer;
		width       = pax_buf_get_width(buffer);
		height      = pax_buf_get_height(buffer);
		is_initialised = true;
		
		// Reset interpolation list.
		while (lerps) {
			lerps->prev = NULL;
			td_lerp_list_t *tmp = lerps->next;
			lerps->next = NULL;
			free(lerps);
			lerps = tmp;
		}
		
		// Reset variables.
		current_event    = 0;
		planned_time     = 0;
		sponsor_alpha    = 0;
		sponsor_col      = 0xff000000;
		
		palette[0]       = 0xffffffff;
		palette[1]       = 0xffffffff;
		
		clip_scaling     = 1;
		clip_pan_x       = 0;
		clip_pan_y       = 0;
		overlay_clip     = true;
		
		text_col         = 0xffffffff;
		text_str         = NULL;
		text_size        = 18;
		
		to_draw          = TD_DRAW_SHAPES;
		angle_0          = 0;
		angle_1          = 0;
		angle_2          = 0;
		angle_3          = 0;
		angle_4          = 0;
		angle_5          = 0;
		
		buffer_scaling   = 1;
		buffer_pan_x     = 0;
		buffer_pan_y     = 0;
		background_color = 0;
		use_background   = true;

		ESP_LOGI(TAG, "PAX tech demo initialised successfully.");
	}
}

/* =============== shaders ================ */

// A shimmery shader.
pax_col_t td_shader_shimmer(pax_col_t tint, int x, int y, float u, float v, void *_args) {
	// Manhattan distance from the top left corner.
	float dist  = u + v;
	float scale = 0.2;
	float phase = (1 + scale) * 2 * angle_0 - scale;
	float value = dist - phase;
	pax_col_t normal    = 0xfff9e82a;
	pax_col_t highlight = 0xffffffff;
	if (value <= 0 && value >= -scale) {
		int part = 255 * (value / scale);
		return pax_col_lerp(part, normal, highlight);
	} else if (value >= 0 && value <= scale) {
		int part = 255 * (value / -scale);
		return pax_col_lerp(part, normal, highlight);
	} else {
		return normal;
	}
}

// Some RAINBOW CONSTANTS.
static const size_t rainbow_segments = 8;
static const float  rainbow_sqrdist[] = {
	0.125 * 0.125,
	0.250 * 0.250,
	0.375 * 0.375,
	0.500 * 0.500,
	0.625 * 0.625,
	0.750 * 0.750,
	0.875 * 0.875,
	1.000 * 1.000,
};
static const pax_col_t rainbow_color[] = {
	0xffff0000,
	0xffff7f00,
	0xffffff00,
	0xff00ff00,
	0xff00ffff,
	0xff0000ff,
	0xffff00ff,
	0xffff0000,
};

// A RAINBOW EXPLOSION shader.
pax_col_t td_shader_rainbow(pax_col_t tint, int x, int y, float u, float v, void *args) {
	int maxDist = (int) args;
	x -= width / 2;
	y -= height;
	float dist  = (x*x + y*y) / (float) maxDist;
	
	for (int i = 0; i < rainbow_segments; i++) {
		if (dist < rainbow_sqrdist[i]) {
			tint = rainbow_color[i];
			goto end;
		}
	}
	
	end:
	return pax_col_lerp(angle_1*255, tint, 0xff00ff00);
}

/* ============== functions =============== */

// Draws some title text on the clip buffer.
// If there's multiple lines (split by '\n'), that's the subtitle.
// Text is horizontally as wide as possible and vertically centered.
static void td_draw_title(size_t planned_time, size_t planned_duration, const void *args) {
	char *raw        = strdup((char *) args);
	char *index      = strchr(raw, '\n');
	char *title;
	char *subtitle;
	pax_buf_t *buf   = clip_buffer;
	pax_font_t *font = PAX_FONT_DEFAULT;
	
	// Split it up just a bit.
	pax_col_t col = 0xff000001;
	pax_background(buf, 1);
	if (index) {
		*index   = 0;
		title    = raw;
		subtitle = index + 1;
		// Title and subtitle.
		pax_vec1_t title_size    = pax_text_size(font, 1, title);
		pax_vec1_t subtitle_size = pax_text_size(font, 1, subtitle);
		float title_scale        = (int) (width / title_size.x    / font->default_size) * font->default_size;
		float subtitle_scale     = (int) (width / subtitle_size.x / font->default_size) * font->default_size;
		title_size               = pax_text_size(font, title_scale, title);
		subtitle_size            = pax_text_size(font, subtitle_scale, subtitle);
		float total_height       = title_scale + subtitle_size.y;
		float title_x            = (width  - title_size.x)    * 0.5;
		float title_y            = (height - total_height)    * 0.5;
		float subtitle_x         = (width  - subtitle_size.x) * 0.5;
		float subtitle_y         = title_y + title_scale;
		pax_draw_text(buf, col, font, title_scale,    title_x,    title_y,    title);
		pax_draw_text(buf, col, font, subtitle_scale, subtitle_x, subtitle_y, subtitle);
	} else {
		title    = raw;
		// Just the title.
		pax_vec1_t title_size    = pax_text_size(font, 1, title);
		float title_scale        = width / title_size.x;
		float title_y            = (height - title_scale) * 0.5;
		pax_draw_text(buf, col, font, title_scale, 0, title_y, title);
	}
	
	free(raw);
}

// Linearly interpolate a variable.
static void td_add_lerp(size_t planned_time, size_t planned_duration, const void *args) {
	const td_lerp_t      *subject = (td_lerp_t *) args;
	td_lerp_list_t *lerp = malloc(sizeof(td_lerp_list_t));
	lerp->start   = planned_time;
	lerp->end     = subject->duration + planned_time;
	lerp->prev    = NULL;
	lerp->next    = lerps;
	lerp->subject = subject;
	if (lerps) {
		lerps->prev = lerp;
	}
	lerps = lerp;
}

// Perform aforementioned interpolation.
static void td_perform_lerp(td_lerp_list_t *lerp) {
	const td_lerp_t *subj = lerp->subject;
	float part = (current_time - lerp->start) / (float) (lerp->end - lerp->start);
	if (current_time <= lerp->start) {
		// Clip to beginning.
		part = 0.0;
	} else if (current_time >= lerp->end) {
		// Clip to end.
		part = 1.0;
	} else {
		switch (subj->timing) {
			case TD_EASE_OUT:
				// Ease-out:    y=-x²+2x
				part = -part*part + 2*part;
				break;
			case TD_EASE_IN:
				// Ease-in:     y=x²
				part *= part;
				break;
			case TD_EASE:
				// Ease-in-out: y=-2x³+3x²
				part = -2*part*part*part + 3*part*part;
				break;
		}
	}
	switch (subj->type) {
		uint32_t bleh;
		case TD_INTERP_TYPE_INT:
			// Interpolate an integer.
			*subj->int_ptr = subj->int_from + (subj->int_to - subj->int_from) * part;
			break;
		case TD_INTERP_TYPE_COL:
			// Interpolate a (RGB) color.
			*subj->int_ptr = pax_col_lerp(part*255, subj->int_from, subj->int_to);
			break;
		case TD_INTERP_TYPE_HSV:
			// Interpolate a (HSV) color.
			bleh = pax_col_lerp(part*255, subj->int_from, subj->int_to);
			*subj->int_ptr = pax_col_ahsv(bleh >> 24, bleh >> 16, bleh >> 8, bleh);
			break;
		case TD_INTERP_TYPE_FLOAT:
			// Interpolate a float.
			*subj->float_ptr = subj->float_from + (subj->float_to - subj->float_from) * part;
			break;
	}
}

// Set a variable of a primitive type.
static void td_set_var(size_t planned_time, size_t planned_duration, const void *args) {
	td_set_t *set = (td_set_t *) args;
	memcpy(set->pointer, (void *) &set->value, set->size);
}

// Set THE string.
static void td_set_str(size_t planned_time, size_t planned_duration, const void *args) {
	text_str = (char *) args;
}

/* =============== drawing ================ */

/* ==== Draws a square, a circle and a triangle ==== */
static void td_draw_shapes() {
	float scale = fminf(width * 0.2, height * 0.4);
	pax_col_t col = 0xffff0000;
	
	pax_apply_2d(buffer, matrix_2d_translate(width * 0.5, height * 0.5));
	pax_apply_2d(buffer, matrix_2d_rotate(angle_1));
	
	// The square.
	pax_push_2d(buffer);
	pax_apply_2d(buffer, matrix_2d_translate(width * -0.25, 0));
	pax_apply_2d(buffer, matrix_2d_rotate(angle_0));
	pax_apply_2d(buffer, matrix_2d_scale(scale, scale));
	pax_draw_rect(buffer, col, -0.5, -0.5, 1, 1);
	pax_pop_2d(buffer);
	
	// The circle.
	pax_draw_circle(buffer, col, 0, 0, scale * 0.5);
	
	// The triangle.
	float my_sin = 0.866, my_cos = 0.5;
	pax_apply_2d(buffer, matrix_2d_translate(width * 0.25, 0));
	pax_apply_2d(buffer, matrix_2d_rotate(angle_0));
	pax_apply_2d(buffer, matrix_2d_scale(scale * 0.6, scale * 0.6));
	pax_draw_tri(buffer, col, -my_cos, -my_sin, -my_cos, my_sin, 1, 0);
}

/* ==== Draws a funny shimmer ==== */
static void td_draw_shimmer() {
	pax_apply_2d(buffer, matrix_2d_translate(width * 0.5, height * 0.5));
	pax_apply_2d(buffer, matrix_2d_rotate(angle_1));
	pax_shader_t shader = {
		.callback          = td_shader_shimmer,
		.alpha_promise_0   = false,
		.alpha_promise_255 = true
	};
	pax_shade_rect(buffer, -1, &shader, NULL, -50, -50, 100, 100);
}

/* ==== Show arcs and curves ==== */
static void td_draw_curves() {
	// Bezier curve control points.
	pax_vec4_t ctl0 = {
		.x0 = pax_buf_get_width(buffer) * 0.05,  .y0 = pax_buf_get_height(buffer) * 0.5,
		.x1 = pax_buf_get_width(buffer) * 0.15,  .y1 = pax_buf_get_height(buffer) * 0.95,
		.x2 = pax_buf_get_width(buffer) * 0.35,  .y2 = pax_buf_get_height(buffer) * 0.5,
		.x3 = pax_buf_get_width(buffer) * 0.5,   .y3 = pax_buf_get_height(buffer) * 0.5
	};
	// Floating crap control points.
	const pax_vec1_t el_tri[] = {
		(pax_vec1_t) { .x =  0.25f,  .y =  0.0f    },
		(pax_vec1_t) { .x = -0.125f, .y =  0.2165f },
		(pax_vec1_t) { .x = -0.125f, .y = -0.2165f },
		(pax_vec1_t) { .x =  0.25f,  .y =  0.0f    },
	};
	const pax_vec1_t el_rect[] = {
		(pax_vec1_t) { .x = -0.25f, .y = -0.25f },
		(pax_vec1_t) { .x =  0.25f, .y = -0.25f },
		(pax_vec1_t) { .x =  0.25f, .y =  0.25f },
		(pax_vec1_t) { .x = -0.25f, .y =  0.25f },
		(pax_vec1_t) { .x = -0.25f, .y = -0.25f },
	};
	
	// Curve values.
	float bez0_from = fmaxf(0, fminf(angle_0,     1));
	float bez0_to   = fmaxf(0, fminf(angle_1,     1));
	float arc0_from = fmaxf(0, fminf(angle_0 - 1, 1));
	float arc0_to   = fmaxf(0, fminf(angle_1 - 1, 1));
	float crap_from = fmaxf(0, fminf(angle_4,     1));
	float crap_to   = fmaxf(0, fminf(angle_3,     1));
	
	// First curve.
	if (bez0_from != bez0_to) {
		pax_draw_bezier_part(buffer, -1, ctl0, bez0_from, bez0_to);
	}
	// Arc.
	if (arc0_from != arc0_to) {
		float a0   = arc0_from * M_PI * -0.25 + M_PI * 0.5;
		float a1   = arc0_to   * M_PI * -0.25 + M_PI * 0.5;
		float diff = arc0_to - arc0_from;
		int   bri  = diff * 255;
		float x    = pax_buf_get_width(buffer)  * 0.5;
		float y    = pax_buf_get_height(buffer) * 0.75;
		float r    = pax_buf_get_height(buffer) * 0.25;
		
		pax_outline_arc(buffer, -1, x, y, r, a0, a1);
		
		pax_col_t stretch = pax_col_argb(bri, 255, 255, 255);
		
		pax_push_2d(buffer);
		pax_apply_2d(buffer, matrix_2d_translate(x, y));
		
		pax_apply_2d(buffer, matrix_2d_rotate(a0));
		pax_draw_line(buffer, stretch, 0, 0, r, 0);
		
		pax_apply_2d(buffer, matrix_2d_rotate(a1 - a0));
		pax_draw_line(buffer, stretch, 0, 0, r, 0);
		
		pax_pop_2d(buffer);
	}
	// Floaty shapes.
	if (crap_from != crap_to) {
		pax_push_2d(buffer);
			pax_apply_2d(buffer, matrix_2d_translate(pax_buf_get_width(buffer)  * 0.25,  pax_buf_get_height(buffer) * 0.25));
			pax_apply_2d(buffer, matrix_2d_scale    (pax_buf_get_height(buffer) * 0.25, pax_buf_get_height(buffer) * 0.25));
			pax_apply_2d(buffer, matrix_2d_rotate   (angle_2));
			pax_outline_shape_part(buffer, -1, 4, el_tri, crap_from, crap_to);
		pax_pop_2d(buffer);
		pax_push_2d(buffer);
			pax_apply_2d(buffer, matrix_2d_translate(pax_buf_get_width(buffer)  * 0.75,  pax_buf_get_height(buffer) * 0.25));
			pax_apply_2d(buffer, matrix_2d_scale    (pax_buf_get_height(buffer) * 0.25, pax_buf_get_height(buffer) * 0.25));
			pax_apply_2d(buffer, matrix_2d_rotate   (angle_2));
			pax_outline_shape_part(buffer, -1, 5, el_rect, crap_from, crap_to);
		pax_pop_2d(buffer);
	}
}

/* ==== Something something fancy air sensors ==== */

// A very low poly bird.
// Flap angle ranges from 0 to 1 and then repeats.
static void td_bird(pax_col_t color, float flap_angle, float random_offset) {
	float wing_y = sin((flap_angle + random_offset) * 2 * M_PI);
	
	pax_push_2d(buffer);
	// Bird size.
	pax_apply_2d(buffer, matrix_2d_scale(10, 10));
	// Flap with the wings.
	pax_apply_2d(buffer, matrix_2d_translate(0, wing_y * -0.2));
	// Some randomness.
	pax_apply_2d(buffer, matrix_2d_translate(0, sin((flap_angle + random_offset) * 0.25 * M_PI) * 0.5));
	
	// Left wing.
	pax_draw_tri(buffer, color, -1, wing_y, 0, -0.5, 0, 0.5);
	// Right wing.
	pax_draw_tri(buffer, color,  1, wing_y, 0, -0.5, 0, 0.5);
	
	pax_pop_2d(buffer);
}

// The scene for air sensors.
// Parameters:
//   angle_0: opacity
//   angle_1: birds flapping wings
//   angle_2: apple rotation
//   angle_3: birds Y (0 is center, relative to buffer size)
//   angle_4: apple Y (0 is center, relative to buffer size)
//   angle_5: apple X (0 is center, relative to buffer size)
static void td_draw_aero() {
	// A falling apple scene.
	pax_col_t sky_color   = 0xffbdefef;
	pax_col_t bird_color  = 0xff000000;
	pax_col_t apple_color = 0xfff82626;
	pax_col_t stem_color  = 0xff87381e;
	
	// Interpolate colors.
	uint8_t   part        = (uint8_t) (255.0 * angle_0);
	bird_color  = pax_col_lerp(part, sky_color, bird_color);
	apple_color = pax_col_lerp(part, sky_color, apple_color);
	stem_color  = pax_col_lerp(part, sky_color, stem_color);
	
	// TODO: Draw the clouds.
	
	// Draw the birds.
	pax_push_2d(buffer);
	// General vicinity.
	pax_apply_2d(buffer, matrix_2d_translate(pax_buf_get_width(buffer) * 0.75, pax_buf_get_height(buffer) * (0.5 + angle_3)));
	
	// Multiple randomly placed birds.
	pax_apply_2d(buffer, matrix_2d_translate(-15, 30));
	td_bird(bird_color, angle_1, 9.1);
	
	pax_apply_2d(buffer, matrix_2d_translate(35, -20));
	td_bird(bird_color, angle_1, 12.9);
	
	pax_apply_2d(buffer, matrix_2d_translate(-15, -15));
	td_bird(bird_color, angle_1, 23.2);
	
	pax_apply_2d(buffer, matrix_2d_translate(35, -10));
	td_bird(bird_color, angle_1, 5.5);
	
	pax_pop_2d(buffer);
	
	// Draw the apple.
	pax_push_2d(buffer);
	// Place of the apple.
	pax_apply_2d(buffer, matrix_2d_translate(pax_buf_get_width(buffer) * (0.5 + angle_5), pax_buf_get_height(buffer) * (0.5 + angle_4)));
	// Make it wobble very slightly.
	pax_apply_2d(buffer, matrix_2d_translate(cos(angle_2 * 0.125 * M_PI) * 15, sin(angle_2 * 0.25 * M_PI) * 10));
	// Rotate it progressively.
	pax_apply_2d(buffer, matrix_2d_rotate(angle_2));
	// Apple size.
	pax_apply_2d(buffer, matrix_2d_scale(12, 12));
	
	pax_draw_circle(buffer, apple_color, 0, 0, 1);
	pax_draw_tri(buffer, stem_color, 0.8, 0, 1.5, 0, 1.5, 0.3);
	
	pax_pop_2d(buffer);
}

// Draws a gear with the given number of teeth.
static void td_gear(pax_col_t color0, pax_col_t color1, int n_teeth, float big_teeth, float small_teeth, float hub_inner, float hub_outer) {
	// Prepare the points.
	small_teeth = big_teeth - small_teeth;
	int threshold   = 4;
	int multiplier  = 8;
	size_t n_points = n_teeth * multiplier + 1;
	pax_vec1_t points[n_points];
	pax_vec1_t teeth [n_points];
	pax_vectorise_circle(points, n_points, 0, 0, 1);
	
	// COMPUTE something.
	for (int i = 0; i < n_points; i++) {
		// Tooth scale.
		if ((i % multiplier) < threshold) {
			// Big teeth.
			teeth[i].x = points[i].x * big_teeth;
			teeth[i].y = points[i].y * big_teeth;
		} else {
			// Small teeth.
			teeth[i].x = points[i].x * small_teeth;
			teeth[i].y = points[i].y * small_teeth;
		}
		
	}
	
	// DRAW something.
	for (int i = 0; i < n_points - 1; i++) {
		// Hub.
		pax_draw_tri(
			buffer, color1,
			points[i].x   * hub_outer, points[i].y   * hub_outer,
			points[i].x   * hub_inner, points[i].y   * hub_inner,
			points[i+1].x * hub_inner, points[i+1].y * hub_inner
		);
		pax_draw_tri(
			buffer, color1,
			points[i].x   * hub_outer, points[i].y   * hub_outer,
			points[i+1].x * hub_inner, points[i+1].y * hub_inner,
			points[i+1].x * hub_outer, points[i+1].y * hub_outer
		);
		// Teeth.
		pax_draw_tri(
			buffer, color0,
			points[i].x * hub_outer,   points[i].y * hub_outer,
			teeth [i].x,               teeth [i].y,
			teeth [i+1].x,             teeth [i+1].y
		);
		pax_draw_tri(
			buffer, color0,
			points[i].x * hub_outer,   points[i].y * hub_outer,
			teeth [i+1].x,             teeth [i+1].y,
			points[i+1].x * hub_outer, points[i+1].y * hub_outer
		);
	}
}

/* ==== Now i'm just making stuff up ==== */

// The rainbowtastical scene.
// Parameters:
//   angle_0: Rainbow size.
//   angle_1: Fade to gears.
//   angle_2: Gear angle.
static void td_draw_rainbow() {
	static float last_radius = 0;
	if (angle_0 < 0.01) {
		last_radius = 0;
	}
	
	int max_dist = width * width * 0.25 + height * height;
	if (angle_1 == 0) {
		// Prepare the shader.
		const pax_shader_t shader = {
			.callback          = &td_shader_rainbow,
			.callback_args     = (void *) max_dist,
			.alpha_promise_0   = false,
			.alpha_promise_255 = true
		};
		
		// Make some OPTIMISED POINTS.
		const size_t n_points = 32;
		pax_vec1_t points[n_points];
		pax_vectorise_arc(points, n_points, 0, 0, 1, 0, M_PI * 2);
		
		// Draw the rainbow.
		pax_push_2d(buffer);
		pax_apply_2d(buffer, matrix_2d_translate(width * 0.5, height));
		float radius = sqrt(max_dist) * angle_0;
		for (int i = 0; i < n_points - 1; i ++) {
			pax_shade_tri(
				buffer, -1, &shader, NULL,
				points[i].x   * last_radius, points[i].y   * last_radius,
				points[i].x   * radius,      points[i].y   * radius,
				points[i+1].x * last_radius, points[i+1].y * last_radius
			);
			pax_shade_tri(
				buffer, -1, &shader, NULL,
				points[i+1].x * radius,      points[i+1].y * radius,
				points[i].x   * radius,      points[i].y   * radius,
				points[i+1].x * last_radius, points[i+1].y * last_radius
			);
		}
	}
	last_radius = angle_0 - 0.02;
	
	// Transition to background color thingy.
	if (angle_1 > 0 && angle_1 < 1) {
		const pax_shader_t shader = {
			.callback          = &td_shader_rainbow,
			.callback_args     = (void *) max_dist,
			.alpha_promise_0   = false,
			.alpha_promise_255 = true
		};
		pax_shade_rect(
			buffer, 0,
			&shader, NULL,
			0, 0, width, height
		);
		// float radius = sqrt(max_dist) * angle_1;
		// pax_draw_arc(
		// 	buffer, 0xff00ff00,
		// 	width * 0.5, height,
		// 	radius, 0, M_PI
		// );
	}
	
	// APPEAR GEAR.
	if (angle_1 > 0.5) {
		// I'd like some colors please!
		uint8_t alpha = (angle_1-0.5)*511;
		pax_col_t color0 = alpha << 24 | 0x7f7f7f;
		pax_col_t color1 = alpha << 24 | 0x3f3f3f;
		
		float hub_inner = 10;
		float hub_outer = 20;
		float gear_depth = 10;
		
		// Draw some FANTASTICAL gear.
		pax_push_2d(buffer);
		pax_apply_2d(buffer, matrix_2d_translate(width*0.5, height*0.5));
		
		// Center gear.
		pax_push_2d(buffer);
			pax_apply_2d(buffer, matrix_2d_rotate(angle_2));
			td_gear(color0, color1, 6, 50, gear_depth, hub_inner, hub_outer);
		pax_pop_2d(buffer);
		
		// Left gear.
		pax_push_2d(buffer);
			pax_apply_2d(buffer, matrix_2d_translate(-108, 0));
			pax_apply_2d(buffer, matrix_2d_rotate(angle_2*-6/8));
			td_gear(color0, color1, 8, 66, gear_depth, hub_inner, hub_outer);
		pax_pop_2d(buffer);
		
		// Right gear.
		pax_push_2d(buffer);
			pax_apply_2d(buffer, matrix_2d_translate(100, 0));
			pax_apply_2d(buffer, matrix_2d_rotate(angle_2*-6/7+M_PI));
			td_gear(color0, color1, 7, 58, gear_depth, hub_inner, hub_outer);
		pax_pop_2d(buffer);
		
		pax_pop_2d(buffer);
	}
}

/* ==== Post office scene ==== */

// Draws a letter.
static void td_letter(float x, float y, float scale) {
	// Letters have a weird size.
	pax_push_2d (buffer);
	pax_apply_2d(buffer, matrix_2d_translate(x, y));
	pax_apply_2d(buffer, matrix_2d_scale(sqrtf(2), 1));
	pax_apply_2d(buffer, matrix_2d_scale(scale, scale));
	
	pax_col_t bg_col = 0xffffffff;
	pax_col_t fg_col = 0xff7f7f7f;
	
	// Background.
	pax_draw_rect(buffer, bg_col, -0.5, -0.5, 1, 1);
	// Outline.
	pax_outline_rect(buffer, fg_col, -0.5, -0.5, 1, 1);
	// Cut thingy.
	pax_draw_line(buffer, fg_col, -0.5, -0.5, 0, 0);
	pax_draw_line(buffer, fg_col, 0.5, -0.5, 0, 0);
	
	pax_pop_2d  (buffer);
}

// Draws a letter flying by at a specific angle at a specific time.
static void td_flying_letter(float angle, float offset, float start, float end, float scale) {
	// Compute stuff.
	float max = scale * 4 + fmaxf(width, height);
	float part = (angle_0 - start) / (end - start);
	
	// Something.
	pax_push_2d (buffer);
	pax_apply_2d(buffer, matrix_2d_translate(width / 2, height / 2));
	pax_apply_2d(buffer, matrix_2d_rotate(angle));
	pax_apply_2d(buffer, matrix_2d_translate(-max/2 + max*part, 0));
	pax_apply_2d(buffer, matrix_2d_rotate(angle_1 + offset));
	td_letter   (0, 0, scale);
	pax_pop_2d  (buffer);
}

// Maybe flying letters?
static void td_draw_post() {
	// Just some chaos, nothing special.
	td_flying_letter(2.3172995179137765, 0.9327325324906965, 0.6200310008051024, 0.8572794494729159, 48.86588953650229);
	td_flying_letter(1.1905063070341015, 0.4183943088989168, 0.3240403988058697, 0.33705272363358596, 38.143095623541285);
	td_flying_letter(2.169114933629167, 2.245400777697782, 0.6312957555365378, 0.7501916341097947, 36.55700488400967);
	td_flying_letter(1.64015389484868, 3.0743381865998374, 0.14477321722782616, 0.20338671121329335, 38.419331703051235);
	td_flying_letter(2.4754739774018657, 1.3762650634594125, 0.48545398736001416, 0.6335792065361739, 39.01610438918797);
	td_flying_letter(1.1918462982962907, 0.9484799058593335, 0.607089712020574, 0.6499199346806023, 20.16319704689827);
	td_flying_letter(2.927789411987766, 0.2602116233999743, 0.6605111997828027, 0.908105035819395, 43.07647581715252);
	td_flying_letter(2.669121756551718, 0.8658407476366089, 0.13602316207365497, 0.41475671576240103, 39.05030845044536);
	td_flying_letter(0.3159085771253503, 1.1458499048251523, 0.18580346689555993, 0.23224011824071963, 28.787298439990934);
	td_flying_letter(2.7690272233484485, 2.300568705108835, 0.6446422896099403, 0.7422192872844627, 43.90241810798484);
	td_flying_letter(0.2974841191836354, 2.512401470684956, 0.03609380030621033, 0.23686171231281655, 43.69030233338533);
	td_flying_letter(2.113161103563894, 1.6288428612030033, 0.36056494296981667, 0.5132525117143615, 48.19278021461517);
	td_flying_letter(2.527710507937092, 2.8875445512690936, 0.09757311583458078, 0.31201138286110786, 46.190837995313615);
	td_flying_letter(2.05206268800383, 0.4667752847978764, 0.3753097843265101, 0.5276153455193775, 28.129515980212915);
	td_flying_letter(1.0159652669510584, 1.807708514496489, 0.18895490288728967, 0.48657324052571327, 35.04562751011628);
	td_flying_letter(1.2738686960698262, 0.12285897725133259, 0.287234101469115, 0.4865526518295369, 27.581135279138138);
	td_flying_letter(0.05161149184345973, 2.9190484660345617, 0.015185080558591123, 0.0776748678921273, 38.0804602103465);
	td_flying_letter(2.815681506060534, 1.8775014638544705, 0.6093510831545631, 0.6922654608510701, 20.050193387042746);
	td_flying_letter(1.669315294457934, 1.5957309899439274, 0.13498914479436858, 0.3218088198513566, 20.865394026032426);
	td_flying_letter(0.6829852529593806, 2.34909292707273, 0.09109951250640223, 0.20833267233162733, 21.905061643025615);
}

/* ==== Something about making the thing ==== */

// BOX.
static void td_box(bool cross, int alpha, float x, float y) {
	pax_push_2d(buffer);
	pax_apply_2d(buffer, matrix_2d_translate(x, y));
	pax_apply_2d(buffer, matrix_2d_scale(50, 50));
	pax_apply_2d(buffer, matrix_2d_translate(-0.5, 0));
	
	if (cross) {
		// Plain red background.
		pax_draw_rect(buffer, alpha|0xff0000, 0, -1, 1, 1);
		// A yellow cross shape.
		pax_draw_rect(buffer, alpha|0xffff00, 0, -0.7, 1, 0.4);
		pax_draw_rect(buffer, alpha|0xffff00, 0.3, -1, 0.4, 1);
	} else {
		// I guess a blue box will do.
		pax_draw_rect(buffer, alpha|0x0000ff, 0.25, -0.50, 0.50, 0.50);
	}
	
	pax_pop_2d(buffer);
}

// Assembly line thing.
// Parameters:
//   angle_0: How far along the boxes move.
//   angle_1: The press.
//   angle_2: The alpha.
static void td_draw_prod() {
	float dx = width / 5.0;
	uint32_t alpha = (int) (angle_2 * 0xff) << 24;
	
	pax_push_2d(buffer);
	pax_apply_2d(buffer, matrix_2d_translate(dx * angle_0 + dx / 2, 0));
		// Boxes before the press.
		for (int i = 0; i < 3; i ++) {
			td_box(false, alpha, dx * i - dx, 200);
		}
		// Boxes after the press.
		for (int i = 0; i < 3; i ++) {
			td_box(true, alpha, dx * i + dx * 2, 200);
		}
	pax_pop_2d(buffer);
	
	// The hiding device.
	pax_draw_rect(buffer, alpha|0xafafaf, (width - dx)/2, 200-dx, dx, dx);
	
	// The press.
	pax_push_2d(buffer);
	pax_apply_2d(buffer, matrix_2d_translate(width/2, 200-dx*1.5+dx/2*angle_1));
		// The plunger.
		pax_draw_rect(buffer, alpha|0x7f3f3f, -dx/2, 0, dx, -dx/4);
		// The rod.
		pax_draw_rect(buffer, alpha|0x5f3f3f, -dx/4, -dx/4, dx/2, -height);
	pax_pop_2d(buffer);
}

/* ============= choreography ============= */
/*
	Goal:
	Show off PAX' features while hiding performance limitations.
	Features to show (in no particular order):
	  ✓ Triangles
	  ✓ Arcs
	  ✓ Circles
	  - Clipping
	  ✓ Advanced shaders
	  - Texure mapping
	  ✓ Curves
	Relevant notes:
	  - MCH2022 sponsors should probably go here, before the demo.
	  - There should be an always present "skip" option (except sponsors maybe).
*/

#define TD_DELAY(time) {.duration=time,.callback=NULL}
#define TD_DRAW_TITLE(title, subtitle) {.duration=0,.callback=td_draw_title,.callback_args=title"\n"subtitle}
#define TD_INTERP_INT(delay_time, interp_time, timing_func, variable, from, to) {\
			.duration = delay_time,\
			.callback = td_add_lerp,\
			.callback_args = &(const td_lerp_t){\
				.duration = interp_time,\
				.int_ptr  = (int *) &(variable),\
				.int_from = (from),\
				.int_to   = (to),\
				.type     =  TD_INTERP_TYPE_INT,\
				.timing   =  timing_func\
			}\
		}
#define TD_INTERP_COL(delay_time, interp_time, timing_func, variable, from, to) {\
			.duration = delay_time,\
			.callback = td_add_lerp,\
			.callback_args = &(const td_lerp_t){\
				.duration = interp_time,\
				.int_ptr  = (int *) &(variable),\
				.int_from = (from),\
				.int_to   = (to),\
				.type     =  TD_INTERP_TYPE_COL,\
				.timing   =  timing_func\
			}\
		}
#define TD_INTERP_AHSV(delay_time, interp_time, timing_func, variable, from, to) {\
			.duration = delay_time,\
			.callback = td_add_lerp,\
			.callback_args = &(const td_lerp_t){\
				.duration = interp_time,\
				.int_ptr  = (int *) &(variable),\
				.int_from = (from),\
				.int_to   = (to),\
				.type     =  TD_INTERP_TYPE_HSV,\
				.timing   =  timing_func\
			}\
		}
#define TD_INTERP_FLOAT(delay_time, interp_time, timing_func, variable, from, to) {\
			.duration = delay_time,\
			.callback = td_add_lerp,\
			.callback_args = &(const td_lerp_t){\
				.duration   = interp_time,\
				.float_ptr  = (float *) &(variable),\
				.float_from = (from),\
				.float_to   = (to),\
				.type       =  TD_INTERP_TYPE_FLOAT,\
				.timing     =  timing_func\
			}\
		}
#define TD_SET_0(type_size, variable, new_value) {\
			.duration = 0,\
			.callback = td_set_var,\
			.callback_args = &(const td_set_t){\
				.size     = (type_size),\
				.pointer  = (void *) &(variable),\
				.value    = (new_value)\
			}\
		}
#define TD_SET_BOOL(variable, value) TD_SET_0(sizeof(bool), variable, value)
#define TD_SET_INT(variable, value) TD_SET_0(sizeof(int), variable, value)
#define TD_SET_LONG(variable, value) TD_SET_0(sizeof(long), variable, value)
#define TD_SET_FLOAT(variable, new_value) {\
			.duration = 0,\
			.callback = td_set_var,\
			.callback_args = &(const td_set_t){\
				.size     = sizeof(float),\
				.pointer  = (void *) &(variable),\
				.f_value  = (new_value)\
			}\
		}
#define TD_SET_STR(value) {\
			.duration = 0,\
			.callback = td_set_str,\
			.callback_args = (void*) (value)\
		}
#define TD_SHOW_LIGHT_TEXT(str) \
		TD_SET_STR(str),\
		TD_INTERP_COL(0, 2500, TD_EASE_IN, text_col, 0xffffffff, 0x00ffffff)
#define TD_SHOW_DARK_TEXT(str) \
		TD_SET_STR(str),\
		TD_INTERP_COL(0, 2500, TD_EASE_IN, text_col, 0xff000000, 0x00000000)

#define TD_ANIM_FRAMETIME 357

const td_event_t events[] = {
	
	/* ==== TITLE SEQUENCE ==== */
	TD_SET_INT        (background_color, 0xffbdefef),
	TD_SET_INT        (to_draw,    TD_DRAW_NONE),
	// Prerender some text.
	/*TD_DRAW_TITLE     ("WHY2025", ""),
	// MCH2022 thingy.
	TD_INTERP_COL     (3500, 1500, TD_LINEAR,  palette[0], 0xffffffff, 0xff000000),
	TD_INTERP_COL     (1500, 1500, TD_LINEAR,  palette[0], 0xff000000, 0xffffffff),
	// Prerender some text.
	TD_DRAW_TITLE     ("Badge.Team",
					   " PRESENTS "),
	// Presents thingy.
	TD_INTERP_COL     (1500, 1500, TD_LINEAR,  palette[0], 0xffffffff, 0xff000000),
	TD_INTERP_COL     (1500, 1500, TD_LINEAR,  palette[0], 0xff000000, 0xffffffff),
	// Prerender some text.
	TD_DRAW_TITLE     ("WHY2025",
					   "Tech demo"),
	// MCH2022_td.
	TD_INTERP_COL     (1500, 1500, TD_LINEAR,  palette[0], 0xffffffff, 0xff000000),
	TD_INTERP_COL     (   0, 2400, TD_LINEAR,  palette[0], 0xff000000, 0x00000000),
	TD_INTERP_COL     (2400, 2400, TD_LINEAR,  palette[1], 0xffffffff, 0x00ffffff),*/
	TD_SET_BOOL       (overlay_clip, false),
	
	/* ==== INTRO ANIMATION ==== */
	TD_SET_BOOL       (use_background, false),
	TD_SET_BOOL       (use_background, true),
	
	TD_INTERP_INT     (1000,  500, TD_LINEAR,   sponsor_alpha, 0, 255),
	// Funny sensors.
	TD_SET_INT        (to_draw,    TD_DRAW_AERO),
	// Rotations.
	TD_SET_FLOAT      (angle_0, 1),
	TD_SET_FLOAT      (angle_3, 1.1),
	TD_SET_FLOAT      (angle_4, -1.1),
	TD_SET_FLOAT      (angle_5, -0.25),
	TD_INTERP_FLOAT   (   0, 3500, TD_LINEAR,   angle_1, 0, 10),
	TD_INTERP_FLOAT   (   0, 4500, TD_LINEAR,   angle_2, 0, 4 * M_PI),
	// Apple falling in from the top.
	TD_INTERP_FLOAT   ( 500,  500, TD_EASE_OUT, angle_4, -1.1, 0),
	// Birds passing by.
	TD_INTERP_FLOAT   (2000, 2000, TD_LINEAR,   angle_3, 1.1, -1.1),
	// End of sponsor spot.
	TD_INTERP_INT     (1000,  500, TD_LINEAR,   sponsor_alpha, 255, 0),
	// Make the apple HIT THE GROUND.
	TD_INTERP_FLOAT   ( 500, 1000, TD_EASE_IN,  angle_4, 0, 0.45),
	TD_INTERP_FLOAT   ( 500,  500, TD_EASE_IN,  angle_5, -0.25, 0),
	
	/* ==== RAINBOW GEARS OF SCENE ==== */
	//TD_SET_INT        (to_draw,    TD_DRAW_RAINBOW),
	// Expand the rainbow.
	//TD_SET_FLOAT      (angle_0, 0),
	//TD_SET_FLOAT      (angle_1, 0),
	//TD_SET_FLOAT      (angle_2, 0),
	//TD_INTERP_COL     (   0,  500, TD_EASE_OUT, background_color, 0xffbdefef, 0xff000000),
	//TD_INTERP_FLOAT   ( 500, 1000, TD_EASE_OUT, angle_0, 0, 1),
	// Espressif spot.
	//TD_INTERP_INT     ( 500,  500, TD_LINEAR,   sponsor_alpha, 0, 255),
	// Fade to GEAR.
	TD_SET_BOOL       (use_background, false),
	TD_INTERP_FLOAT   (   0, 4000, TD_LINEAR,   angle_2, 0, M_PI),
	TD_INTERP_FLOAT   (1000, 1000, TD_LINEAR,   angle_1, 0, 1),
	TD_SET_INT        (background_color, 0xff00ff00),
	TD_SET_BOOL       (use_background, true),
	TD_DELAY          (2000),
	// End of sponsor spot.
	TD_INTERP_INT     (   0,  500, TD_LINEAR,   sponsor_alpha, 255, 0),
	// ZOOM IN.
	TD_INTERP_FLOAT   (   0, 1000, TD_EASE_IN,  buffer_scaling, 1, 20),
	TD_INTERP_AHSV    (1000, 1000, TD_LINEAR,   background_color, 0xff55ffff, 0xffaaffff),
	TD_SET_INT        (to_draw, TD_DRAW_NONE),
	TD_SET_FLOAT      (buffer_scaling, 1),
	
	/* ==== FPGA SCENE ==== */
	// Lattice spot.
	TD_SET_INT        (sponsor_col, 0xffffffff),
	TD_INTERP_INT     (   0,  500, TD_LINEAR,   sponsor_alpha, 0, 255),
	// Draw funny arcs and curves.
	TD_SET_INT        (to_draw,    TD_DRAW_CURVES),
	TD_SET_FLOAT      (angle_0,        0),
	TD_SET_FLOAT      (angle_4,        0),
	TD_INTERP_FLOAT   (   0, 5500, TD_EASE,     angle_2, 0, M_PI * 0.5),
	TD_INTERP_FLOAT   (   0, 3000, TD_EASE,     angle_3, 0, 1),
	TD_INTERP_FLOAT   (1500, 1500, TD_EASE,     angle_1, 0, 1),
	TD_INTERP_FLOAT   ( 500,  500, TD_EASE,     angle_1, 1, 2),
	TD_INTERP_FLOAT   (1000, 1000, TD_EASE,     angle_1, 2, 3),
	// Wait for just a bit.
	TD_DELAY          (1000),
	// Curves go away.
	TD_INTERP_FLOAT   (   0, 1500, TD_EASE,     angle_4, 0, 1),
	TD_INTERP_FLOAT   ( 750,  750, TD_EASE,     angle_0, 0, 1),
	TD_INTERP_FLOAT   ( 250,  250, TD_EASE,     angle_0, 1, 2),
	// End of sponsor spot.
	TD_INTERP_INT     (   0,  500, TD_LINEAR,   sponsor_alpha, 255, 0),
	TD_INTERP_FLOAT   ( 500,  500, TD_EASE,     angle_0, 2, 3),
	
	/* ==== PRODUCTION SCENE ==== */
	// The box line.
	TD_SET_FLOAT      (angle_0, 0),
	TD_SET_FLOAT      (angle_1, 0),
	TD_SET_INT        (to_draw,    TD_DRAW_PROD),
	TD_INTERP_AHSV    (   0,  500, TD_LINEAR,   background_color, 0xffaaffff, 0xffaa007f),
	TD_INTERP_FLOAT   ( 500,  500, TD_LINEAR,   angle_2, 0, 1),
	// Allnet spot.
	TD_INTERP_INT     (   0,  500, TD_LINEAR,   sponsor_alpha, 0, 255),
	TD_DELAY          (1000),
	
	// aADACSGDjebsddjrbfhvbhdsj
	TD_SET_FLOAT      (angle_1, 1),
	TD_DELAY          ( 250),
	TD_INTERP_FLOAT   ( 250,  250, TD_EASE_OUT, angle_1, 1, 0),
	TD_INTERP_FLOAT   ( 500,  500, TD_EASE,     angle_0, 0, 1),
	// aADACSGDjebsddjrbfhvbhdsj
	TD_SET_FLOAT      (angle_1, 1),
	TD_DELAY          ( 250),
	TD_INTERP_FLOAT   ( 250,  250, TD_EASE_OUT, angle_1, 1, 0),
	TD_INTERP_FLOAT   ( 500,  500, TD_EASE,     angle_0, 0, 1),
	// aADACSGDjebsddjrbfhvbhdsj
	TD_SET_FLOAT      (angle_1, 1),
	TD_DELAY          ( 250),
	TD_INTERP_FLOAT   ( 250,  250, TD_EASE_OUT, angle_1, 1, 0),
	TD_INTERP_FLOAT   ( 500,  500, TD_EASE,     angle_0, 0, 1),
	// aADACSGDjebsddjrbfhvbhdsj
	TD_SET_FLOAT      (angle_1, 1),
	TD_DELAY          ( 250),
	TD_INTERP_FLOAT   ( 250,  250, TD_EASE_OUT, angle_1, 1, 0),
	TD_INTERP_FLOAT   ( 500,  500, TD_EASE,     angle_0, 0, 1),
	// End of sponsor spot.
	TD_INTERP_INT     (   0,  500, TD_LINEAR,   sponsor_alpha, 255, 0),
	// aADACSGDjebsddjrbfhvbhdsj
	TD_SET_FLOAT      (angle_0, 0),
	TD_INTERP_FLOAT   (   0, 1000, TD_LINEAR,   angle_2, 1, 0),
	TD_SET_FLOAT      (angle_1, 1),
	TD_DELAY          ( 250),
	TD_INTERP_FLOAT   ( 250,  250, TD_EASE_OUT, angle_1, 1, 0),
	TD_INTERP_FLOAT   ( 500,  500, TD_EASE,     angle_0, 0, 1),
	TD_INTERP_COL     (   0,  500, TD_LINEAR,   background_color, 0xff7f7f7f, 0xffff3faf),
	
	/* ==== MAIL SCENE ==== */
	// RasPi spot.
	TD_SET_INT        (sponsor_col, 0xff000000),
	TD_INTERP_INT     (   0,  500, TD_LINEAR,   sponsor_alpha, 0, 255),
	// Mail flying around.
	TD_SET_INT        (to_draw, TD_DRAW_POST),
	TD_INTERP_FLOAT   (4500, 5000, TD_LINEAR,   angle_0, 0, 1),
	// End of sponsor spot.
	TD_INTERP_INT     (   0,  500, TD_LINEAR,   sponsor_alpha, 255, 0),
	TD_DELAY          ( 500),
	
	/* ==== END OF THE DEMO ==== */
	// No more sponsors.
	// Prerender the tickets thing.
	/*TD_DRAW_TITLE     ("WHY2025",
					   "Get your tickets at\n"
					   "tickets.why2025.org"),
	// Draw the tickets thing.
	TD_SET_BOOL       (overlay_clip, true),
	TD_INTERP_COL     (   0, 1500, TD_LINEAR,  palette[0], 0x00ffffff, 0xff000000),
	TD_INTERP_COL     (1500, 1500, TD_LINEAR,  palette[1], 0x00ffffff, 0xffffffff),
	TD_DELAY          (5000),
	TD_INTERP_COL     (1500, 1500, TD_LINEAR,  palette[0], 0xff000000, 0xffffffff),*/
	
	// Mark the end.
	TD_DELAY          (   0),
};
static const size_t n_events = sizeof(events) / sizeof(td_event_t);

// Draws the appropriate frame of the tech demo for the given time.
// Time is in milliseconds after the first frame.
// Returns true when running, false when finished.
bool pax_techdemo_draw(size_t now) {
	if (!is_initialised) {
		if (!warning_made) {
			ESP_LOGE(TAG, "PAX tech demo was not initialised, call `pax_techdemo_init` first.");
			warning_made = true;
		}
		return true;
	}
	bool finished = false;
	current_time = now;
	
	// Perform interpolations.
	td_lerp_list_t *lerp = lerps;
	while (lerp) {
		bool remove = lerp->end <= current_time;
		if (remove) {
			// Perform the final frame before removing it.
			td_perform_lerp(lerp);
			// Unlink it.
			if (lerp->prev) lerp->prev->next = lerp->next;
			else lerps = lerp->next;
			if (lerp->next) lerp->next->prev = lerp->prev;
			// Free it.
			td_lerp_list_t *next = lerp->next;
			free(lerp);
			lerp = next;
		} else {
			lerp = lerp->next;
		}
	}
	
	// Handle events.
	if (current_event < n_events) {
		while (current_event < n_events && planned_time <= now) {
			if (current_event == 0) {
				printf("%p\n", events);
			}
			td_event_t event = events[current_event];
			if (event.callback) {
				ESP_LOGI(TAG, "Performing event %d.", current_event);
				event.callback(planned_time, event.duration, event.callback_args);
			} else {
				ESP_LOGI(TAG, "Skipping event %d.", current_event);
			}
			planned_time += event.duration;
			current_event ++;
		}
	} else {
		finished = true;
	}
	
	// Perform interpolations.
	lerp = lerps;
	while (lerp) {
		td_perform_lerp(lerp);
		lerp = lerp->next;
	}
	
	if (use_background) {
		// Fill the background.
		pax_background(buffer, background_color);
	}
	
	pax_reset_2d(buffer, PAX_RESET_TOP);
	pax_push_2d(buffer);
	// Apply transformations.
	pax_apply_2d(buffer, matrix_2d_translate(width * 0.5, height * 0.5));
	pax_apply_2d(buffer, matrix_2d_scale(buffer_scaling, buffer_scaling));
	pax_apply_2d(buffer, matrix_2d_translate(buffer_pan_x - width * 0.5, buffer_pan_y - height * 0.5));
	
	// Draw the current scene.
	switch (to_draw) {
		case TD_DRAW_SHAPES:
			td_draw_shapes();
			break;
		case TD_DRAW_SHIMMER:
			td_draw_shimmer();
			break;
		case TD_DRAW_CURVES:
			td_draw_curves();
			break;
		case TD_DRAW_AERO:
			td_draw_aero();
			break;
		case TD_DRAW_RAINBOW:
			td_draw_rainbow();
			break;
		case TD_DRAW_POST:
			td_draw_post();
			break;
		case TD_DRAW_PROD:
			td_draw_prod();
			break;
	}
	
	pax_pop_2d(buffer);
	
	// Draw the text overlay.
	if (text_col >= 0x01000000) {
		pax_draw_text(buffer, text_col, PAX_FONT_DEFAULT, text_size, 0, 0, text_str);
	}
	
	// Draw the global overlay as clip.
	if (overlay_clip) {
		pax_push_2d(buffer);
		pax_apply_2d(buffer, matrix_2d_scale(clip_scaling, clip_scaling));
		pax_apply_2d(buffer, matrix_2d_translate(clip_pan_x * width, clip_pan_y * height));
		pax_draw_image(buffer, clip_buffer, 0, 0);
		pax_pop_2d(buffer);
	}
	
	// Draw the sponsor.
	if (sponsor_alpha) {
		pax_col_t text_col = pax_col_lerp(sponsor_alpha, sponsor_col & 0x00ffffff, sponsor_col);
		// Draw the text.
		if (sponsor_text) {
			pax_draw_text(
				buffer, text_col,
				pax_font_saira_regular, 18,
				sponsor_text_x, sponsor_text_y,
				sponsor_text
			);
		}
	}
	
	return finished;
}
