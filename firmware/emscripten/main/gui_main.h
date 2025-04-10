/*
 * Top-level GUI manager for web interface
 *
 * Copyright 2024 Dan Julio
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef GUI_MAIN
#define GUI_MAIN

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

//
// Global constants
//
// Top-level pages
#define GUI_MAIN_PAGE_DISCONNECTED 0
#define GUI_MAIN_PAGE_IMAGE        1
#define GUI_MAIN_PAGE_SETTINGS     2
#define GUI_MAIN_PAGE_LIBRARY      3

#define GUI_NUM_MAIN_PAGES         4


// Background color (should match theme background - A kludge, I know.  Specified here because
// themes don't allow direct access to it and IMHO the LVGL theme system is incredibly hard to use)
#define GUI_THEME_BG_COLOR         lv_color_hex(0x444b5a)
#define GUI_THEME_SLD_BG_COLOR     lv_color_hex(0x3d4351)
#define GUI_THEME_RLR_BG_COLOR     lv_color_hex(0x3d4351)
#define GUI_THEME_TBL_BG_COLOR     lv_color_hex(0x3d4351)


// Disconnected page layout (we manage that page in this module)
#define GUIP_DISC_LBL_OFFSET_X     40
#define GUIP_DISC_LBL_OFFSET_Y     30
#define GUIP_DISC_LBL_W            120

#define GUIP_DISC_BTN_OFFSET_X     40
#define GUIP_DISC_BTN_OFFSET_Y     60
#define GUIP_DISC_BTN_W            120
#define GUIP_DISC_BTN_H            30


//
// Global groups for LVGL objects wishing to get keypad or encoder input
//
extern lv_group_t* gui_keypad_group;
//extern lv_group_t* gui_encoder_group;


//
// Function signature for socket (re)connect routine
//
typedef void (*socket_activity_handler)();



//
// API
//

// For use by main
void gui_main_init(lv_obj_t* screen, uint16_t browser_w, uint16_t browser_h, bool is_mobile);
void gui_main_set_connected(bool is_connected);
void gui_main_reset_browser_dimensions(uint16_t browser_w, uint16_t browser_h);
void gui_main_register_socket_connect(socket_activity_handler connect_handler);
void gui_main_register_socket_disconnect(socket_activity_handler disconnect_handler);

// For use by GUI pages or cmd decoder
void gui_main_set_page(uint32_t page);
void gui_main_shutdown();

#endif /* GUI_MAIN */