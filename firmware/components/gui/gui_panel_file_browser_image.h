/*
 * GUI file browse image display panel
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
#ifndef GUI_PANEL_FILE_BROWSER_IMAGE_H
#define GUI_PANEL_FILE_BROWSER_IMAGE_H

#include "gui_render.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>



//
// Constants
//

// Control indicies
#define GUIPN_FILE_BROWSER_IMAGE_NEXT      0
#define GUIPN_FILE_BROWSER_IMAGE_PREV      1
#define GUIPN_FILE_BROWSER_IMAGE_DEL       2

// Main image area
#define GUIPN_FILE_BROWSER_IMG_X_OFFSET    0
#define GUIPN_FILE_BROWSER_IMG_Y_OFFSET    0

// Message bar (y offset is from top of image)
#define GUIPN_FILE_BROWSER_MSG_OFFSET_X    GUIPN_FILE_BROWSER_IMG_X_OFFSET
#define GUIPN_FILE_BROWSER_MSG_OFFSET_Y    0

// Bottom control bar (y offset is image height, width is image width)
#define GUIPN_FILE_BROWSER_CTRL_OFFSET_X   0
#define GUIPN_FILE_BROWSER_CTRL_H          32

#define GUIPN_FILE_BROWSER_BTN_W           40
#define GUIPN_FILE_BROWSER_BTN_H           30



//
// API
//

// Call before init() or reset_size() so this panel knows its dimensions
void gui_panel_file_browser_image_calculate_size(uint16_t max_w, uint16_t max_h, uint16_t* imgp_w, uint16_t* imgp_h);

// From our parent
lv_obj_t* gui_panel_file_browser_image_init(lv_obj_t* page);
void gui_panel_file_browser_image_reset_size();
void gui_panel_file_browser_image_set_active(bool is_active);

// From command handlers

// From gui_panel_file_browser_files
void gui_panel_file_browser_image_set_valid(bool valid);
#ifdef ESP_PLATFORM
void gui_panel_file_browser_image_set_image();
#else
void gui_panel_file_browser_image_set_image(uint32_t len, uint8_t* src);
#endif

void gui_panel_file_browser_image_set_ctrl_en(int index, bool en);

#endif /* GUI_PANEL_FILE_BROWSER_IMAGE_H */