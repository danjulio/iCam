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
#ifndef GUI_RENDER_H
#define GUI_RENDER_H

#include "gui_state.h"
#include <stdbool.h>
#include <stdint.h>


//
// Global constants
//

// Raw image size (matches Tiny1C)
#define GUI_RAW_IMG_W        256
#define GUI_RAW_IMG_H        192

// Orientation
#define GUI_RENDER_PORTRAIT  0
#define GUI_RENDER_LANDSCAPE 1

// Magnification
#define GUI_MAGNIFICATION_0_5  0
#define GUI_MAGNIFICATION_1_0  1
#define GUI_MAGNIFICATION_1_5  2
#define GUI_MAGNIFICATION_2_0  3

#define GUI_LARGEST_MAG_FACTOR 2

// Spot meter (at 1X)
#define GUI_SPOT_SIZE       6

// Min/Max markers (at 1X)
#define GUI_MARKER_SIZE     6

// Freeze marker (at 1X)
#define GUI_FREEZE_MARKER_SIZE 10

// Rendering canvas size
#ifdef ESP_PLATFORM
	// ESP32: Using 16-bit pixels: RGB565
	#define GUI_REND_IMG_T uint16_t
#else
	// Web: Using 32-bit pixels: ARGB8888
	#define GUI_REND_IMG_T uint32_t
#endif


//
// Typedefs
//
typedef struct {
	bool high_gain;
	bool vid_frozen;
	bool spot_valid;
	bool minmax_valid;
	bool region_valid;
	bool amb_temp_valid;
	bool amb_hum_valid;
	bool distance_valid;
	uint8_t* y8_data;       // 8-bits pre-scaled from imager Y16 data and organized in portrait or landscape mode
	int16_t amb_temp;
	uint16_t amb_hum;
	uint16_t distance;
	uint16_t spot_temp;
	uint16_t spot_x;
	uint16_t spot_y;
	uint16_t min_temp;
	uint16_t min_x;
	uint16_t min_y;
	uint16_t max_temp;
	uint16_t max_x;
	uint16_t max_y;
	uint16_t region_x1;
	uint16_t region_y1;
	uint16_t region_x2;
	uint16_t region_y2;
	uint16_t region_avg_temp;
	uint16_t region_min_x;
	uint16_t region_min_y;
	uint16_t region_min_temp;
	uint16_t region_max_x;
	uint16_t region_max_y;
	uint16_t region_max_temp;
} gui_img_buf_t;



//
// Render API
//
bool gui_render_init();
void gui_render_set_configuration(int orientation, int magnification);  // Must be called before using other routines
uint8_t* gui_render_get_y8_data(uint16_t* y16_data, uint16_t y16_min, uint16_t y16_max);
void gui_render_image_data(gui_img_buf_t* raw, GUI_REND_IMG_T* img, gui_state_t* g);
void gui_render_spotmeter(gui_img_buf_t* raw, GUI_REND_IMG_T* img);
void gui_render_min_max_markers(gui_img_buf_t* raw, GUI_REND_IMG_T* img);
void gui_render_region_marker(gui_img_buf_t* raw, GUI_REND_IMG_T* img);
void gui_render_region_drag_marker(gui_img_buf_t* raw, GUI_REND_IMG_T* img);
void gui_render_freeze_marker(GUI_REND_IMG_T* img);

#endif /* GUI_RENDER_H */