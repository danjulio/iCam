/*
 * GUI Live image display panel
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
#ifndef GUI_PANEL_IMAGE_MAIN_H
#define GUI_PANEL_IMAGE_MAIN_H

#include "gui_render.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>



//
// Constants
//

// Battery status update period
#define GUIPN_IMAGE_BATT_UPD_MSEC    5000

// Timelapse indication update period
#define GUIPN_IMAGE_TIMELAPSE_MSEC   1000

// Palette change NVS update period
#define GUIPN_IMAGE_PALETTE_UPD_MSEC 5000

// Region selection timeout period
#define GUIPN_IMAGE_REGION_SEL_MSEC  5000

// Maximum message length
#define GUIP_MAX_MSG_LEN            80

// Dimensional

// Top status bar
#define GUIPN_IMAGE_STATUS_H        32
#define GUIPN_IMAGE_BATT_X_OFFSET   0
#define GUIPN_IMAGE_BATT_Y_OFFSET   8
#define GUIPN_IMAGE_SPOT_Y_OFFSET   7
#define GUIPN_IMAGE_SPOT_W          50
#define GUIPN_IMAGE_RGN_X_OFFSET    -10
#define GUIPN_IMAGE_RGN_Y_OFFSET    9
#define GUIPN_IMAGE_RGN_W           100
#define GUIPN_IMAGE_ENV_X_OFFSET    5
#define GUIPN_IMAGE_ENV_Y_OFFSET    9
#define GUIPN_IMAGE_ENV_W           130

// Left Palette bar
#define GUIPN_IMAGE_PAL_BAR_W       32
#define GUIPN_IMAGE_PAL_W           16
#define GUIPN_IMAGE_PAL_X_OFFSET    0
#define GUIPN_IMAGE_PAL_Y_OFFSET    (GUIPN_IMAGE_STATUS_H + 16)
#define GUIPN_IMAGE_PAL_H           256
#define GUIPN_IMAGE_MINMAX_X_OFFSET 0
#define GUIPN_IMAGE_MIN_T_Y_OFFSET  (GUIPN_IMAGE_PAL_Y_OFFSET + GUIPN_IMAGE_PAL_H + 0)
#define GUIPN_IMAGE_MAX_T_Y_OFFSET  (GUIPN_IMAGE_STATUS_H + 0)
#define GUIPN_IMAGE_MRK_X_OFFSET    (GUIPN_IMAGE_PAL_X_OFFSET + GUIPN_IMAGE_PAL_W + 2)
#define GUIPN_IMAGE_MRK_Y_OFFSET    GUIPN_IMAGE_PAL_Y_OFFSET
#define GUIPN_IMAGE_MRK_W           10

// Main image area
#define GUIPN_IMAGE_IMG_X_OFFSET    GUIPN_IMAGE_PAL_BAR_W
#define GUIPN_IMAGE_IMG_Y_OFFSET    GUIPN_IMAGE_STATUS_H

// Message bar (y offset is from top of image)
#define GUIPN_IMAGE_MSG_OFFSET_X    GUIPN_IMAGE_PAL_BAR_W
#define GUIPN_IMAGE_MSG_OFFSET_Y    0


//
// Externally accessible image structure
//
extern gui_img_buf_t gui_panel_image_buf;



//
// API
//

// Call before init() or reset_size() so this panel knows its dimensions
void gui_panel_image_calculate_size(uint16_t max_w, uint16_t max_h, uint16_t* imgp_w, uint16_t* imgp_h);

lv_obj_t* gui_panel_image_init(lv_obj_t* page);
void gui_panel_image_reset_size();
void gui_panel_image_set_active(bool is_active);

// From command handlers
void gui_panel_image_set_batt_percent(int percent);
void gui_panel_image_render_image();
void gui_panel_image_set_message(char* msg, int display_msec);
void gui_panel_image_set_timelapse(bool en);

// From other gui pages
void gui_panel_image_update_palette();
void gui_panel_image_enable_region_selection(bool en);
bool gui_panel_image_region_selection_in_progress();

#endif /* GUI_PANEL_IMAGE_MAIN_H */