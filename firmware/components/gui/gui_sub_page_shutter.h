/*
 * GUI system shutter settings sub-page
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
#ifndef GUI_SUB_PAGE_SHUTTER_H
#define GUI_SUB_PAGE_SHUTTER_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>


//
// Constants
//



//
// LVGL setup
//
// Back button
#define GUISP_SHUTTER_BACK_X     5
#define GUISP_SHUTTER_BACK_Y     10
#define GUISP_SHUTTER_BACK_W     40
#define GUISP_SHUTTER_BACK_H     30

// Page title
#define GUISP_SHUTTER_TITLE_X    (GUISP_SHUTTER_BACK_X + GUISP_SHUTTER_BACK_W)
#define GUISP_SHUTTER_TITLE_Y    12

// Control panel page
#define GUISP_SHUTTER_CONTROL_Y  40


//
// API
//
// From our panel
lv_obj_t* gui_sub_page_shutter_init(lv_obj_t* screen);

// From settings page
void gui_sub_page_shutter_set_active(bool is_active);
void gui_sub_page_shutter_reset_screen_size(uint16_t page_w, uint16_t page_h);

// From settings panels
void gui_sub_page_shutter_register_panel(lv_obj_t* panel);
void gui_sub_page_shutter_note_change();

#endif /* GUI_SUB_PAGE_SHUTTER_H */