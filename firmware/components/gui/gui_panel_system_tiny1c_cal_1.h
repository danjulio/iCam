/*
 * GUI system settings Tiny1C 1-point calibration control panel
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
#ifndef GUI_PANEL_SYSTEM_TINY1C_CAL_1_H
#define GUI_PANEL_SYSTEM_TINY1C_CAL_1_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>



//
// Constants
//
#define GUIPN_SYSTEM_TINY1C_CAL_1_UNIT_W 30
#define GUIPN_SYSTEM_TINY1C_CAL_1_BTN_W  80
#define GUIPN_SYSTEM_TINY1C_CAL_1_BTN_H  25

// Default blackbody temperature (°C)
#define GUIPN_SYSTEM_TINY1C_CAL_1_TEMP   35


//
// API
//
void gui_panel_system_tiny1c_cal_1_init(lv_obj_t* parent_cont);
void gui_panel_system_tiny1c_cal_1_set_active(bool is_active);

#endif /* GUI_PANEL_SYSTEM_TINY1C_CAL_1_H */
