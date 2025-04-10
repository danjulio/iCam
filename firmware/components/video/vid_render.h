/*
 * Renderers for Tiny1c images, text, spot meter and min/max markers
 *
 * Copyright 2020 - 2024 Dan Julio
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
#ifndef VID_RENDER_H
#define VID_RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include "out_state_utilities.h"
#include "tiny1c.h"


//
// Vid Render Constants
//

// Image buffer dimensions
//
// Left column with battery, palette, min/max temps and spot temp marker
#define IMG_BUF_CMAP_WIDTH   32

// Battery area
#define IMG_BUF_BATT_BOD_W   (IMG_BUF_CMAP_WIDTH - 12)
#define IMG_BUF_BATT_RGN_H   16
#define IMG_BUF_BATT_BOD_H   (IMG_BUF_BATT_RGN_H - 6)

// Palette area
#define IMG_BUF_CMAP_TEXT_H  16
#define IMG_BUF_CMAP_HEIGHT  (T1C_HEIGHT - (2*IMG_BUF_CMAP_TEXT_H + IMG_BUF_BATT_RGN_H))
#define PALETTE_BAR_X_OFFSET (IMG_BUF_CMAP_WIDTH/4)
#define PALETTE_BAR_WIDTH    (IMG_BUF_CMAP_WIDTH/4)
#define PALETTE_MARKER_WIDTH ((IMG_BUF_CMAP_WIDTH/6) + 2)

// Region text area
#define IMG_BUF_REG_TEXT_H   16

// Overall image dimensions (palette + image)
#define IMG_BUF_WIDTH        (T1C_WIDTH+IMG_BUF_CMAP_WIDTH)
#define IMG_BUF_HEIGHT       T1C_HEIGHT

// Environmental conditions (lower right side of image)
#define IMG_ENV_TEXT_HEIGHT  16

// Spot meter
#define IMG_SPOT_SIZE       6

// Min/Max markers
#define IMG_MARKER_SIZE_S   6
#define IMG_MARKER_SIZE_L   8

// Video freeze marker dimensions
#define IMG_FREEZE_SIZE     10

// Battery icon intensity
#define BATT_COLOR          250

// Text intensity
#define TEXT_COLOR          250

// Marker intensity
#define MARKER_COLOR        250

// Text background intensity
#define CMAP_TEXT_BG_COLOR  40
#define IMG_TEXT_BG_COLOR   120



//
// Vid Render API
//
void vid_render_test_pattern(uint8_t* img);
void vid_render_t1c_data(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g);
void vid_render_palette(uint8_t* img, out_state_t* g);
void vid_render_spotmeter(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g);
void vid_render_min_max_markers(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g);
void vid_render_region_marker(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g);
void vid_render_region_temps(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g);
void vid_render_min_max_temps(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g);
void vid_render_palette_marker(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g);
void vid_render_parm_string(const char* s, uint8_t* img);
void vid_render_freeze_marker(uint8_t* img);
void vid_render_battery_info(uint8_t* img, int batt_percent, bool batt_critical);
void vid_render_env_info(t1c_buffer_t* t1c, uint8_t* img, out_state_t* g);
void vid_render_timelapse_status(uint8_t* img);

#endif /* VID_RENDER_H */
