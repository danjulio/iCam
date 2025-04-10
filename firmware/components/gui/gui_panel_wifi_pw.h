/*
 * GUI wifi settings set password control panel
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
#ifndef GUI_PANEL_WIFI_PW_H
#define GUI_PANEL_WIFI_PW_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>



//
// Constants
//
#define GUIPN_WIFI_PW_BTN_W      180
#define GUIPN_WIFI_PW_BTN_H      25
#define GUIPN_WIFI_PW_BTN_SPACE  10
#define GUIPN_WIFI_PW_HIDE_BTN_W 30
#define GUIPN_WIFI_PW_HIDE_BTN_H 25



//
// API
//
void gui_panel_wifi_pw_init(lv_obj_t* parent_cont);
void gui_panel_wifi_pw_set_active(bool is_active);
void gui_panel_wifi_pw_note_updated_mode();

#endif /* GUI_PANEL_WIFI_PW_H */
