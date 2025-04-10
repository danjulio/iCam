/*
 * Renderers for Tiny1c images, text, spot meter and min/max markers
 *
 * Copyright 2020-2024 Dan Julio
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include <math.h>
#include <string.h>
#include "vid_render.h"
#include "font.h"
#include "font7x10.h"
#include "esp_ota_ops.h"



//
// Constants
//
#define CLIP_REGION_ALL   0
#define CLIP_REGION_CMAP  1
#define CLIP_REGION_TMRK  2
#define CLIP_REGION_IMAGE 3



//
// Variables
//
static int16_t clip_x1;
static int16_t clip_y1;
static int16_t clip_x2;
static int16_t clip_y2;



//
// Forward declarations for internal functions
//
static void set_clip_region(int region);
static void draw_min_marker(t1c_buffer_t* t1c, int16_t n, uint8_t* img);
static void draw_max_marker(t1c_buffer_t* t1c, int16_t n, uint8_t* img);
static void draw_temp(uint8_t* img, int16_t x, int16_t y, uint16_t v, out_state_t* g);
static void draw_hline(uint8_t* img, int16_t x1, int16_t x2, int16_t y, uint8_t c);
static void draw_vline(uint8_t* img, int16_t x, int16_t y1, int16_t y2, uint8_t c);
static void draw_line(uint8_t* img, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t c);
static void draw_circle(uint8_t* img, int16_t x0, int16_t y0, int16_t r, uint8_t c);
static void draw_rect(uint8_t* img, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t c);
static void draw_fill_rect(uint8_t* img, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t c);
static int16_t draw_char(uint8_t* img, int16_t x, int16_t y, uint8_t c, const Font_TypeDef *Font);
static void draw_string(uint8_t* img, int16_t x, int16_t y, const char *str, const Font_TypeDef *Font);
static __inline__ void draw_pixel(uint8_t* img, int16_t x, int16_t y, uint8_t c);



//
// Vid Render API
//
void vid_render_test_pattern(uint8_t* img)
{
	char buf[48];
	int i;
	int n;
	const esp_app_desc_t* app_desc;
	
	set_clip_region(CLIP_REGION_ALL);
	
	// Clear the frame buffer
	draw_fill_rect(img, 0, 0, IMG_BUF_WIDTH, IMG_BUF_HEIGHT, 0x00);
	
	// Bounding box
	draw_rect(img, 0, 0, IMG_BUF_WIDTH, IMG_BUF_HEIGHT, 0xFF);
	
	// Grayscale rectangles at the top, horizontal lines at the bottom
	n = (IMG_BUF_WIDTH - 16) / 16;
	for (i=0; i<16; i++) {
		draw_fill_rect(img, i * n + 8, 4, n, n, i*16);
		draw_vline(img, i * n + 8 + n/2, IMG_BUF_HEIGHT - n - 4, IMG_BUF_HEIGHT - 4, 0xFF);
	}
	
	// Centered circle
	draw_circle(img, IMG_BUF_WIDTH/2, IMG_BUF_HEIGHT/2, (IMG_BUF_HEIGHT - 40)/2, 0xFF);	
	
	// Draw some text
	app_desc = esp_app_get_description();
	sprintf(buf, "iCamMini v%s", app_desc->version);
	n = font_get_string_width(buf, &Font7x10);
	draw_string(img, (IMG_BUF_WIDTH - n)/2, IMG_BUF_HEIGHT/2 - Font7x10.font_Height - 2, buf, &Font7x10);
	strcpy(buf, "(c) 2024 Dan Julio");
	n = font_get_string_width(buf, &Font7x10);
	draw_string(img, (IMG_BUF_WIDTH - n)/2, IMG_BUF_HEIGHT/2 + 2, buf, &Font7x10);
	
	// Vertical lines on either side
	n = (IMG_BUF_HEIGHT - 40) / 16;
	for (i=0; i<16; i++) {
		draw_hline(img, 4, 4 + n, i*n + 20 + 8, 0xFF);
		draw_hline(img, IMG_BUF_WIDTH - n - 4, IMG_BUF_WIDTH - 4, i*n + 20 + 8, 0xFF);
	}
}


void vid_render_t1c_data(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g)
{
	uint8_t render_palette_mod;    // Either 0x00 or 0xFF, used to invert image (white-hot -> black-hot)
	uint8_t* imgP = img;
	uint16_t* t1cP = t1c->img_data;
	uint32_t diff;
	uint32_t t32;
	uint32_t x, y;
	
	// Don't worry about setting a clip region, this only generates valid x,y by design
	
	// Setup the global palette modifier
	render_palette_mod = (g->vid_palette_index == 1) ? 0xFF : 0x00;

	diff = t1c->y16_max - t1c->y16_min;
	if (diff == 0) diff = 1;
	y = T1C_HEIGHT;
	while (y--) {
		x = T1C_WIDTH;
		imgP += IMG_BUF_CMAP_WIDTH;
		while (x--) {
			t32 = ((uint32_t)(*t1cP++ - t1c->y16_min) * 255) / diff;
			*imgP++ = ((t32 > 255) ? 255 : (uint8_t) t32) ^ render_palette_mod;
		}
	}
}


void vid_render_palette(uint8_t* img, out_state_t* g)
{
	float delta;
	float cur;
	int16_t y;
	uint8_t c;
	
	set_clip_region(CLIP_REGION_CMAP);
	
	// Blank the entire area
	draw_fill_rect(img, 0, 0, IMG_BUF_CMAP_WIDTH, IMG_BUF_HEIGHT, CMAP_TEXT_BG_COLOR);
	
	// Draw the palette from top to bottom (warm to cold)
	if (g->vid_palette_index == 1) {
		// Black hot palette
		delta = 255.0 / (float) IMG_BUF_CMAP_HEIGHT;
		cur = 0;
	} else {
		// White hot palette
		delta = 255.0 / (float) -IMG_BUF_CMAP_HEIGHT;
		cur = 255.0;
	}
	
	for (y=IMG_BUF_BATT_RGN_H+IMG_BUF_CMAP_TEXT_H; y<(IMG_BUF_CMAP_HEIGHT+IMG_BUF_BATT_RGN_H+IMG_BUF_CMAP_TEXT_H); y++) {
		c = round(cur);
		draw_hline(img, PALETTE_BAR_X_OFFSET, PALETTE_BAR_X_OFFSET+PALETTE_BAR_WIDTH, y, c);
		cur += delta;
	}
}


void vid_render_spotmeter(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g)
{
	char buf[10];
	int16_t x, y;
	int16_t d;
	uint16_t c, r;
	uint16_t w, h;
	
	set_clip_region(CLIP_REGION_IMAGE);
	
	// Center
	c = t1c->spot_point.x + IMG_BUF_CMAP_WIDTH;
	r = t1c->spot_point.y;
	
	// Diameter
	d = IMG_SPOT_SIZE;
	
	// Draw a white circle surrounded by a black circle for contrast on all
	// color palettes
	draw_circle(img, c, r, d/2, MARKER_COLOR);
	draw_circle(img, c, r, (d+2)/2, 0x00);
	
	// Get the temperature string
	if (g->temp_unit_C) {
		sprintf(buf, "%d%cC", (int16_t) round(temp_to_float_temp(t1c->spot_temp, g->temp_unit_C)), FONT7X10_DEGREE_CHAR);
	} else {
		sprintf(buf, "%d%cF", (int16_t) round(temp_to_float_temp(t1c->spot_temp, g->temp_unit_C)), FONT7X10_DEGREE_CHAR);
	}
	
	// Compute upper left corner for text string
	w = font_get_string_width(buf, &Font7x10);
	h = Font7x10.font_Height;
	x = c - w/2;
	y = (r <= (IMG_BUF_HEIGHT/2)) ? r + d/2 + 3 : r - d/2 - h - 3;  // below if r < half, above if r > half
	
	// Blank an area and the draw the text
	draw_fill_rect(img, x-1, y-1, w+2, h+2, IMG_TEXT_BG_COLOR);
	draw_string(img, x, y, buf, &Font7x10);
}


void vid_render_min_max_markers(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g)
{
	set_clip_region(CLIP_REGION_IMAGE);
	
	if (g->vid_palette_index == 1) {
		// Black hot palette
		draw_min_marker(t1c, IMG_MARKER_SIZE_S, img);
		draw_max_marker(t1c, IMG_MARKER_SIZE_L, img);
	} else {
		// White hot palette
		draw_min_marker(t1c, IMG_MARKER_SIZE_L, img);
		draw_max_marker(t1c, IMG_MARKER_SIZE_S, img);
	}
}


void vid_render_region_marker(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g)
{
	int16_t x, y;
	uint16_t w, h;
	
	set_clip_region(CLIP_REGION_IMAGE);
	
	// Start coordinates
	x = (int16_t) t1c->region_points.start_point.x + IMG_BUF_CMAP_WIDTH;
	y = (int16_t) t1c->region_points.start_point.y;
	
	// Dimensions
	w = (int16_t) t1c->region_points.end_point.x - x + 1;
	h = (int16_t) t1c->region_points.end_point.y - y + 1;
	
	// Draw a white bounding box surrounded by a black bounding box for contrast
	// on all color palettes
	draw_rect(img, x, y, w, h, MARKER_COLOR);	
	draw_rect(img, x-1, y-1, w+2, h+2, 0x00);
}


void vid_render_region_temps(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g)
{
	char buf[24];                           // "-xxx / -xxx / -xxx°C" + null + safety
	int min, avg, max;
	int16_t x, y;
	int16_t w, h;
	
	set_clip_region(CLIP_REGION_IMAGE);
	
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
	x = IMG_BUF_WIDTH - w;
	y = 0;
	
	// Blank an area for the text
	draw_fill_rect(img, x-1, y, w+1, IMG_BUF_REG_TEXT_H, IMG_TEXT_BG_COLOR);
	
	// Offset y in text area and draw string
	y += (IMG_BUF_REG_TEXT_H - h) / 2;
	draw_string(img, x, y, buf, &Font7x10);
}


void vid_render_min_max_temps(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g)
{
	set_clip_region(CLIP_REGION_CMAP);
	
	draw_temp(img, 0, IMG_BUF_BATT_RGN_H, t1c->max_min_temp_info.max_temp, g);
	draw_temp(img, 0, T1C_HEIGHT - IMG_BUF_CMAP_TEXT_H, t1c->max_min_temp_info.min_temp, g);
}


void vid_render_palette_marker(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g)
{
	float offset;
	int16_t x_offset, y_offset;
	
	set_clip_region(CLIP_REGION_TMRK);
	
	// Compute the scaled temperature location from the high temp
	offset = ((float)(t1c->max_min_temp_info.max_temp - t1c->spot_temp) / 
	          (float) (t1c->max_min_temp_info.max_temp - t1c->max_min_temp_info.min_temp))
	         * IMG_BUF_CMAP_HEIGHT;
	
	// Compute the pixel offset inside the CMAP region
	x_offset = PALETTE_BAR_X_OFFSET + PALETTE_BAR_WIDTH + 2;
	y_offset = IMG_BUF_BATT_RGN_H + IMG_BUF_CMAP_TEXT_H + round(offset);
	
	// Erase all possible locations of the previous marker
	draw_fill_rect(img, x_offset, IMG_BUF_BATT_RGN_H + IMG_BUF_CMAP_TEXT_H, PALETTE_MARKER_WIDTH, IMG_BUF_CMAP_HEIGHT, CMAP_TEXT_BG_COLOR);
	
	// Draw the new marker
	draw_hline(img, x_offset, x_offset + PALETTE_MARKER_WIDTH - 1, y_offset, MARKER_COLOR);
}


void vid_render_parm_string(const char* s, uint8_t* img)
{
	uint16_t w, h;
	uint16_t x, y;
	
	// Do nothing for zero-length strings
	if (s[0] == 0) return;
	
	set_clip_region(CLIP_REGION_IMAGE);
	
	// Compute the width and height of the string
	w = font_get_string_width(s, &Font7x10);
	h = Font7x10.font_Height;
	
	// Compute the starting location
	x = IMG_BUF_CMAP_WIDTH + ((T1C_WIDTH - w) / 2);
	y = IMG_BUF_HEIGHT/3;
	
	// Blank an area and draw the text
	draw_fill_rect(img, x-1, y-1, w+2, h+2, IMG_TEXT_BG_COLOR);
	draw_string(img, x, y, s, &Font7x10);
}


void vid_render_freeze_marker(uint8_t* img)
{
	int16_t x, y;
	
	set_clip_region(CLIP_REGION_IMAGE);
	
	// Compute the left-top corner
	x = IMG_BUF_WIDTH - IMG_FREEZE_SIZE - 10;
	y = 10;
	
	// Draw a black bounding box
	draw_rect(img, x - 1, y - 1, IMG_FREEZE_SIZE + 2, IMG_FREEZE_SIZE + 2, 0x00);
	
	// Draw the inner white box
	draw_fill_rect(img, x, y, IMG_FREEZE_SIZE, IMG_FREEZE_SIZE, MARKER_COLOR);
}


void vid_render_battery_info(uint8_t* img, int batt_percent, bool batt_critical)
{
	int16_t x, y;
	int16_t w;
	
	set_clip_region(CLIP_REGION_CMAP);
	
	// Erase the battery status region
	draw_fill_rect(img, 0, 0, IMG_BUF_CMAP_WIDTH, IMG_BUF_BATT_RGN_H, CMAP_TEXT_BG_COLOR);
	
	// Update with current battery status
	if (batt_critical) {
		// Critical battery warning
		x = (IMG_BUF_CMAP_WIDTH - font_get_string_width("CRIT", &Font7x10)) / 2;
		y = (IMG_BUF_BATT_RGN_H - IMG_BUF_BATT_BOD_H) / 2;
		draw_string(img, x, y, "CRIT", &Font7x10);
	} else {
		// Draw the battery nipple
		x = IMG_BUF_CMAP_WIDTH - ((IMG_BUF_CMAP_WIDTH - IMG_BUF_BATT_BOD_W) / 2);
		y = ((IMG_BUF_BATT_RGN_H - IMG_BUF_BATT_BOD_H) / 2) + 3;
		draw_fill_rect(img, x, y, 2, IMG_BUF_BATT_BOD_H - 2*3, BATT_COLOR);
		
		// Draw the battery body
		x = (IMG_BUF_CMAP_WIDTH - IMG_BUF_BATT_BOD_W) / 2;
		y = (IMG_BUF_BATT_RGN_H - IMG_BUF_BATT_BOD_H) / 2;
		draw_rect(img, x, y, IMG_BUF_BATT_BOD_W, IMG_BUF_BATT_BOD_H, BATT_COLOR);
		
		// Draw the battery fill state
		w = batt_percent * IMG_BUF_BATT_BOD_W / 100;
		draw_fill_rect(img, x, y, w, IMG_BUF_BATT_BOD_H, BATT_COLOR);
	}
}


void vid_render_env_info(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g)
{
	char buf[23];                       // ">-xxx°F xxx% xx' xx"" + null + safety
	int n = 0;
	int16_t x, y;
	int16_t w, h;
	
	set_clip_region(CLIP_REGION_IMAGE);
	
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
		x = IMG_BUF_WIDTH - w;
		y = IMG_BUF_HEIGHT - IMG_ENV_TEXT_HEIGHT;
		
		// Blank an area for the text
		draw_fill_rect(img, x-1, y, w+1, IMG_ENV_TEXT_HEIGHT, IMG_TEXT_BG_COLOR);
		
		// Offset y in text area and draw string
		y += (IMG_ENV_TEXT_HEIGHT - h) / 2;
		draw_string(img, x, y, buf, &Font7x10);
	}
}


void vid_render_timelapse_status(uint8_t* img)
{
	const char* buf = "TL";
	int16_t x, y;
	int16_t w, h;
	
	set_clip_region(CLIP_REGION_CMAP);
	
	// Erase the battery status region
	draw_fill_rect(img, 0, 0, IMG_BUF_CMAP_WIDTH, IMG_BUF_BATT_RGN_H, CMAP_TEXT_BG_COLOR);
	
	// Draw the text where the battery icon usually goes
	w = font_get_string_width(buf, &Font7x10) + 1;
	h = Font7x10.font_Height;
	x = (IMG_BUF_CMAP_WIDTH - w) / 2;
	y = (IMG_BUF_BATT_RGN_H - h) / 2;
	draw_string(img, x, y, buf, &Font7x10);
}


//
// Internal functions
//
static void set_clip_region(int region)
{
	switch (region) {
		case CLIP_REGION_ALL:
			clip_x1 = 0;
			clip_y1 = 0;
			clip_x2 = IMG_BUF_WIDTH - 1;
			clip_y2 = IMG_BUF_HEIGHT - 1;
			break;
			
		case CLIP_REGION_CMAP:
			clip_x1 = 0;
			clip_y1 = 0;
			clip_x2 = IMG_BUF_CMAP_WIDTH - 1;
			clip_y2 = IMG_BUF_HEIGHT - 1;
			break;
			
		case CLIP_REGION_TMRK:
			clip_x1 = PALETTE_BAR_X_OFFSET + PALETTE_BAR_WIDTH;
			clip_y1 = IMG_BUF_BATT_RGN_H + IMG_BUF_CMAP_TEXT_H;
			clip_x2 = IMG_BUF_CMAP_WIDTH - 1;
			clip_y2 = IMG_BUF_HEIGHT - IMG_BUF_CMAP_TEXT_H - 1;
			break;
		
		case CLIP_REGION_IMAGE:
			clip_x1 = IMG_BUF_CMAP_WIDTH;
			clip_y1 = 0;
			clip_x2 = IMG_BUF_WIDTH - 1;
			clip_y2 = IMG_BUF_HEIGHT - 1;
			break;
			
		default:
			clip_x1 = 0;
			clip_y1 = 0;
			clip_x2 = 0;
			clip_y2 = 0;
	}
}


static void draw_min_marker(t1c_buffer_t* t1c, int16_t n, uint8_t* img)
{
	int16_t x1, xm, x2, y1, y2;
	
	// Compute a bounding box around the marker triangle
	x1 = t1c->max_min_temp_info.min_temp_point.x + IMG_BUF_CMAP_WIDTH - (n/2);
	xm = t1c->max_min_temp_info.min_temp_point.x + IMG_BUF_CMAP_WIDTH;
	x2 = x1 + n;
	y1 = t1c->max_min_temp_info.min_temp_point.y - (n/2);
	y2 = y1 + n;
	
	// Draw a white downward facing triangle surrounded by a black triangle for contrast
	draw_hline(img, x1, x2, y1, MARKER_COLOR);
	draw_line(img, x1, y1, xm, y2, MARKER_COLOR);
	draw_line(img, xm, y2, x2, y1, MARKER_COLOR);
	
	x1--;
	y1--;
	x2++;
	y2++;
	
	draw_hline(img, x1, x2, y1, 0x00);
	draw_line(img, x1, y1, xm, y2, 0x00);
	draw_line(img, xm, y2, x2, y1, 0x00);
}


static void draw_max_marker(t1c_buffer_t* t1c, int16_t n, uint8_t* img)
{
	int16_t x1, xm, x2, y1, y2;
	
	// Compute a bounding box around the marker triangle
	x1 = t1c->max_min_temp_info.max_temp_point.x + IMG_BUF_CMAP_WIDTH - (n/2);
	xm = t1c->max_min_temp_info.max_temp_point.x + IMG_BUF_CMAP_WIDTH;
	x2 = x1 + n;
	y1 = t1c->max_min_temp_info.max_temp_point.y - (n/2);
	y2 = y1 + n;
	
	// Draw a white upward facing triangle surrounded by a black triangle for contrast
	draw_hline(img, x1, x2, y2, MARKER_COLOR);
	draw_line(img, x1, y2, xm, y1, MARKER_COLOR);
	draw_line(img, xm, y1, x2, y2, MARKER_COLOR);
	
	x1--;
	y1--;
	x2++;
	y2++;
	
	draw_hline(img, x1, x2, y2, 0x00);
	draw_line(img, x1, y2, xm, y1, 0x00);
	draw_line(img, xm, y1, x2, y2, 0x00);
}


static void draw_temp(uint8_t* img, int16_t x, int16_t y, uint16_t v, out_state_t* g)
{
	char buf[8];
	uint16_t w, h;
	
	// Get the temperature string
	sprintf(buf, "%d", (int16_t) round(temp_to_float_temp(v, g->temp_unit_C)));
	
	// Get attributes for text string
	w = font_get_string_width(buf, &Font7x10);
	h = Font7x10.font_Height;
	
	// Offset y in text area
	y += (IMG_BUF_CMAP_TEXT_H - h) / 2;
	
	// Blank the text area
	draw_fill_rect(img, x, y, IMG_BUF_CMAP_WIDTH, h, CMAP_TEXT_BG_COLOR);
	
	// Draw the text
	draw_string(img, x + (IMG_BUF_CMAP_WIDTH-w)/2, y, buf, &Font7x10);
}


static void draw_hline(uint8_t* img, int16_t x1, int16_t x2, int16_t y, uint8_t c)
{
	uint8_t* imgP;
	
	if (x1 < clip_x1)
		x1 = clip_x1;
	if (x2 > clip_x2)
		x2 = clip_x2;
	if ((y < clip_y1) || (y > clip_y2)) return;
	
	imgP = img + y*IMG_BUF_WIDTH + x1;
	memset(imgP, c, x2 - x1 + 1);
}


static void draw_vline(uint8_t* img, int16_t x, int16_t y1, int16_t y2, uint8_t c)
{
	uint8_t* imgP;
	
	if ((x < clip_x1) || (x > clip_x2)) return;
	if (y1 < clip_y1)
		y1 = clip_y1;
	if (y2 > clip_y2)
		y2 = clip_y2;
	
	imgP = img + y1*IMG_BUF_WIDTH + x;
	
	while (y1++ <= y2) {
		*imgP = c;
		imgP += IMG_BUF_WIDTH;
	}
}


static void draw_line(uint8_t* img, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t c)
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


static void draw_circle(uint8_t* img, int16_t x0, int16_t y0, int16_t r, uint8_t c) {
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


static void draw_rect(uint8_t* img, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t c)
{
	uint8_t* imgP;
	int16_t y1;
	
	if (x < clip_x1) {
		w -= (clip_x1 - x);
		if (w <= 0) return;
		x = clip_x1;
	}
	if ((x+w-1) > clip_x2)
		w -= (x+w) - clip_x2;
	if (y < clip_y1) {
		h -= (clip_y1 - y);
		if (h <= 0) return;
		y = clip_y1;
	}
	if ((y+h-1) > clip_y2)
		h -= (y+h) - clip_y2;
	
	// Top and bottom lines
	imgP = img + y*IMG_BUF_WIDTH + x;
	memset(imgP, c, w);
	memset(imgP + IMG_BUF_WIDTH * (h-1), c, w);
	
	// Left and right lines
	y1 = y + h;
	while (y++ < y1) {
		*imgP = c;
		*(imgP + w - 1) = c;
		imgP += IMG_BUF_WIDTH;
	}
}


static void draw_fill_rect(uint8_t* img, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t c)
{
	if (x < clip_x1) {
		w -= (clip_x1 - x);
		if (w <= 0) return;
		x = clip_x1;
	}
	if ((x+w-1) > clip_x2)
		w -= (x+w) - clip_x2;
	if (y < clip_y1) {
		h -= (clip_y1 - y);
		if (h <= 0) return;
		y = clip_y1;
	}
	if ((y+h-1) > clip_y2)
		h -= (y+h) - clip_y2;
		
	int16_t y1 = y;
	uint8_t* imgP = img + y*IMG_BUF_WIDTH + x;
	
	while (y1++ < (y+h)) {
		memset(imgP, c, w);
		imgP += IMG_BUF_WIDTH;
	}
}


static int16_t draw_char(uint8_t* img, int16_t x, int16_t y, uint8_t c, const Font_TypeDef *Font)
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
					if (tmpCh & 0x01) draw_pixel(img, pX, pY, TEXT_COLOR);
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
							if (tmpCh & 0x01) draw_pixel(img, pX, pY, TEXT_COLOR);
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
					if (tmpCh & 0x01) draw_pixel(img, pX, pY, TEXT_COLOR);
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
							if (tmpCh & 0x01) draw_pixel(img, pX, pY, TEXT_COLOR);
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


static void draw_string(uint8_t* img, int16_t x, int16_t y, const char *str, const Font_TypeDef *Font)
{
	uint16_t pX = x;
	uint16_t eX = IMG_BUF_WIDTH - Font->font_Width - 1;

	while (*str) {
		pX += draw_char(img, pX, y, *str++, Font);
		if (pX > eX) break;
	}
}


static __inline__ void draw_pixel(uint8_t* img, int16_t x, int16_t y, uint8_t c)
{
	if ((x < clip_x1) || (x > clip_x2)) return;
	if ((y < clip_y1) || (y > clip_y2)) return;
	*(img + x + y*IMG_BUF_WIDTH) = c;
}
