/*
 * Renderers for file images
 *
 * Note about portrait vs landscape rendering: The buffer is treated in a landscape (native
 * Tiny1C) orientation for all drawing routines except draw_char.  This means that the coordinates
 * passed to them must change based on orientation.  draw_char automatically renders differently
 * based on orientation.
 *
 * Copyright 2024 Dan Julio
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
#include <math.h>
#include <string.h>
#include "file_render.h"
#include "font.h"
#include "font7x10.h"



//
// Constants
//
#define COLOR_BLACK       RGB_TO_24BIT(0, 0, 0)



//
// Variables
//
static bool img_is_portrait;
static int16_t img_w;
static int16_t img_h;



//
// Forward declarations for internal functions
//
static void draw_min_marker(t1c_buffer_t* t1c, int16_t n, uint32_t* img);
static void draw_max_marker(t1c_buffer_t* t1c, int16_t n, uint32_t* img);
static void draw_temp(uint32_t* img, int16_t x, int16_t y, uint16_t v, out_state_t* g);
static void draw_hline(uint32_t* img, int16_t x1, int16_t x2, int16_t y, uint32_t c);
static void draw_vline(uint32_t* img, int16_t x, int16_t y1, int16_t y2, uint32_t c);
static void draw_line(uint32_t* img, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t c);
static void draw_circle(uint32_t* img, int16_t x0, int16_t y0, int16_t r, uint32_t c);
static void draw_rect(uint32_t* img, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c);
static void darken_rect(uint32_t* img, int16_t x, int16_t y, int16_t w, int16_t h);
static int16_t draw_landscape_char(uint32_t* img, int16_t x, int16_t y, uint32_t c, const Font_TypeDef *Font);
static int16_t draw_portrait_char(uint32_t* img, int16_t x, int16_t y, uint32_t c, const Font_TypeDef *Font);
static void draw_string(uint32_t* img, int16_t x, int16_t y, const char *str, const Font_TypeDef *Font);
static __inline__ void draw_pixel(uint32_t* img, int16_t x, int16_t y, uint32_t c);



//
// File Render API
//
void file_render_set_orientation(bool is_portrait)
{
	img_is_portrait = is_portrait;
	
	if (is_portrait) {
		img_w = T1C_HEIGHT;
		img_h = T1C_WIDTH;
	} else {
		img_w = T1C_WIDTH;
		img_h = T1C_HEIGHT;
	}
}


void file_render_t1c_data(t1c_buffer_t* t1c, uint32_t* img)
{
	uint16_t* t1cP = t1c->img_data;
	uint32_t diff;
	uint32_t t32;
	
	// Scale and render the raw Tiny1C data into 24-bit RGB (RGB888)
	diff = t1c->y16_max - t1c->y16_min;
	if (diff == 0) diff = 1;
	while (t1cP < (t1c->img_data + (T1C_WIDTH*T1C_HEIGHT))) {
		t32 = ((uint32_t)(*t1cP++ - t1c->y16_min) * 255) / diff;
		*img++ = PALETTE_SAVE_LOOKUP(t32 & 0x000000FF);
	}
}


void file_render_spotmeter(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g)
{
	char buf[10];
	int16_t x, y;
	int16_t d;
	uint16_t c, r;
	uint16_t w, h;
	
	// Center
	c = t1c->spot_point.x;
	r = t1c->spot_point.y;
	
	// Diameter
	d = FILE_IMG_SPOT_SIZE;
	
	// Draw a white circle surrounded by a black circle for contrast on all
	// color palettes
	draw_circle(img, c, r, d/2, FILE_MARKER_COLOR);
	draw_circle(img, c, r, (d+2)/2, COLOR_BLACK);
	
	// Get the temperature string
	if (g->temp_unit_C) {
		sprintf(buf, "%d%cC", (int16_t) round(temp_to_float_temp(t1c->spot_temp, g->temp_unit_C)), FONT7X10_DEGREE_CHAR);
	} else {
		sprintf(buf, "%d%cF", (int16_t) round(temp_to_float_temp(t1c->spot_temp, g->temp_unit_C)), FONT7X10_DEGREE_CHAR);
	}
	
	// Get info about the text string
	w = font_get_string_width(buf, &Font7x10);
	h = Font7x10.font_Height;
	
	if (img_is_portrait) {
		x = (c <= (img_w/2)) ? c + d/2 + 3 : c - d/2 - h - 3;  // right if c < half, left if c > half
		y = r - w/2;
		
		// Blank an area and then draw the text
		darken_rect(img, x-1, y-1, h+2, w+3);
		y += w-1;
		draw_string(img, x, y, buf, &Font7x10);
	} else {
		x = c - w/2;
		y = (r <= (img_h/2)) ? r + d/2 + 3 : r - d/2 - h - 3;  // below if r < half, above if r > half
		
		darken_rect(img, x-2, y-1, w+3, h+2);
		draw_string(img, x, y, buf, &Font7x10);
	}
}


void file_render_min_max_markers(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g)
{
	draw_min_marker(t1c, FILE_IMG_MARKER_SIZE, img);
	draw_max_marker(t1c, FILE_IMG_MARKER_SIZE, img);
}


void file_render_region_marker(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g)
{
	int16_t x, y;
	uint16_t w, h;
	
	// Start coordinates
	x = (int16_t) t1c->region_points.start_point.x;
	y = (int16_t) t1c->region_points.start_point.y;
	
	// Dimensions
	w = (int16_t) t1c->region_points.end_point.x - x + 1;
	h = (int16_t) t1c->region_points.end_point.y - y + 1;
	
	// Draw a white bounding box surrounded by a black bounding box for contrast
	// on all color palettes
	draw_rect(img, x, y, w, h, FILE_MARKER_COLOR);	
	draw_rect(img, x-1, y-1, w+2, h+2, COLOR_BLACK);
}


void file_render_region_temps(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g)
{
	char buf[24];                           // "-xxx / -xxx / -xxx°C" + null + safety
	int min, avg, max;
	int16_t x, y;
	int16_t w, h;
	
	min = round(temp_to_float_temp(t1c->region_temp_info.temp_info_value.min_temp, g->temp_unit_C));
	avg = round(temp_to_float_temp(t1c->region_temp_info.temp_info_value.ave_temp, g->temp_unit_C));
	max = round(temp_to_float_temp(t1c->region_temp_info.temp_info_value.max_temp, g->temp_unit_C));
	
	if (g->temp_unit_C) {
		sprintf(buf, "%d / %d / %d%cC", min, avg, max, FONT7X10_DEGREE_CHAR);
	} else {
		sprintf(buf, "%d / %d / %d%cF", min, avg, max, FONT7X10_DEGREE_CHAR);
	}
	
	w = font_get_string_width(buf, &Font7x10) + 1;
	h = Font7x10.font_Height;
	
	if (img_is_portrait) {
		// Blank an area and then draw the text
		x = 0;
		y = 0;
		darken_rect(img, x, y, FILE_IMG_REG_TEXT_HEIGHT, w+2);
		
		x = (FILE_IMG_REG_TEXT_HEIGHT - h) / 2;
		y = w-1;
		draw_string(img, x, y, buf, &Font7x10);
	} else {
		x = img_w - w;
		y = 0;		
		darken_rect(img, x-2, y, w+2, FILE_IMG_REG_TEXT_HEIGHT);
		
		y += (FILE_IMG_REG_TEXT_HEIGHT - h) / 2;
		draw_string(img, x, y, buf, &Font7x10);
	}
}


void file_render_palette(uint32_t* img, out_state_t* g)
{
	float delta;
	float cur;
	int i;
	int16_t l;
	
	// Compute the palette length
	l = img_h - 2*FILE_IMG_PAL_TEXT_HEIGHT;
	
	// Draw the palette from top to bottom (warm to cold)
	delta = 255.0 / (float) -l;
	cur = 255.0;
	
	if (img_is_portrait) {
		darken_rect(img, FILE_IMG_PAL_TEXT_HEIGHT, img_w-FILE_IMG_PALETTE_WIDTH, l, FILE_IMG_PALETTE_WIDTH);
		
		for (int16_t x=FILE_IMG_PAL_TEXT_HEIGHT; x<(FILE_IMG_PAL_TEXT_HEIGHT+l); x++) {
			i = round(cur);
			if (i < 0) i = 0;
			draw_vline(img, x, T1C_HEIGHT-FILE_IMG_CMAP_WIDTH, T1C_HEIGHT-1, PALETTE_SAVE_LOOKUP(i));
			cur += delta;
		}
	} else {
		darken_rect(img, 0, FILE_IMG_PAL_TEXT_HEIGHT, FILE_IMG_PALETTE_WIDTH, l);
		
		for (int16_t y=FILE_IMG_PAL_TEXT_HEIGHT; y<(FILE_IMG_PAL_TEXT_HEIGHT+l); y++) {
			i = round(cur);
			if (i < 0) i = 0;
			draw_hline(img, 0, FILE_IMG_CMAP_WIDTH-1, y, PALETTE_SAVE_LOOKUP(i));
			cur += delta;
		}
	}
}


void file_render_palette_marker(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g)
{
	float offset;
	int16_t o;
	int16_t x_offset, y_offset;
	
	// Compute the scaled temperature location from the high temp
	o = img_h - 2*FILE_IMG_PAL_TEXT_HEIGHT;
	offset = ((float)(t1c->max_min_temp_info.max_temp - t1c->spot_temp) / 
	          (float) (t1c->max_min_temp_info.max_temp - t1c->max_min_temp_info.min_temp))
	         * o;

	if (img_is_portrait) {
		// Compute the pixel offset inside the CMAP region
		x_offset = FILE_IMG_PAL_TEXT_HEIGHT + round(offset);
		y_offset = T1C_HEIGHT - FILE_IMG_PALETTE_MRK_X - FILE_IMG_PALETTE_MRK_W;
		
		// Draw the marker
		draw_vline(img, x_offset, y_offset, y_offset + FILE_IMG_PALETTE_MRK_W - 1, FILE_MARKER_COLOR);
	} else {
		x_offset = FILE_IMG_PALETTE_MRK_X;
		y_offset = FILE_IMG_PAL_TEXT_HEIGHT + round(offset);
		
		draw_hline(img, x_offset, x_offset + FILE_IMG_PALETTE_MRK_W - 1, y_offset, FILE_MARKER_COLOR);
	}
}


void file_render_min_max_temps(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g)
{
	if (img_is_portrait) {
		draw_temp(img, 0, T1C_HEIGHT - 1, t1c->max_min_temp_info.max_temp, g);
		draw_temp(img, T1C_WIDTH - FILE_IMG_PAL_TEXT_HEIGHT, T1C_HEIGHT - 1, t1c->max_min_temp_info.min_temp, g);
	} else {
		draw_temp(img, 0, 0, t1c->max_min_temp_info.max_temp, g);
		draw_temp(img, 0, T1C_HEIGHT - FILE_IMG_PAL_TEXT_HEIGHT, t1c->max_min_temp_info.min_temp, g);
	}
}


void file_render_env_info(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g)
{
	char buf[23];                        // ">-xxx°F xxx% xx' xx"" + null + safety
	int n = 0;
	int16_t x, y;
	int16_t w, h;
	
	// Environmental info displayed if available
	if (g->use_auto_ambient && (t1c->amb_temp_valid || t1c->amb_hum_valid || t1c->distance_valid)) {
		// Indicate that we are using ambient correction
		buf[0] = '>';
		n = 1;
	}
	if (t1c->amb_temp_valid) {
		int t = (int) t1c->amb_temp;
		if (g->temp_unit_C) {
			sprintf(&buf[n], "%d%cC ", t, FONT7X10_DEGREE_CHAR);
		} else {
			t = t * 9.0 / 5.0 + 32.0;
			sprintf(&buf[n], "%d%cF ", t, FONT7X10_DEGREE_CHAR);
		}
		n = strlen(buf);
	}
	if (t1c->amb_hum_valid) {
		sprintf(&buf[n], "%u%% ", t1c->amb_hum);
		n = strlen(buf);
	}
	if (t1c->distance_valid) {
		if (g->temp_unit_C) {
			float d = (float) t1c->distance / 100.0;         // cm -> m
			sprintf(&buf[n], "%1.2fm", d);
		} else {
			float d = (float) t1c->distance / (2.54 * 12);   // cm -> feet
			int ft = floor(d);
			int in = round(((float) d - ft) * 12);
			if (in == 12) {
				in = 0;
				ft += 1;
			}
			sprintf(&buf[n], "%d' %d\"", ft, in);
		}
	}
	
	if (n != 0) {
		w = font_get_string_width(buf, &Font7x10) + 1;
		h = Font7x10.font_Height;
		
		if (img_is_portrait) {
			x = T1C_WIDTH - FILE_IMG_ENV_TEXT_HEIGHT;
			y = 0;
			darken_rect(img, x, y, FILE_IMG_ENV_TEXT_HEIGHT, w+2);
			
			// Set y and offset x in text area and draw string
			x += (FILE_IMG_ENV_TEXT_HEIGHT - h) / 2;
			y = w-1;
			draw_string(img, x, y, buf, &Font7x10);
		} else {
			x = img_w - w;
			y = img_h - FILE_IMG_ENV_TEXT_HEIGHT;
			darken_rect(img, x-3, y, w+3, FILE_IMG_ENV_TEXT_HEIGHT);
			
			// Offset y in text area and draw string
			y += (FILE_IMG_ENV_TEXT_HEIGHT - h) / 2;
			draw_string(img, x, y, buf, &Font7x10);
		}
	}
}


//
// Internal functions
//
static void draw_min_marker(t1c_buffer_t* t1c, int16_t n, uint32_t* img)
{
	int16_t m, x1, x2, y1, y2;
	
	if (img_is_portrait) {
		// Compute a bounding box around the marker triangle
		x1 = t1c->max_min_temp_info.min_temp_point.x - (n/2);
		x2 = x1 + n;
		y1 = t1c->max_min_temp_info.min_temp_point.y - (n/2);
		m  = t1c->max_min_temp_info.min_temp_point.y;
		y2 = y1 + n;
		
		// Draw a white right facing triangle surrounded by a black triangle for contrast
		draw_vline(img, x1, y1, y2, FILE_MARKER_COLOR);
		draw_line(img, x1, y1, x2, m, FILE_MARKER_COLOR);
		draw_line(img, x1, y2, x2, m, FILE_MARKER_COLOR);
		
		x1--;
		y1--;
		x2++;
		y2++;
		
		draw_vline(img, x1, y1, y2, COLOR_BLACK);
		draw_line(img, x1, y1, x2, m, COLOR_BLACK);
		draw_line(img, x1, y2, x2, m, COLOR_BLACK);
	} else {
		// Compute a bounding box around the marker triangle
		x1 = t1c->max_min_temp_info.min_temp_point.x - (n/2);
		m  = t1c->max_min_temp_info.min_temp_point.x;
		x2 = x1 + n;
		y1 = t1c->max_min_temp_info.min_temp_point.y - (n/2);
		y2 = y1 + n;
	
		// Draw a white downward facing triangle surrounded by a black triangle for contrast
		draw_hline(img, x1, x2, y1, FILE_MARKER_COLOR);
		draw_line(img, x1, y1, m, y2, FILE_MARKER_COLOR);
		draw_line(img, m, y2, x2, y1, FILE_MARKER_COLOR);
		
		x1--;
		y1--;
		x2++;
		y2++;
		
		draw_hline(img, x1, x2, y1, COLOR_BLACK);
		draw_line(img, x1, y1, m, y2, COLOR_BLACK);
		draw_line(img, m, y2, x2, y1, COLOR_BLACK);
	}
}


static void draw_max_marker(t1c_buffer_t* t1c, int16_t n, uint32_t* img)
{
	int16_t m, x1, x2, y1, y2;
	
	if (img_is_portrait) {
		// Compute a bounding box around the marker triangle
		x1 = t1c->max_min_temp_info.max_temp_point.x - (n/2);
		x2 = x1 + n;
		y1 = t1c->max_min_temp_info.max_temp_point.y - (n/2);
		m  = t1c->max_min_temp_info.max_temp_point.y;
		y2 = y1 + n;
		
		// Draw a white left facing triangle surrounded by a black triangle for contrast
		draw_line(img, x1, m, x2, y1, FILE_MARKER_COLOR);
		draw_line(img, x1, m, x2, y2, FILE_MARKER_COLOR);
		draw_vline(img, x2, y1, y2, FILE_MARKER_COLOR);
		
		x1--;
		y1--;
		x2++;
		y2++;
		
		draw_line(img, x1, m, x2, y1, COLOR_BLACK);
		draw_line(img, x1, m, x2, y2, COLOR_BLACK);
		draw_vline(img, x2, y1, y2, COLOR_BLACK);
	} else {
		// Compute a bounding box around the marker triangle
		x1 = t1c->max_min_temp_info.max_temp_point.x - (n/2);
		m  = t1c->max_min_temp_info.max_temp_point.x;
		x2 = x1 + n;
		y1 = t1c->max_min_temp_info.max_temp_point.y - (n/2);
		y2 = y1 + n;
	
		// Draw a white upward facing triangle surrounded by a black triangle for contrast
		draw_hline(img, x1, x2, y2, FILE_MARKER_COLOR);
		draw_line(img, x1, y2, m, y1, FILE_MARKER_COLOR);
		draw_line(img, m, y1, x2, y2, FILE_MARKER_COLOR);
		
		x1--;
		y1--;
		x2++;
		y2++;
		
		draw_hline(img, x1, x2, y2, COLOR_BLACK);
		draw_line(img, x1, y2, m, y1, COLOR_BLACK);
		draw_line(img, m, y1, x2, y2, COLOR_BLACK);
	}
}


static void draw_temp(uint32_t* img, int16_t x, int16_t y, uint16_t v, out_state_t* g)
{
	char buf[8];
	uint16_t w, h;
	
	// Get the temperature string
	sprintf(buf, "%d", (int16_t) round(temp_to_float_temp(v, g->temp_unit_C)));
	
	// Get attributes for text string
	w = font_get_string_width(buf, &Font7x10);
	h = Font7x10.font_Height;
		
	// Blank the text area and draw text
	if (img_is_portrait) {
		darken_rect(img, x, y-FILE_IMG_PAL_TEXT_WIDTH+1, FILE_IMG_PAL_TEXT_HEIGHT, FILE_IMG_PAL_TEXT_WIDTH);
		
		// Offset x in text area
		x += (FILE_IMG_PAL_TEXT_HEIGHT - h) / 2;
		draw_string(img, x, y - (FILE_IMG_PAL_TEXT_WIDTH-w)/2, buf, &Font7x10);
	} else {
		darken_rect(img, x, y, FILE_IMG_PAL_TEXT_WIDTH, FILE_IMG_PAL_TEXT_HEIGHT);
		
		// Offset y in text area
		y += (FILE_IMG_PAL_TEXT_HEIGHT - h) / 2;
		draw_string(img, x + (FILE_IMG_PAL_TEXT_WIDTH-w)/2, y, buf, &Font7x10);
	}
}


static void draw_hline(uint32_t* img, int16_t x1, int16_t x2, int16_t y, uint32_t c)
{
	uint32_t* imgP;
	
	if (x1 < 0)
		x1 = 0;
	if (x2 > (T1C_WIDTH-1))
		x2 = T1C_WIDTH-1;
	if ((y < 0) || (y > (T1C_HEIGHT-1))) return;
	
	imgP = img + y*T1C_WIDTH + x1;
	
	while (x1++ <= x2) {
		*imgP++ = c;
	}
}


static void draw_vline(uint32_t* img, int16_t x, int16_t y1, int16_t y2, uint32_t c)
{
	uint32_t* imgP;
	
	if ((x < 0) || (x > (T1C_WIDTH-1))) return;
	if (y1 < 0)
		y1 = 0;
	if (y2 > (T1C_HEIGHT-1))
		y2 = T1C_HEIGHT-1;
	
	imgP = img + y1*T1C_WIDTH + x;
	
	while (y1++ <= y2) {
		*imgP = c;
		imgP += T1C_WIDTH;
	}
}


static void draw_line(uint32_t* img, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t c)
{
	int16_t dx = abs(x2 - x1);
	int16_t dy = -abs(y2 - y1);
	int16_t err = dx + dy;
	int16_t e2;
	int16_t sx = (x1 < x2) ? 1 : -1;
	int16_t sy = (y1 < y2) ? 1 : -1;
	
	for (;;) {
		draw_pixel(img, x1, y1, c);
		
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


static void draw_circle(uint32_t* img, int16_t x0, int16_t y0, int16_t r, uint32_t c) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	draw_pixel(img, x0, y0 + r, c);
	draw_pixel(img, x0, y0 - r, c);
	draw_pixel(img, x0 + r, y0, c);
	draw_pixel(img, x0 - r, y0, c);

	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;
		
		draw_pixel(img, x0 + x, y0 + y, c);
		draw_pixel(img, x0 - x, y0 + y, c);
		draw_pixel(img, x0 + x, y0 - y, c);
		draw_pixel(img, x0 - x, y0 - y, c);
		draw_pixel(img, x0 + y, y0 + x, c);
		draw_pixel(img, x0 - y, y0 + x, c);
		draw_pixel(img, x0 + y, y0 - x, c);
		draw_pixel(img, x0 - y, y0 - x, c);
	}
}


static void draw_rect(uint32_t* img, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c)
{
	if (x < 0) {
		w += x;
		if (w <= 0) return;
		x = 0;
	}
	if ((x+w) > T1C_WIDTH)
		w -= (x+w) - T1C_WIDTH;
	if (y < 0)
		y = 0;
	if ((y+h) > T1C_HEIGHT) {
		h += y;
		if (h <= 0) return;
		h -= (y+h) - T1C_HEIGHT;
	}
	
	// Top and bottom lines
	draw_hline(img, x, x+w-1, y, c);
	draw_hline(img, x, x+w-1, y+h-1, c);
	
	// Left and right lines
	draw_vline(img, x, y, y+h-1, c);
	draw_vline(img, x+w-1, y, y+h-1, c);
}


static void darken_rect(uint32_t* img, int16_t x, int16_t y, int16_t w, int16_t h)
{
	uint8_t r, g, b;
	int16_t x1, y1;
	uint32_t c;
	uint32_t* imgP;
	
	if (x < 0) {
		w += x;
		if (w <= 0) return;
		x = 0;
	}
	if ((x+w) > T1C_WIDTH)
		w -= (x+w) - T1C_WIDTH;
	if (y < 0)
		y = 0;
	if ((y+h) > T1C_HEIGHT) {
		h += y;
		if (h <= 0) return;
		h -= (y+h) - T1C_HEIGHT;
	}
	
	y1 = y;
	do {
		x1 = x;
		imgP = img + y1*T1C_WIDTH + x1;
		while (x1++ < (x+w)) {
			c = *imgP;
			b = ((c >> 16) & 0xFF) >> 1;
			g = ((c >> 8) & 0xFF) >> 1;
			r = (c & 0xFF) >> 1;
			*imgP++ = RGB_TO_24BIT(r, g, b);
		}
	} while (++y1 < (y+h));
}


static int16_t draw_landscape_char(uint32_t* img, int16_t x, int16_t y, uint32_t c, const Font_TypeDef *Font)
{
	uint16_t pX;
	uint16_t pY;
	uint8_t tmpCh;
	uint8_t bL;
	const uint8_t *pCh;

	// If the specified character code is out of bounds should substitute the code of the "unknown" character
	if ((c < Font->font_MinChar) || (c > Font->font_MaxChar)) c = Font->font_UnknownChar;

	// Pointer to the first byte of character in font data array
	pCh = &Font->font_Data[(c - Font->font_MinChar) * Font->font_BPC];

	// Draw character
	if (Font->font_Scan == FONT_V) {
		// Vertical pixels order
		if (Font->font_Height < 9) {
			// Height is 8 pixels or less (one byte per column)
			pX = x;
			while (pX < x + Font->font_Width) {
				pY = y;
				tmpCh = *pCh++;
				while (tmpCh) {
					if (tmpCh & 0x01) {
						draw_pixel(img, pX, pY, FILE_TEXT_COLOR);
					}
					tmpCh >>= 1;
					pY++;
				}
				pX++;
			}
		} else {
			// Height is more than 8 pixels (several bytes per column)
			pX = x;
			while (pX < x + Font->font_Width) {
				pY = y;
				while (pY < y + Font->font_Height) {
					bL = 8;
					tmpCh = *pCh++;
					if (tmpCh) {
						while (bL) {
							if (tmpCh & 0x01) {
								draw_pixel(img, pX, pY, FILE_TEXT_COLOR);
							}
							tmpCh >>= 1;
							if (tmpCh) {
								pY++;
								bL--;
							} else {
								pY += bL;
								break;
							}
						}
					} else {
						pY += bL;
					}
				}
				pX++;
			}
		}
	} else {
		// Horizontal pixels order
		if (Font->font_Width < 9) {
			// Width is 8 pixels or less (one byte per row)
			pY = y;
			while (pY < y + Font->font_Height) {
				pX = x;
				tmpCh = *pCh++;
				while (tmpCh) {
					if (tmpCh & 0x01) {
						draw_pixel(img, pX, pY, FILE_TEXT_COLOR);
					}
					tmpCh >>= 1;
					pX++;
				}
				pY++;
			}
		} else {
			// Width is more than 8 pixels (several bytes per row)
			pY = y;
			while (pY < y + Font->font_Height) {
				pX = x;
				while (pX < x + Font->font_Width) {
					bL = 8;
					tmpCh = *pCh++;
					if (tmpCh) {
						while (bL) {
							if (tmpCh & 0x01) {
								draw_pixel(img, pX, pY, FILE_TEXT_COLOR);
							}
							tmpCh >>= 1;
							if (tmpCh) {
								pX++;
								bL--;
							} else {
								pX += bL;
								break;
							}
						}
					} else {
						pX += bL;
					}
				}
				pY++;
			}
		}
	}

	return Font->font_Width + 1;
}


static int16_t draw_portrait_char(uint32_t* img, int16_t x, int16_t y, uint32_t c, const Font_TypeDef *Font)
{
	uint16_t pX;
	uint16_t pY;
	uint8_t tmpCh;
	uint8_t bL;
	const uint8_t *pCh;

	// If the specified character code is out of bounds should substitute the code of the "unknown" character
	if ((c < Font->font_MinChar) || (c > Font->font_MaxChar)) c = Font->font_UnknownChar;

	// Pointer to the first byte of character in font data array
	pCh = &Font->font_Data[(c - Font->font_MinChar) * Font->font_BPC];

	// Draw character
	if (Font->font_Scan == FONT_V) {
		// Vertical pixels order
		if (Font->font_Height < 9) {
			// Height is 8 pixels or less (one byte per column)
			pY = y;
			while (pY > y - Font->font_Height) {
				pX = x;
				tmpCh = *pCh++;
				while (tmpCh) {
					if (tmpCh & 0x01) {
						draw_pixel(img, pX, pY, FILE_TEXT_COLOR);
					}
					tmpCh >>= 1;
					pX++;
				}
				pY--;
			}
		} else {
			// Height is more than 8 pixels (several bytes per column)
			pY = y;
			while (pY > y - Font->font_Height) {
				pX = x;
				while (pX < x + Font->font_Width) {
					bL = 8;
					tmpCh = *pCh++;
					if (tmpCh) {
						while (bL) {
							if (tmpCh & 0x01) {
								draw_pixel(img, pY, pX, FILE_TEXT_COLOR);
							}
							tmpCh >>= 1;
							if (tmpCh) {
								pX++;
								bL--;
							} else {
								pX += bL;
								break;
							}
						}
					} else {
						pX += bL;
					}
				}
				pY--;
			}
		}
	} else {
		// Horizontal pixels order
		if (Font->font_Width < 9) {
			// Width is 8 pixels or less (one byte per row)
			pX = x;
			while (pX < x + Font->font_Height) {
				pY = y;
				tmpCh = *pCh++;
				while (tmpCh) {
					if (tmpCh & 0x01) {
						draw_pixel(img, pX, pY, FILE_TEXT_COLOR);
					}
					tmpCh >>= 1;
					pY--;
				}
				pX++;
			}
		} else {
			// Width is more than 8 pixels (several bytes per row)
			pX = x;
			while (pX < x + Font->font_Height) {
				pY = y;
				while (pY > y + Font->font_Width) {
					bL = 8;
					tmpCh = *pCh++;
					if (tmpCh) {
						while (bL) {
							if (tmpCh & 0x01) {
								draw_pixel(img, pX, pY, FILE_TEXT_COLOR);
							}
							tmpCh >>= 1;
							if (tmpCh) {
								pY--;
								bL--;
							} else {
								pY -= bL;
								break;
							}
						}
					} else {
						pY -= bL;
					}
				}
				pX++;
			}
		}
	}

	return Font->font_Width + 1;
}


static void draw_string(uint32_t* img, int16_t x, int16_t y, const char *str, const Font_TypeDef *Font)
{
	if (img_is_portrait) {
		int16_t pY = y;
		
		while (*str) {
			pY -= draw_portrait_char(img, x, pY, *str++, Font);
			if (pY < 0) break;
		}
	} else {
		uint16_t pX = x;
		uint16_t eX = img_w - Font->font_Width - 1;
		
		while (*str) {
			pX += draw_landscape_char(img, pX, y, *str++, Font);
			if (pX > eX) break;
		}
	}
}


static __inline__ void draw_pixel(uint32_t* img, int16_t x, int16_t y, uint32_t c)
{
	if ((x < 0) || (x > (T1C_WIDTH-1))) return;
	if ((y < 0) || (y > (T1C_HEIGHT-1))) return;
	*(img + x + y*T1C_WIDTH) = c;
}
