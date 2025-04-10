/*
 * GUI settings gain mode control panel
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
#ifndef GUI_SETTINGS_GAIN_H
#define GUI_SETTINGS_GAIN_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>



//
// Constants
//
#define GUIPN_SETTINGS_GAIN_TYP_W 50
#define GUIPN_SETTINGS_GAIN_SW_W  60
#define GUIPN_SETTINGS_GAIN_SW_H  25



//
// API
//
void gui_panel_settings_gain_init(lv_obj_t* parent_cont);
void gui_panel_settings_gain_set_active(bool is_active);

#endif /* GUI_SETTINGS_GAIN_H */
