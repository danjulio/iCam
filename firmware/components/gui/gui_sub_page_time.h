/*
 * GUI system set time/date sub-page
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
#ifndef GUI_SUB_PAGE_TIME_H
#define GUI_SUB_PAGE_TIME_H

#include "lvgl.h"
#include <time.h>
#include <stdbool.h>
#include <stdint.h>


//
// Constants
//

// Maximum length of info string
#define GUISP_TIME_MAX_INFO   1024

//
// LVGL setup
//
// Back button
#define GUISP_TIME_BACK_X     5
#define GUISP_TIME_BACK_Y     10
#define GUISP_TIME_BACK_W     40
#define GUISP_TIME_BACK_H     30

// Page title
#define GUISP_TIME_TITLE_X    (GUISP_TIME_BACK_X + GUISP_TIME_BACK_W)
#define GUISP_TIME_TITLE_Y    12

// Scrollable page for controls
#define GUISP_TIME_CONTROL_Y   20

// Time/Date string
#define GUISP_TIME_SETSTR_X    0
#define GUISP_TIME_SETSTR_Y    0

// Keypad
#define GUISP_TIME_KEYPAD_W    200
#define GUISP_TIME_KEYPAD_H    260

// Autoset button 
#define GUISP_TIME_AUTOSET_W   100
#define GUISP_TIME_AUTOSET_H   25


//
// API
//
// From our panel
lv_obj_t* gui_sub_page_time_init(lv_obj_t* screen);

// From settings page
void gui_sub_page_time_set_active(bool is_active);
void gui_sub_page_time_reset_screen_size(uint16_t page_w, uint16_t page_h);

// From command handler
void gui_sub_page_time_set_time(struct tm* cur_time);


#endif /* GUI_SUB_PAGE_TIME_H */