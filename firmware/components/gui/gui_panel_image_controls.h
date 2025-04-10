/*
 * GUI Live image control panel
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
#ifndef GUI_PANEL_IMAGE_CONTROL_H
#define GUI_PANEL_IMAGE_CONTROL_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>



//
// Constants
//

// LVGL Objects
#define GUIPN_IMAGEC_BTN_W       40
#define GUIPN_IMAGEC_BTN_H       40
#define GUIPN_IMAGEC_BTN_SPACING 10

// Marker status message display time
#define GUIPN_IMAGEC_MARKER_MSG_MSEC 700


// Delays before updating NVS after marker changes
#define GUIPN_IMAGEC_MINMAX_UPD_MSEC 2000
#define GUIPN_IMAGEC_SPOT_UPD_MSEC   1000



//
// API
//

// Call before init() or reset_size() so this panel knows its dimensions
void gui_panel_image_controls_calculate_size(uint16_t major_dim, uint16_t* minor_dim, bool is_portrait);

lv_obj_t* gui_panel_image_controls_init(lv_obj_t* page);
void gui_panel_image_controls_reset_size();
void gui_panel_image_controls_set_active(bool is_active);

// From command handlers
void gui_panel_image_controls_set_timelapse(bool en);

#endif /* GUI_PANEL_IMAGE_CONTROL_H */