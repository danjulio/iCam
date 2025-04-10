/*
 * GUI utilities functions including various pop-up windows
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
#ifndef GUI_UTILITIES_H
#define GUI_UTILITIES_H

#include "cmd_list.h"
#include "gui_state.h"
#include "lvgl.h"
#include <stdint.h>


//
// Constants
//

// Card update poll interval
#define GUI_CARD_PRESENT_POLL_MSEC  500

// Message box related
//
// Message box types
#define GUI_MSG_BOX_1_BTN       false
#define GUI_MSG_BOX_2_BTN       true

// Message box button ids
#define GUI_MSG_BOX_BTN_NONE    LV_BTNM_BTN_NONE
#define GUI_MSG_BOX_BTN_DISMSS  0
#define GUI_MSG_BOX_BTN_AFFIRM  1

// Message box dimensions
#define GUI_MSG_BOX_W           300
#define GUI_MSG_BOX_H           240

// Maximum preset message box string length
#define GUI_MSG_BOX_MAX_LEN     128

// Keypad pop-up related
//
// Keypad pop-up types
#define GUI_KEYPAD_TYPE_ALPHA       0
#define GUI_KEYPAD_TYPE_NUMERIC     1

// Keypad events delivered via the handler callback
#define GUI_KEYPAD_EVENT_CLOSE_CANCEL 0
#define GUI_KEYPAD_EVENT_CLOSE_ACCEPT 1

// Activity pop-up related
//
// Dimensions
#define GUI_ACTIVITY_PU_W        300
#define GUI_ACTIVITY_PU_H        220

// Spinner Dimensions
#define GUI_ACT_PU_SPIN_W        100
#define GUI_ACT_PU_SPIN_H        100
#define GUI_ACT_PU_SPIN_OFF_Y    25

// Time to display result
#define GUI_ACT_PU_DISP_MSEC     1500

// File related (copied from file_utilities.h)
// Directory and file string lengths (including null)
//  DIR_NAME has room for "NNNICAMF" + extra to make the compiler happy
//  FILE_NAME has room for "ICAM_NNNN.JPG" + extra to make the compiler happy
#define GUI_DIR_NAME_LEN       16
#define GUI_FILE_NAME_LEN      21



//
// Typedefs for callbacks
//
// Handler for Message Box pressed button type
typedef void (*messagebox_handler_t)(int btn_id);

// Handler for Keypad pressed key
typedef void (*keypad_handler_t)(int kp_event);



//
// API
//

// Scale a coordinate value from the displayed value to the absolute value
uint16_t gui_act_coord_to_real_coord(int mag_level, uint16_t act_coord_val);

// Convert a Tiny1C native temperature value to a displayable value in specified units (C or F)
float gui_t1c_to_disp_temp(uint16_t v, gui_state_t* g);

// Convert an float °C temperature value to a displayable value in specified units (C or F)
float gui_float_c_to_disp_temp(float t, gui_state_t* g);

// Convert a distance in cm to a displayable value in specified units (M or Feet)
float gui_dist_to_disp_dist(uint16_t v, gui_state_t* g);

// Fill a string with a IPV4 network address from a 4-byte array
void gui_print_ipv4_addr(char* s, uint8_t* addr);

// Parse (and validate) a formatted IPV4 network address string ("XXX.XXX.XXX.XXX")
bool gui_parse_ipv4_addr_string(char* s, uint8_t* addr);

// Validate a numeric text entry ([-]NN or [-]NN.N...)
bool gui_validate_numeric_text(char* s);

// Display memory statistics for the LVGL memory heap
void gui_dump_mem_info();

// Enable or disable periodic updating of card present status in gui_state
void gui_enable_card_present_updating(bool en);

// Return true if any of the following pop-ups are displayed.  Can be used to prevent
// other operations or navigation away from the current page
bool gui_popup_displayed();

// Display a message box
void gui_display_message_box(lv_obj_t* parent, const char* msg, bool dual_button, messagebox_handler_t cb_mbox);
bool gui_message_box_displayed();

// Display a draggable keypad popup
void gui_display_keypad(lv_obj_t* parent, int type, const char* name, char* val, int val_len, keypad_handler_t cb_keypad);
bool gui_keypad_displayed();

// Call to send a controller activity command and display the controller activity popup
// if one is not currently in progress
void gui_send_activity_command(enum cmd_ctrl_act_param cmd, int32_t aux_data, lv_obj_t* parent, const char* desc);

// Display a controller activity in process popup
void gui_display_activity_popup(lv_obj_t* parent, const char* desc);
void gui_update_activity_popup(bool success);
bool gui_activity_popup_displayed();

#endif /* GUI_UTILITIES_H */