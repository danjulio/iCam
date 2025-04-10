/*
 * GUI file browser display page
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
#ifndef GUI_PAGE_FILE_BROWSER_H
#define GUI_PAGE_FILE_BROWSER_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>


//
// Constants
//
// Page min/max width
#define GUIP_FILE_BROWSER_MIN_WIDTH      320
#define GUIP_FILE_BROWSER_MAX_WIDTH      720

// Back button
#define GUIP_FILE_BROWSER_BACK_X         5
#define GUIP_FILE_BROWSER_BACK_Y         10
#define GUIP_FILE_BROWSER_BACK_W         40
#define GUIP_FILE_BROWSER_BACK_H         30

// Page title
#define GUIP_FILE_BROWSER_TITLE_X        (GUIP_FILE_BROWSER_BACK_X + GUIP_FILE_BROWSER_BACK_W)
#define GUIP_FILE_BROWSER_TITLE_Y        12

// Start of panels
#define GUIP_FILE_BROWSER_CONTROL_Y      50

// Maximum padding to the left, above and between panels
#define GUIP_FILE_BROWSER_PAD_X          32
#define GUIP_FILE_BROWSER_PAD_Y          16



//
// API
//
lv_obj_t* gui_page_file_browser_init(lv_obj_t* screen, uint16_t page_w, uint16_t page_h, bool mobile);
void gui_page_file_browser_set_active(bool is_active);
void gui_page_file_browser_reset_screen_size(uint16_t page_w, uint16_t page_h);


#endif /* GUI_PAGE_FILE_BROWSER_H */
