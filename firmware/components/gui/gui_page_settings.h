/*
 * GUI settings display page
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
#ifndef GUI_PAGE_SETTINGS_H
#define GUI_PAGE_SETTINGS_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>


//
// Constants
//

// Page min/max width
#define GUIP_SETTINGS_MIN_WIDTH  320
#define GUIP_SETTINGS_MAX_WIDTH  480

// Back button
#define GUIP_SETTINGS_BACK_X     5
#define GUIP_SETTINGS_BACK_Y     10
#define GUIP_SETTINGS_BACK_W     40
#define GUIP_SETTINGS_BACK_H     30

// Page title
#define GUIP_SETTINGS_TITLE_X    (GUIP_SETTINGS_BACK_X + GUIP_SETTINGS_BACK_W)
#define GUIP_SETTINGS_TITLE_Y    12

// Control panel page
#define GUIP_SETTINGS_CONTROL_Y  40

// Settings for individual control panels
#define GUIP_SETTINGS_TOP_PAD    10
#define GUIP_SETTINGS_BTM_PAD    10
#define GUIP_SETTINGS_LEFT_PAD   10
#define GUIP_SETTINGS_RIGHT_PAD  10
#define GUIP_SETTINGS_INNER_PAD  10

// Sub-page animation time
#define GUIP_SETTINGS_SUB_PG_MSEC 500


//
// Function definitions for sub-page registration
//

// Sub-page activation routines
typedef void (*sub_page_activation_handler)(bool en);

// Sub-page resize routines
typedef void (*sub_page_resize_handler)(uint16_t page_w, uint16_t page_h);



//
// API
//
//
// From GUI controller
lv_obj_t* gui_page_settings_init(lv_obj_t* screen, uint16_t page_w, uint16_t page_h, bool mobile);
void gui_page_settings_set_active(bool is_active);
void gui_page_settings_reset_screen_size(uint16_t page_w, uint16_t page_h);

// From settings panels
void gui_page_settings_register_panel(lv_obj_t* panel, lv_obj_t* sub_page, sub_page_activation_handler activate_func, sub_page_resize_handler resize_func);
void gui_page_settings_open_sub_page(lv_obj_t* sub_page);

// From sub-pages
void gui_page_settings_close_sub_page(lv_obj_t* sub_page);


#endif /* GUI_PAGE_SETTINGS_H */
