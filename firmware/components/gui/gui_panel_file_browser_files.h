/*
 * GUI file browser file selection control panel
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
#ifndef GUI_PANEL_FILE_BROWSER_FILES_H
#define GUI_PANEL_FILE_BROWSER_FILES_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>



//
// Constants
//

// Actions for the companion image panel
#define GUIPN_FILE_BROWSER_FILES_ACT_PREV 0
#define GUIPN_FILE_BROWSER_FILES_ACT_NEXT 1
#define GUIPN_FILE_BROWSER_FILES_ACT_DEL  2

// SD Card present poll rate
#define GUIPN_FILE_BROWSER_FILES_POLL_MSEC 500


// LVGL Objects

// Page maximum dimensions (min set for gCore taking image panel into account)
#define GUIPN_FILE_BROWSER_FILES_MIN_W 224
#define GUIPN_FILE_BROWSER_FILES_MAX_W 320
#define GUIPN_FILE_BROWSER_FILES_MIN_H 216
#define GUIPN_FILE_BROWSER_FILES_MAX_H 480

#define GUIPN_FILE_BROWSER_FILES_LBL_Y 0

#define GUIPN_FILE_BROWSER_FILES_TBL_W 120
#define GUIPN_FILE_BROWSER_FILES_TBL_Y 30
#define GUIPN_FILE_BROWSER_FILES_PAD_B 10
#define GUIPN_FILE_BROWSER_FILES_PAD_I 26



//
// API
//

// Call before init() or reset_size() so this panel knows its dimensions
void gui_panel_file_browser_files_calculate_size(uint16_t minor_dim_max, uint16_t major_dim, uint16_t* minor_dim, bool is_portrait);

// From our parent
lv_obj_t* gui_panel_file_browser_files_init(lv_obj_t* page);
void gui_panel_file_browser_files_reset_size();
void gui_panel_file_browser_files_set_active(bool is_active);

// From command handlers
void gui_panel_file_browser_files_set_catalog(int type, int num_entries, char* entries);

// From our companion image panel
void gui_panel_file_browser_files_action(int action);

#endif /* GUI_PANEL_FILE_BROWSER_FILES_H */
