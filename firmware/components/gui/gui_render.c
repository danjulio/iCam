/*
 * Renderers for images, spot meter and min/max markers
 *
 * Copyright 2020-2024 Dan Julio
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "esp_system.h"
#ifndef CONFIG_BUILD_ICAM_MINI

#ifdef ESP_PLATFORM
//	#include "esp_system.h"
	#include "esp_log.h"
	#include "esp_heap_caps.h"
#else
	#include <stdio.h>
	#include <stdlib.h>
#endif
#include "gui_render.h"
#include "palettes.h"
#include <math.h>
#include <string.h>



//
// Local constants
//
#ifdef ESP_PLATFORM
	#define COLOR_WHITE 0xFFFF
	#define COLOR_BLACK 0x0000
#else
	#define COLOR_WHITE 0xFFFFFFFF
	#define COLOR_BLACK 0xFF000000
#endif

// Linear Doubling Interpolation Scale Factors
//  DS = Dual Source Pixel case (SF_DS is typically 2 or 3)
//  QS = Quad Source Pixel case (SF_QS is typically 3 or 5)
//
#define SF_DS 3
#define SF_QS 5
#define DIV_DS (SF_DS + 1)
#define DIV_QS (SF_QS + 3)



//
// Local variables
//
static const char* TAG = "gui_render";

// State
static bool is_portrait = false;
static int mag_level = GUI_MAGNIFICATION_1_0;
static float mag_factor = 1;
static int16_t img_w, img_h;   // Raw image dimensions accounting for rotation

// y8 buffer (holds scaled and correctly rotated raw image data)
static uint8_t* y8_buf;



//
// Forward declarations for internal functions
//
static void _render_image_0_5(gui_img_buf_t* raw, GUI_REND_IMG_T* img, gui_state_t* g);
static void _render_image_1_0(gui_img_buf_t* raw, GUI_REND_IMG_T* img, gui_state_t* g);
static void _render_image_1_5(gui_img_buf_t* raw, GUI_REND_IMG_T* img, gui_state_t* g);
static void _render_image_2_0(gui_img_buf_t* raw, GUI_REND_IMG_T* img, gui_state_t* g);
static void _render_min_marker(gui_img_buf_t* raw, GUI_REND_IMG_T* img);
static void _render_max_marker(gui_img_buf_t* raw, GUI_REND_IMG_T* img);

static void _draw_hline(GUI_REND_IMG_T* img, int16_t x1, int16_t x2, int16_t y, GUI_REND_IMG_T c);
static void _draw_vline(GUI_REND_IMG_T* img, int16_t x, int16_t y1, int16_t y2, GUI_REND_IMG_T c);
static void _draw_line(GUI_REND_IMG_T* img, int16_t x1, int16_t y1, int16_t x2, int16_t y2, GUI_REND_IMG_T c);
static void _draw_circle(GUI_REND_IMG_T* img, int16_t x0, int16_t y0, int16_t r, GUI_REND_IMG_T c);
static void _draw_rect(GUI_REND_IMG_T* img, int16_t x, int16_t y, int16_t w, int16_t h, GUI_REND_IMG_T c);
static void _draw_fill_rect(GUI_REND_IMG_T* img, int16_t x, int16_t y, int16_t w, int16_t h, GUI_REND_IMG_T c);
static __inline__ void _draw_pixel(GUI_REND_IMG_T* img, int16_t x, int16_t y, GUI_REND_IMG_T c);

static void _interp_set_pixel(uint8_t src, GUI_REND_IMG_T* img, int x, int y);
static void _interp_set_outer_row(uint8_t* src, GUI_REND_IMG_T* img, int16_t src_w, int16_t src_h, bool first_row);
static void _interp_set_outer_col(uint8_t* src, GUI_REND_IMG_T* img, int16_t src_w, int16_t src_h, bool first_col);
static void _interp_set_inner(uint8_t* src, int16_t src_w, int16_t src_h, GUI_REND_IMG_T* img);



//
// API
//
bool gui_render_init()
{
	// Allocate memory for the y8 buffer
#ifdef ESP_PLATFORM
	y8_buf = (uint8_t*) heap_caps_calloc(GUI_RAW_IMG_W*GUI_RAW_IMG_H, sizeof(uint8_t), MALLOC_CAP_8BIT);
	if (y8_buf == NULL) {
		ESP_LOGE(TAG, "Could not allocate y8_buf");
		return false;
	}
#else
	y8_buf = (uint8_t*) malloc(GUI_RAW_IMG_W*GUI_RAW_IMG_H);
	if (y8_buf == NULL) {
		printf("%s Could not allocate y8_buf", TAG);
		return false;
	}
#endif

	return true;
}


void gui_render_set_configuration(int orientation, int magnification)
{
	is_portrait = (orientation == GUI_RENDER_PORTRAIT);
	
	mag_level = magnification;
	switch (magnification) {
		case GUI_MAGNIFICATION_0_5:
			mag_factor = 0.5;
			break;
		case GUI_MAGNIFICATION_1_5:
			mag_factor = 1.5;
			break;
		case GUI_MAGNIFICATION_2_0:
			mag_factor = 2.0;
			break;
		default: // 1x
			mag_level = GUI_MAGNIFICATION_1_0;  // In case they handed us an illegal value
			mag_factor = 1.0;
	}
	
	if (is_portrait) {
		img_w = (int16_t) round((float) GUI_RAW_IMG_H * mag_factor);
		img_h = (int16_t) round((float) GUI_RAW_IMG_W * mag_factor);
	} else {
		img_w = (int16_t) round((float) GUI_RAW_IMG_W * mag_factor);
		img_h = (int16_t) round((float) GUI_RAW_IMG_H * mag_factor);
	}
}


uint8_t* gui_render_get_y8_data(uint16_t* y16_data, uint16_t y16_min, uint16_t y16_max)
{
#ifdef ESP_PLATFORM
	uint32_t t32;
	uint32_t diff;
	
	diff = y16_max - y16_min;
	if (diff == 0) diff = 1;
	
	// We have to scale the y16 to 8-bits
	if (is_portrait) {
		// We also have to rotate the data
		// 
		// 11 21 31 41
		// 12 22 32 42
		// 13 23 33 43
		//
		// becomes
		//
		// 13 12 11
		// 23 22 21
		// 33 32 31
		// 43 42 41
		//
		// xy
		// 20 = 11
		// 21 = 21
		// 22 = 31
		// 23 = 41
		// 10 = 12
		// 11 = 22
		// 12 = 32
		// 13 = 42
		// 00 = 13
		// 01 = 23
		// 02 = 33
		// 03 = 43
		//
		// for x=(GUI_RAW_IMG_H-1) downto 0
		//   dstP = dst + x
		//   dstE = dstP + GUI_RAW_IMG_H*GUI_RAW_IMG_W
		//   do {
		//      *dstP = *srcP++
		//      dstP += GUI_RAW_IMG_H
		//   } while (dstP < dstE)
		uint16_t* y16p = y16_data;
		uint8_t* y8p;
		uint8_t* y8end;
	
		for (int x=(GUI_RAW_IMG_H-1); x >= 0; x--) {
			y8p = y8_buf + x;
			y8end = y8p + (GUI_RAW_IMG_H*GUI_RAW_IMG_W);
			do {
				t32 = ((uint32_t)(*y16p++ - y16_min) * 255) / diff;
				*y8p = (t32 > 255) ? 255 : (uint8_t) t32;
				y8p += GUI_RAW_IMG_H;
			} while (y8p < y8end);
		}
	} else {
		// Landscape is native format
		uint16_t* y16p = y16_data;
		uint16_t* y16end = y16p + (GUI_RAW_IMG_W*GUI_RAW_IMG_H);
		uint8_t* y8p = y8_buf;
		do {
			t32 = ((uint32_t)(*y16p - y16_min) * 255) / diff;
			*y8p++ = (t32 > 255) ? 255 : (uint8_t) t32;
		} while (++y16p < y16end);
	}
#else
	// Web data has already been scaled to 8 bits so we recast the y16 pointer and treat
	// the data as 8 bits - no need to use min/max for scaling
	if (is_portrait) {
		// Rotate
		uint8_t* y16p = (uint8_t*) y16_data;
		uint8_t* y8p;
		uint8_t* y8end;
	
		for (int x=(GUI_RAW_IMG_H-1); x >= 0; x--) {
			y8p = y8_buf + x;
			y8end = y8p + (GUI_RAW_IMG_H*GUI_RAW_IMG_W);
			do {
				*y8p = *y16p++;
				y8p += GUI_RAW_IMG_H;
			} while (y8p < y8end);
		}
	} else {
		// Only have copy it over
		memcpy(y8_buf, (uint8_t*) y16_data, GUI_RAW_IMG_W*GUI_RAW_IMG_H);
	}
#endif

	return y8_buf;
}


void gui_render_image_data(gui_img_buf_t* raw, GUI_REND_IMG_T* img, gui_state_t* g)
{
	switch (mag_level) {
		case GUI_MAGNIFICATION_0_5:
			_render_image_0_5(raw, img, g);
			break;
		case GUI_MAGNIFICATION_1_0:
			_render_image_1_0(raw, img, g);
			break;
		case GUI_MAGNIFICATION_1_5:
			_render_image_1_5(raw, img, g);
			break;
		case GUI_MAGNIFICATION_2_0:
			_render_image_2_0(raw, img, g);
			break;
	}
}


void gui_render_spotmeter(gui_img_buf_t* raw, GUI_REND_IMG_T* img)
{
	int16_t x, y;
	int16_t r;
	
	// Radius
	r = (uint16_t) round(((float) GUI_SPOT_SIZE * mag_factor)) / 2;
	
	// Spot center
	if (is_portrait) {
		x = (int16_t) round(((float) img_w - (raw->spot_y * mag_factor)));
		y = (int16_t) round(((float) raw->spot_x * mag_factor));
	} else {
		x = (int16_t) round(((float) raw->spot_x * mag_factor));
		y = (int16_t) round(((float) raw->spot_y * mag_factor));
	}
	
	// Draw a white circle surrounded by a black circle for contrast on
	// all color palettes
	_draw_circle(img, x, y, r, COLOR_WHITE);
	_draw_circle(img, x, y, r+2, COLOR_BLACK);
}


void gui_render_min_max_markers(gui_img_buf_t* raw, GUI_REND_IMG_T* img)
{
	_render_min_marker(raw, img);
	_render_max_marker(raw, img);
}


void gui_render_region_marker(gui_img_buf_t* raw, GUI_REND_IMG_T* img)
{
	int16_t x, y;
	uint16_t w, h;
	
	// Upper left corner and dimensions of spot box
	if (is_portrait) {
		// Rotate x, y preserving origin
		w = (uint16_t) round(((float) (raw->region_y2 - raw->region_y1 + 1) * mag_factor));
		h = (uint16_t) round(((float) (raw->region_x2 - raw->region_x1 + 1) * mag_factor));
		x = (int16_t) round(((float) img_w - (raw->region_y2 * mag_factor)));
		y = (int16_t) round(((float) raw->region_x1 * mag_factor));
	} else {
		w = (uint16_t) round(((float) (raw->region_x2 - raw->region_x1 + 1) * mag_factor));
		h = (uint16_t) round(((float) (raw->region_y2 - raw->region_y1 + 1) * mag_factor));
		x = (int16_t) round(((float) raw->region_x1 * mag_factor));
		y = (int16_t) round(((float) raw->region_y1 * mag_factor));
	}
	
	// Draw a white bounding box surrounded by a black bounding box for contrast
	// on all color palettes
	_draw_rect(img, x, y, w, h, COLOR_WHITE);
	
	x--;
	y--;
	w += 2;
	h += 2;
	
	_draw_rect(img, x, y, w, h, COLOR_BLACK);
}


// Draws marker without adjusting for rotation or magnification factor because this is happening
// on the current rotation coordinate system
void gui_render_region_drag_marker(gui_img_buf_t* raw, GUI_REND_IMG_T* img)
{
	int16_t x, y;
	uint16_t w, h;
	
	w = raw->region_x2 - raw->region_x1 + 1;
	h = raw->region_y2 - raw->region_y1 + 1;
	x = (int16_t) raw->region_x1;
	y = (int16_t) raw->region_y1;
	
	// Draw a white bounding box surrounded by a black bounding box for contrast
	// on all color palettes
	_draw_rect(img, x, y, w, h, COLOR_WHITE);
	
	x--;
	y--;
	w += 2;
	h += 2;
	
	_draw_rect(img, x, y, w, h, COLOR_BLACK);
}


void gui_render_freeze_marker(GUI_REND_IMG_T* img)
{
	int16_t x, y;
	int16_t w, h;
	
	// Compute width and height
	w = (uint16_t) round(((float) GUI_FREEZE_MARKER_SIZE * mag_factor));
	h = (uint16_t) round(((float) GUI_FREEZE_MARKER_SIZE * mag_factor));
	
	// Compute the left-top corner
	x = img_w - ((int16_t) round(GUI_FREEZE_MARKER_SIZE * mag_factor)) - 10;
	y = 10;
	
	// Draw a black bounding box
	_draw_rect(img, x - 1, y - 1, w + 2, h + 2, COLOR_BLACK);
	
	// Draw the inner white box
	_draw_fill_rect(img, x, y, w, h, COLOR_WHITE);
}



//
// Internal functions
//

// Linear interpolation magnification of 0.5X
static void _render_image_0_5(gui_img_buf_t* raw, GUI_REND_IMG_T* img, gui_state_t* g)
{
	uint8_t* src1 = raw->y8_data;          // Row 0, 2, 4, ... 2*img_h-2
	uint8_t* src2 = src1 + (img_w*2);      // Row 1, 3, 5, ... 2*img_h-1
	uint16_t row = 0;
	uint16_t col;
	uint32_t s;
	GUI_REND_IMG_T* dst = img;
	
	// Average the 4 surrounding pixels into 1
	while (row<img_h) {
		for (col=0; col<img_w; col++) {
			// Each dest pixel is made up of 4 src pixels
			s = *src1++;
			s += *src2++;
			s += *src1++;
			s += *src2++;
			*dst++ = PALETTE_LOOKUP(s/4);
		}
		src1 += (img_w*2);
		src2 += (img_w*2);
		row += 2;
	}
}


// Pixel magnification of 1.0X
static void _render_image_1_0(gui_img_buf_t* raw, GUI_REND_IMG_T* img, gui_state_t* g)
{
	uint8_t* src = raw->y8_data;
	uint8_t* src_end = src + (GUI_RAW_IMG_W*GUI_RAW_IMG_H);
	
	do {
		*img++ = PALETTE_LOOKUP(*src++);
	} while (src < src_end);
}


// Linear interpolation pixel magnification of 1.5X
static void _render_image_1_5(gui_img_buf_t* raw, GUI_REND_IMG_T* img, gui_state_t* g)
{
	uint8_t* src1 = raw->y8_data;
	uint8_t* src2;
	uint16_t row = 0;
	uint16_t col = 0;
	uint32_t s;
	GUI_REND_IMG_T* dst = img;
	
	// Scale the image 1.5x with an unrolled loop
	while (row<img_h) {
		// Row 0, 3, 6, ... {img_h-3}
		for (col=0; col<img_w; col += 3) {
			// Col 0, 3, 6, ... {img_w-3}
			*dst++ = PALETTE_LOOKUP(*src1);
			
			// Col 1, 4, 7, ... {img_w-2}
			s = *src1++;
			s += *src1;
			*dst++ = PALETTE_LOOKUP(s/2);
			
			// Col 2, 5, 8, ... {img_w-1}
			*dst++ = PALETTE_LOOKUP(*src1++);
		}
		
		// Row 1, 4, 7, ... {img_h-2}
		src2 = src1;
		src1 = src1 - (2*img_w/3);   // src1 = src1 - SRC_WIDTH
		for (col=0; col<img_w; col += 3) {
			// Col 0, 3, 6, ... {img_w-3}
			s = *src1;
			s += *src2;
			*dst++ = PALETTE_LOOKUP(s/2);
			
			// Col 1, 4, 7, ... {img_w-2}
			s = *src1++;
			s += *src2++;
			s += *src1;
			s += *src2;
			*dst++ = PALETTE_LOOKUP(s/4);
			
			// Col 2, 5, 8, ... {img_w-1}
			s = *src1++;
			s += *src2++;
			*dst++ = PALETTE_LOOKUP(s/2);
		}
		
		// Row 2, 5, 8, ... {img_h-1}
		for (col=0; col<img_w; col += 3) {
			// Col 0, 3, 6, ... {img_w-3}
			*dst++ = PALETTE_LOOKUP(*src1);
			
			// Col 1, 4, 7, ... {img_w-2}
			s = *src1++;
			s += *src1;
			*dst++ = PALETTE_LOOKUP(s/2);
			
			// Col 2, 5, 8, ... {img_w-1}
			*dst++ = PALETTE_LOOKUP(*src1++);
		}
		row += 3;
	}
}


static void _render_image_2_0(gui_img_buf_t* raw, GUI_REND_IMG_T* img, gui_state_t* g)
{
	uint8_t* src = raw->y8_data;
	uint16_t src_w = img_w/2;
	uint16_t src_h = img_h/2;
	
	// Corner pixels
	_interp_set_pixel(*src, img, 0, 0);
	_interp_set_pixel(*(src + (src_w-1)), img, img_w-1, 0);
	_interp_set_pixel(*(src + src_w*(src_h-1)), img, 0, img_h-1);
	_interp_set_pixel(*(src + src_w*(src_h-1) + (src_w-1)), img, img_w-1, img_h-1);
	
	// Top/Bottom rows
	_interp_set_outer_row(src, img, src_w, src_h, true);
	_interp_set_outer_row(src, img, src_w, src_h, false);
	
	// Left/Right columns
	_interp_set_outer_col(src, img, src_w, src_h, true);
	_interp_set_outer_col(src, img, src_w, src_h, false);
	
	// Inner pixels
	_interp_set_inner(src, src_w, src_h, img);
}


static void _render_min_marker(gui_img_buf_t* raw, GUI_REND_IMG_T* img)
{
	int16_t x1, xm, x2, y1, y2;
	
	// Compute a bounding box around the marker triangle
	if (is_portrait) {
		x1 = (int16_t) round(img_w - (raw->min_y * mag_factor) - (GUI_MARKER_SIZE/2) * mag_factor);
		xm = (int16_t) round(img_w - (raw->min_y * mag_factor));
		y1 = (int16_t) round(raw->min_x * mag_factor - (GUI_MARKER_SIZE/2) * mag_factor);
	} else {
		x1 = (int16_t) round(raw->min_x * mag_factor - (GUI_MARKER_SIZE/2) * mag_factor);
		xm = (int16_t) round(raw->min_x * mag_factor);
		y1 = (int16_t) round(raw->min_y * mag_factor - (GUI_MARKER_SIZE/2) * mag_factor);
	}
	x2 = x1 + GUI_MARKER_SIZE * mag_factor;
	y2 = y1 + GUI_MARKER_SIZE * mag_factor;
	
	// Draw a white downward facing triangle surrounded by a black triangle for contrast
	_draw_hline(img, x1, x2, y1, COLOR_WHITE);
	_draw_line(img, x1, y1, xm, y2, COLOR_WHITE);
	_draw_line(img, xm, y2, x2, y1, COLOR_WHITE);
	
	x1--;
	y1--;
	x2++;
	y2++;
	
	_draw_hline(img, x1, x2, y1, COLOR_BLACK);
	_draw_line(img, x1, y1, xm, y2, COLOR_BLACK);
	_draw_line(img, xm, y2, x2, y1, COLOR_BLACK);
}


static void _render_max_marker(gui_img_buf_t* raw, GUI_REND_IMG_T* img)
{
	int16_t x1, xm, x2, y1, y2;
	
	// Compute a bounding box around the marker triangle
	if (is_portrait) {
		x1 = (int16_t) round(img_w - (raw->max_y * mag_factor) - (GUI_MARKER_SIZE/2) * mag_factor);
		xm = (int16_t) round(img_w - (raw->max_y * mag_factor));
		y1 = (int16_t) round(raw->max_x * mag_factor - (GUI_MARKER_SIZE/2) * mag_factor);
	} else {
		x1 = (int16_t) round(raw->max_x * mag_factor - (GUI_MARKER_SIZE/2) * mag_factor);
		xm = (int16_t) round(raw->max_x * mag_factor);
		y1 = (int16_t) round(raw->max_y * mag_factor - (GUI_MARKER_SIZE/2) * mag_factor);
	}
	x2 = x1 + GUI_MARKER_SIZE * mag_factor;
	y2 = y1 + GUI_MARKER_SIZE * mag_factor;
	
	// Draw a white upward facing triangle surrounded by a black triangle for contrast
	_draw_hline(img, x1, x2, y2, COLOR_WHITE);
	_draw_line(img, x1, y2, xm, y1, COLOR_WHITE);
	_draw_line(img, xm, y1, x2, y2, COLOR_WHITE);
	
	x1--;
	y1--;
	x2++;
	y2++;
	
	_draw_hline(img, x1, x2, y2, COLOR_BLACK);
	_draw_line(img, x1, y2, xm, y1, COLOR_BLACK);
	_draw_line(img, xm, y1, x2, y2, COLOR_BLACK);
}


static void _draw_hline(GUI_REND_IMG_T* img, int16_t x1, int16_t x2, int16_t y, GUI_REND_IMG_T c)
{
	GUI_REND_IMG_T* imgP;
	
	if ((y < 0) || (y >= img_h)) return;
	
	imgP = img + y*img_w + x1;
	
	while (x1 <= x2) {
		if ((x1 >= 0) && (x1 < img_w)) {
			*imgP = c;
		}
		imgP++;
		x1++;
	}
}


static void _draw_vline(GUI_REND_IMG_T* img, int16_t x, int16_t y1, int16_t y2, GUI_REND_IMG_T c)
{
	GUI_REND_IMG_T* imgP;
	
	if ((x < 0) || (x >= img_w)) return;
	
	imgP = img + y1*img_w + x;
	
	while (y1 <= y2) {
		if ((y1 >= 0) && (y1 < img_h)) {
			*imgP = c;
		}
		imgP += img_w;
		y1++;
	}
}


static void _draw_line(GUI_REND_IMG_T* img, int16_t x1, int16_t y1, int16_t x2, int16_t y2, GUI_REND_IMG_T c)
{
	int16_t dx = abs(x2 - x1);
	int16_t dy = -abs(y2 - y1);
	int16_t err = dx + dy;
	int16_t e2;
	int16_t sx = (x1 < x2) ? 1 : -1;
	int16_t sy = (y1 < y2) ? 1 : -1;
	
	for (;;) {
		if ((x1 >= 0) && (x1 < img_w) && (y1 >= 0) && (y1 < img_h)) {
			*(img + x1 + img_w*y1) = c;
		}
		
		if ((x1 == x2) && (y1 == y2)) break;
		
		e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x1 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y1 += sy;
		}
	}
}


static void _draw_circle(GUI_REND_IMG_T* img, int16_t x0, int16_t y0, int16_t r, GUI_REND_IMG_T c)
{
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	_draw_pixel(img, x0, y0 + r, c);
	_draw_pixel(img, x0, y0 - r, c);
	_draw_pixel(img, x0 + r, y0, c);
	_draw_pixel(img, x0 - r, y0, c);

	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;
		
		_draw_pixel(img, x0 + x, y0 + y, c);
		_draw_pixel(img, x0 - x, y0 + y, c);
		_draw_pixel(img, x0 + x, y0 - y, c);
		_draw_pixel(img, x0 - x, y0 - y, c);
		_draw_pixel(img, x0 + y, y0 + x, c);
		_draw_pixel(img, x0 - y, y0 + x, c);
		_draw_pixel(img, x0 + y, y0 - x, c);
		_draw_pixel(img, x0 - y, y0 - x, c);
	}
}


static void _draw_rect(GUI_REND_IMG_T* img, int16_t x, int16_t y, int16_t w, int16_t h, GUI_REND_IMG_T c)
{
	int16_t x1 = x + w - 1;
	int16_t y1 = y + h - 1;
	
	_draw_hline(img, x, x1, y, c);
	_draw_hline(img, x, x1, y1, c);
	_draw_vline(img, x, y, y1, c);
	_draw_vline(img, x1, y, y1, c);
}


static void _draw_fill_rect(GUI_REND_IMG_T* img, int16_t x, int16_t y, int16_t w, int16_t h, GUI_REND_IMG_T c)
{
	int16_t x1 = x + w - 1;
	int16_t y1 = y + h - 1;
	
	do {
		_draw_hline(img, x, x1, y, c);
	} while (++y < y1);
}


static __inline__ void _draw_pixel(GUI_REND_IMG_T* img, int16_t x, int16_t y, GUI_REND_IMG_T c)
{
	if ((x < 0) || (x >= img_w)) return;
	if ((y < 0) || (y >= img_h)) return;
	*(img + x + y*img_w) = c;
}


/******
 *
 * Linear Interpolation Pixel Doubler
 *
 * Each source pixel is broken into 4 sub-pixels (a-d), shown below.  Each sub-
 * pixel value is based primarily by its source pixel value plus contribution from 
 * surrounding source pixels.  Four source pixels (A-D) are shown.
 *
 *      +---+---+ +---+---+
 *      | a | b | | a | b |
 *      +---A---+ +---B---+
 *      | c | d | | c | d |
 *      +---+---+ +---+---+
 *
 *      +---+---+ +---+---+
 *      | a | b | | a | b |
 *      +---C---+ +---D---+
 *      | c | d | | c | d |
 *      +---+---+ +---+---+
 * 
 * There are three cases to calculate:
 *   1. The four corners of the source array (Aa, Bb, Cc, and Dd here)
 *      - These are simply set to the value of the source pixel (A, B, C and D)
 *   2. The outer edges of the source array (Ab, Ba, Ac, Bd, Ca, Db, Dc, Dc)
 *      - The sub-pixel value is based on two source pixels (the owning pixel and its neighbor)
 *      - The owning pixel contributes more to the sub-pixel by a Scale Factor (SF)
 *      - The sub-pixel = (SF * Owning Pixel + Neighbor Pixel) / DIV
 *      - The divisor scales the sum back to 8-bits = SF + 1
 *   3. The inner sub-pixels (Ad, Bc, Cb, Da)
 *      - The sub-pixel value is based on four source pixels (the owning pixel and its neighbors)
 *      - The owning pixel contributes more to the sub-pixel by a Scale Factor (SF)
 *      - The sub-pixel = (SF * Owning Pixel + 3 Neighbor Pixels) / DIV
 *      - The divisor scales the sum back to 8-bits = SF + 3
 *
 */
 
 
/**
 * Set a single pixel in the segment buffer
 *   d contains source buffer 8-bit value
 *   img points to display buffer
 *   x, y specify position in display buffer
 */
static void _interp_set_pixel(uint8_t src, GUI_REND_IMG_T* img, int x, int y)
{
	*(img + y*img_w + x) = PALETTE_LOOKUP(src);
}


/**
 * Process either the top or bottom row in the destination buffer where each pixel
 * only depends on contributions from two source locations.
 *   src points to the lepton source buffer
 *   img points to the display buffer
 *   first_row indicates top or bottom
 */
static void _interp_set_outer_row(uint8_t* src, GUI_REND_IMG_T* img, int16_t src_w, int16_t src_h, bool first_row)
{
	int x;
	uint8_t A, B, sub_pixel;
	
	// Set the pointers to the start of the row to load
	if (first_row) {
		// Top row starting 1 pixel in (dest)
		img += 1;
	} else {
		// Bottom row starting 1 pixel in (dest)
		src += (src_h-1)*src_w;
		img += (img_h-1)*img_w + 1;
	}
	
	// Inner pixels
	B = *src;
	for (x=0; x<src_w-1; x++) {
		A = B;
		B = *++src;
		
		// Left sub-pixel Ab (top) / Ad (bottom)
		sub_pixel = (SF_DS*A + B) / DIV_DS;
		*img++ = PALETTE_LOOKUP(sub_pixel);
		
		// Right sub-pixel Ba (top) / Bc (bottom)
		sub_pixel = (A + SF_DS*B) / DIV_DS;
		*img++ = PALETTE_LOOKUP(sub_pixel);
	}
}


static void _interp_set_outer_col(uint8_t* src, GUI_REND_IMG_T* img, int16_t src_w, int16_t src_h, bool first_col)
{
	int y;
	uint8_t A, B, sub_pixel;
	
	// Set the pointers to the start of the column to load
	if (first_col) {
		// Left column starting 1 pixel down (dest)
		img += img_w;
	} else {
		// Right column starting 1 pixel down (dest)
		src += src_w - 1;
		img += img_w + (img_w-1);
	}
		
	// Inner pixels
	B = *src;
	for (y=0; y<src_h-1; y++) {
		A = B;
		src += src_w;
		B = *src;
	
		// Top sub-pixel Ac (left) / Ad (right)
		sub_pixel = (SF_DS*A + B) / DIV_DS;
		*img = PALETTE_LOOKUP(sub_pixel);
		img += img_w;
		
		// Bottom sub-pixel Ba (left) / Bb (right)
		sub_pixel = (A + SF_DS*B) / DIV_DS;
		*img = PALETTE_LOOKUP(sub_pixel);
		img += img_w;
	}
}


static void _interp_set_inner(uint8_t* src, int16_t src_w, int16_t src_h, GUI_REND_IMG_T* img)
{
	int x, y;
	uint8_t A, B, C, D, sub_pixel;
	
	// Set the destination pointer to the start of the first inner row
	img += img_w + 1;

	// Loop over inner lines (src_h-1 lines of src_w-1 pixels)
	for (y=0; y<src_h-1; y++) {	
		// Compute all four sub-pixels in the inner section
		B = *src;
		D = *(src + src_w);
		for (x=0; x<src_w-1; x++) {
			A = B;
			C = D;
			src++;
			B = *src;
			D = *(src + src_w);
			
			// Lower right sub-pixel Ad
			sub_pixel = (SF_QS*A + B + C + D) / DIV_QS;
			*img = PALETTE_LOOKUP(sub_pixel);
			
			// Upper right sub-pixel Cb
			sub_pixel = (A + B + SF_QS*C + D) / DIV_QS;
			*(img + img_w) = PALETTE_LOOKUP(sub_pixel);
			img++;
			
			// Lower left sub-pixel Bc
			sub_pixel = (A + SF_QS*B + C + D) / DIV_QS;
			*img = PALETTE_LOOKUP(sub_pixel);
			
			// Upper left sub-pixel Da
  			sub_pixel = (A + B + C + SF_QS*D) / DIV_QS;
			*(img + img_w) = PALETTE_LOOKUP(sub_pixel);
			img++;
		}

		// Next source line, 2 dest lines down, 1-pixel in
		src++;
		img += img_w + 2;
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
