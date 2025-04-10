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
#include "esp_system.h"
#ifndef CONFIG_BUILD_ICAM_MINI

#include <arpa/inet.h>
#include "cmd_utilities.h"
#include "gui_render.h"
#include "gui_state.h"
#include "gui_utilities.h"
#include "lvgl.h"
#include <math.h>
#include <string.h>

#ifdef ESP_PLATFORM
	#include "esp_log.h"
#else
	#include "gui_main.h"
	#include <stdio.h>
#endif


//
// Local Constants
//

// Keypad special key character values
#define KEYPAD_CHAR_BACKSPACE     0x7F



//
// Variables
//
static const char* TAG = "gui_utilities";

// LVGL objects
static lv_obj_t* msg_box_bg = NULL;
static lv_obj_t* msg_box = NULL;

static lv_obj_t* win_keypad = NULL;
static lv_obj_t* btn_kp_accept;
static lv_obj_t* btn_kp_close;
static lv_obj_t* ta_kp_value;
static lv_obj_t* btnm_kp;

static lv_obj_t* act_pu_parent;
static lv_obj_t* act_pu_bg = NULL;
static lv_obj_t* act_pu_win;
static lv_obj_t* act_pu_spinner;
static lv_obj_t* act_pu_desc;

// Timer tasks
static lv_task_t* task_card_update_timer = NULL;
static lv_task_t* task_act_pu_timer = NULL;


// Message box buttons
static const char* msg_box_buttons1[] = {"OK", ""};
static const char* msg_box_buttons2[] = {"Cancel", "Confirm", ""};

// Message box callback
static messagebox_handler_t msg_box_cb;


// Keypad pointer to caller's string buffer
static char* keypad_val_buf;
static int keypad_val_buf_len;

static int keypad_type;

// Keypad callback
static keypad_handler_t keypad_cb;



//
// Forward declarations for internal functions
//
static void _cb_mbox(lv_obj_t *obj, lv_event_t event);
static void _cb_keypad(lv_obj_t *obj, lv_event_t event);
static void _cb_activity_pu(lv_obj_t *obj, lv_event_t event);
static void _cb_task_card_update_timer(lv_task_t* task);
static void _cb_task_act_pu_timer(lv_task_t* task);



//
// API
//
uint16_t gui_act_coord_to_real_coord(int mag_level, uint16_t act_coord_val)
{
	float mag;
	
	switch (mag_level) {
		case GUI_MAGNIFICATION_0_5:
			mag = 0.5;
			break;
		case GUI_MAGNIFICATION_1_0:
			mag = 1.0;
			break;
		case GUI_MAGNIFICATION_1_5:
			mag = 1.5;
			break;
		case GUI_MAGNIFICATION_2_0:
			mag = 2.0;
			break;
		default:
			mag = 1.0;
	}
	
	return (uint16_t) round(act_coord_val / mag);
}


float gui_t1c_to_disp_temp(uint16_t v, gui_state_t* g)
{
	float t;
	
	t = ((float) v) / 16.0 - 273.15;

	// Convert to F if required
	if (!g->temp_unit_C) {
		t = t * 9.0 / 5.0 + 32.0;
	}
	
	return t;
}


float gui_float_c_to_disp_temp(float t, gui_state_t* g)
{
	if (!g->temp_unit_C) {
		t = t * 9.0 / 5.0 + 32.0;
	}
	
	return t;
}


float gui_dist_to_disp_dist(uint16_t v, gui_state_t* g)
{
	if (g->temp_unit_C) {
		return (float) v / 100.0;
	} else {
		return (float) v / (2.54 * 12);
	}
}


void gui_print_ipv4_addr(char* s, uint8_t* addr)
{
	sprintf(s, "%d.%d.%d.%d", addr[3], addr[2], addr[1], addr[0]);
}


bool gui_parse_ipv4_addr_string(char* s, uint8_t* addr)
{
	char c;
	int i;
	int n = 3;
	int len;
	int temp_array[4] = { 0 };
	
	len = strlen(s);
	
	// Scan through characters and attempt to build an array of 4 numbers
	for (i=0; i<len; i++) {
		c = s[i];
		if ((c >= '0') && (c <= '9')) {
			temp_array[n] = temp_array[n]*10 + (c - '0');
			if (temp_array[n] > 255) {
				// Illegal value in field
				return false;
			}
		} else if (c == '.') {
			// Next field
			if (--n < 0) {
				// Too many fields
				return false;
			}
		} else {
			// Unexpected character
			return false;
		}
	}
	
	// Success, copy our array to the callers
	for (int i=0; i<4; i++) addr[i] = (uint8_t) temp_array[i];
	
	return true;
}


bool gui_validate_numeric_text(char* s)
{
	char c;
	int i;
	int len;
	int num_decimal_points = 0;
	
	len = strlen(s);
	
	for (i=0; i<len; i++) {
		c = s[i];
		if (c == '-') {
			if (i != 0) return false;
		} else if (c == '.') {
			if (num_decimal_points > 0) {
				return false;
			} else {
				num_decimal_points = 1;
			}
		} else if ((c < '0') || (c > '9')) {
			return false;
		}
	}
	
	return true;
}


void gui_dump_mem_info()
{
	lv_mem_monitor_t mem_info;
	
	// Get LVGL's current private heap info
	lv_mem_monitor(&mem_info);
	
#ifdef ESP_PLATFORM
	ESP_LOGI(TAG, "LVGL Memory Statistics:");
	ESP_LOGI(TAG, "  Total size: %lu", mem_info.total_size);
	ESP_LOGI(TAG, "  Free Count: %lu   Free Size: %lu   Free Biggest Size: %lu", mem_info.free_cnt, mem_info.free_size, mem_info.free_biggest_size);
	ESP_LOGI(TAG, "  Used Count: %lu   Max Used: %lu  Used Percent: %u", mem_info.used_cnt, mem_info.max_used, mem_info.used_pct);
	ESP_LOGI(TAG, "  Frag Percent: %u", mem_info.frag_pct);
#else
	printf("%s LVGL Memory Statistics:\n", TAG);
	printf("%s   Total size: %u\n", TAG, mem_info.total_size);
	printf("%s   Free Count: %u   Free Size: %u   Free Biggest Size: %u\n", TAG, mem_info.free_cnt, mem_info.free_size, mem_info.free_biggest_size);
	printf("%s   Used Count: %u   Max Used: %u  Used Percent: %u\n", TAG, mem_info.used_cnt, mem_info.max_used, mem_info.used_pct);
	printf("%s   Frag Percent: %u\n", TAG, mem_info.frag_pct);
#endif
}


void gui_enable_card_present_updating(bool en)
{
	if (en) {
		// Send an update request immediately
		(void) cmd_send(CMD_GET, CMD_CARD_PRESENT);
		
		if (task_card_update_timer == NULL) {			
			// Start the timer
			task_card_update_timer = lv_task_create(_cb_task_card_update_timer, GUI_CARD_PRESENT_POLL_MSEC, LV_TASK_PRIO_MID, NULL);
		}
	} else {
		if (task_card_update_timer != NULL) {
			lv_task_del(task_card_update_timer);
			task_card_update_timer = NULL;
		}
	}
}


bool gui_popup_displayed()
{
	return (msg_box_bg != NULL) || (win_keypad != NULL) || (act_pu_bg != NULL);
}


void gui_display_message_box(lv_obj_t* parent, const char* msg, bool dual_button, messagebox_handler_t cb_mbox)
{
	uint16_t parent_w, parent_h;
	
	// Don't open a messagebox over another one
	if (msg_box_bg != NULL) return;
	
	// Set the callback handler
	msg_box_cb = cb_mbox;
	
	// Create a base object for the modal background that covers the parent with opacity
	parent_w = lv_obj_get_width(parent);
	parent_h = lv_obj_get_height(parent);
	msg_box_bg = lv_obj_create(parent, NULL);
	lv_obj_set_pos(msg_box_bg, 0, 0);
	lv_obj_set_size(msg_box_bg, parent_w, parent_h);
	lv_obj_set_style_local_bg_color(msg_box_bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
	lv_obj_set_style_local_bg_opa(msg_box_bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_50);
	lv_obj_set_event_cb(msg_box_bg, _cb_mbox);
	
	// Create the message box as a child of the modal background
	msg_box = lv_msgbox_create(msg_box_bg, NULL);
	lv_msgbox_set_anim_time(msg_box, 0);
	lv_msgbox_set_text(msg_box, msg);
	if (dual_button) {
		lv_msgbox_add_btns(msg_box, msg_box_buttons2);
	} else {
		lv_msgbox_add_btns(msg_box, msg_box_buttons1);
	}
	lv_obj_set_size(msg_box, GUI_MSG_BOX_W, GUI_MSG_BOX_H);
	lv_obj_align(msg_box, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_event_cb(msg_box, _cb_mbox);
}
		

bool gui_message_box_displayed()
{
	return (msg_box_bg != NULL);
}


void gui_display_keypad(lv_obj_t* parent, int type, const char* name, char* val, int val_len, keypad_handler_t cb_keypad)
{
	// Don't open a keypad over another one
	if (win_keypad != NULL) return;
	
	// Setup
	keypad_val_buf = val;
	keypad_val_buf_len = val_len;
	keypad_type = type;
	keypad_cb = cb_keypad;
	
	// Create the keypad window
	win_keypad = lv_win_create(parent, NULL);
	lv_win_set_title(win_keypad, name);
	lv_obj_set_width(win_keypad, lv_obj_get_width(parent));
	lv_obj_align(win_keypad, parent, LV_ALIGN_CENTER, 0, 0);
	lv_win_set_layout(win_keypad, LV_LAYOUT_COLUMN_MID);
	lv_win_set_drag(win_keypad, true);
	lv_obj_set_style_local_border_width(win_keypad, LV_WIN_PART_BG, LV_STATE_DEFAULT, 5);
	lv_obj_set_style_local_border_color(win_keypad, LV_WIN_PART_BG, LV_STATE_DEFAULT, LV_THEME_DEFAULT_COLOR_SECONDARY);
	lv_obj_set_style_local_border_width(win_keypad, LV_WIN_PART_HEADER, LV_STATE_DEFAULT, 5);
	lv_obj_set_style_local_border_color(win_keypad, LV_WIN_PART_HEADER, LV_STATE_DEFAULT, LV_THEME_DEFAULT_COLOR_SECONDARY);
	lv_obj_set_style_local_pad_top(win_keypad, LV_WIN_PART_CONTENT_SCROLLABLE, LV_STATE_DEFAULT, 5);
	lv_obj_set_style_local_pad_inner(win_keypad, LV_WIN_PART_CONTENT_SCROLLABLE, LV_STATE_DEFAULT, 10);
	
	btn_kp_close = lv_win_add_btn(win_keypad, LV_SYMBOL_CLOSE);
	lv_obj_set_event_cb(btn_kp_close, _cb_keypad);
	btn_kp_accept = lv_win_add_btn(win_keypad, LV_SYMBOL_OK);
	lv_obj_set_event_cb(btn_kp_accept, _cb_keypad);
	
	// Create the local text display of entered value
	ta_kp_value = lv_textarea_create(win_keypad, NULL);
	lv_textarea_set_one_line(ta_kp_value, true);
	lv_textarea_set_max_length(ta_kp_value, val_len);
	lv_textarea_set_text(ta_kp_value, val);
#ifndef ESP_PLATFORM
	lv_group_add_obj(gui_keypad_group, ta_kp_value);
#endif
	
	// Create the button array
	btnm_kp = lv_keyboard_create(win_keypad, NULL);
	if (keypad_type == GUI_KEYPAD_TYPE_ALPHA) {
		lv_obj_set_width(ta_kp_value, lv_obj_get_width(win_keypad) - 20);
		lv_textarea_set_text_align(ta_kp_value, LV_LABEL_ALIGN_LEFT);
		lv_keyboard_set_mode(btnm_kp, LV_KEYBOARD_MODE_TEXT_LOWER);
		lv_obj_set_width(btnm_kp, lv_obj_get_width(win_keypad) - 2);
	} else {
		lv_obj_set_width(ta_kp_value, 200);
		lv_textarea_set_text_align(ta_kp_value, LV_LABEL_ALIGN_CENTER);
		lv_keyboard_set_mode(btnm_kp, LV_KEYBOARD_MODE_NUM);
		lv_obj_set_width(btnm_kp, 200);
	}
	lv_obj_set_height(btnm_kp, 160);
	lv_obj_set_style_local_border_width(btnm_kp, LV_KEYBOARD_PART_BG, LV_STATE_DEFAULT, 0);
	lv_keyboard_set_textarea(btnm_kp, ta_kp_value);
    lv_keyboard_set_cursor_manage(btnm_kp, true);
    lv_obj_set_event_cb(btnm_kp, _cb_keypad);

	lv_win_set_content_size(win_keypad, lv_obj_get_width(win_keypad) - 2, lv_obj_get_height(ta_kp_value) + lv_obj_get_height(btnm_kp) + 5);
	lv_obj_set_height(win_keypad, lv_obj_get_height(ta_kp_value) + lv_obj_get_height(btnm_kp) + 80);
}


bool gui_keypad_displayed()
{
	return (win_keypad != NULL);
}


void gui_send_activity_command(enum cmd_ctrl_act_param cmd, int32_t aux_data, lv_obj_t* parent, const char* desc)
{
	uint8_t buf[8];
	uint32_t* tx32P = (uint32_t*) buf;
	
	// Don't send the command if another is currently running
	if (act_pu_bg != NULL) return;
	
	// Send the command
	*tx32P++ = htonl((uint32_t) cmd);
	*tx32P = htonl((uint32_t) aux_data);
	if (cmd_send_binary(CMD_SET, CMD_CTRL_ACTIVITY, 8, buf)) {
		// Display the popup
		gui_display_activity_popup(parent, desc);
	}
}


void gui_display_activity_popup(lv_obj_t* parent, const char* desc)
{
	uint16_t parent_w, parent_h;
	
	// Don't create a popup over another one
	if (act_pu_bg != NULL) return;
	
	// Save our parent and then disable it so it can't receive input
	act_pu_parent = parent;
	
	// Create a base object for the modal background that covers the parent with opacity
	parent_w = lv_obj_get_width(parent);
	parent_h = lv_obj_get_height(parent);
	act_pu_bg = lv_obj_create(parent, NULL);
	lv_obj_set_pos(act_pu_bg, 0, 0);
	lv_obj_set_size(act_pu_bg, parent_w, parent_h);
	lv_obj_set_click(act_pu_bg, false);
	lv_obj_set_style_local_bg_color(act_pu_bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
	lv_obj_set_style_local_bg_opa(act_pu_bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_50);
	lv_obj_set_event_cb(act_pu_bg, _cb_activity_pu);
	
	// Create a centered object to hold the displayed objects
	act_pu_win = lv_obj_create(act_pu_bg, NULL);
	lv_obj_set_size(act_pu_win, GUI_ACTIVITY_PU_W, GUI_ACTIVITY_PU_H);
	lv_obj_align_origo(act_pu_win, act_pu_bg, LV_ALIGN_CENTER, 0, 0);
	
	// Add a spinner to the top center
	act_pu_spinner = lv_spinner_create(act_pu_win, NULL);
    lv_obj_set_size(act_pu_spinner, GUI_ACT_PU_SPIN_W, GUI_ACT_PU_SPIN_H);
    lv_obj_set_pos(act_pu_spinner, (GUI_ACTIVITY_PU_W - GUI_ACT_PU_SPIN_W) / 2, GUI_ACT_PU_SPIN_OFF_Y);
	
	// Add the descriptive text centered below spinner
	act_pu_desc = lv_label_create(act_pu_win, NULL);
	lv_label_set_long_mode(act_pu_desc, LV_LABEL_LONG_BREAK);
	lv_label_set_align(act_pu_desc, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_width(act_pu_desc, GUI_ACTIVITY_PU_W - 10);
	lv_obj_set_pos(act_pu_desc, 5, GUI_ACT_PU_SPIN_H + 2*GUI_ACT_PU_SPIN_OFF_Y);
	lv_label_set_text(act_pu_desc, desc);
}


void gui_update_activity_popup(bool success)
{
	// Don't execute if our page has gone away
	if (act_pu_bg == NULL) return;
	
	// Reconfigure the desc to use a larger font
	lv_obj_set_style_local_text_font(act_pu_desc, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
	
	if (success) {
		lv_label_set_static_text(act_pu_desc, "SUCCEEDED");
	} else {
		lv_label_set_static_text(act_pu_desc, "FAILED");
	}
	
	// Start a timer to end display of this popup
	if (task_act_pu_timer == NULL) {
			// Start the timer
			task_act_pu_timer = lv_task_create(_cb_task_act_pu_timer, GUI_ACT_PU_DISP_MSEC, LV_TASK_PRIO_MID, NULL);
		} else {
			// Reset timer
			lv_task_reset(task_act_pu_timer);
		}
}


bool gui_activity_popup_displayed()
{
	return (act_pu_bg != NULL);
}



//
// Internal functions
//
static void _cb_mbox(lv_obj_t *obj, lv_event_t event)
{
	if (event == LV_EVENT_DELETE) {
		if (obj == msg_box_bg) {
			msg_box_bg = NULL;
		} else if (obj == msg_box) {
			// Delete the parent modal background
			lv_obj_del_async(lv_obj_get_parent(msg_box));
			msg_box = NULL; // happens before object is actually deleted!
		}
	} else if ((event == LV_EVENT_VALUE_CHANGED) && (obj == msg_box)) {
		// Let the calling page know a button was clicked
		if (msg_box_cb != NULL) {
			msg_box_cb((int) lv_msgbox_get_active_btn(obj));
		}
		
		// Delete the message box
		lv_obj_del(msg_box);
	}
}


static void _cb_keypad(lv_obj_t *obj, lv_event_t event)
{
	if ((event == LV_EVENT_APPLY) || ((event == LV_EVENT_CLICKED) && (obj == btn_kp_accept))) {
		if (keypad_cb != NULL) {
			strncpy(keypad_val_buf, lv_textarea_get_text(ta_kp_value), keypad_val_buf_len);
			if (keypad_type == GUI_KEYPAD_TYPE_NUMERIC) {
				// We have to sanitize the LVGL numeric keypad type of any leading '+' characters
				if (keypad_val_buf[0] == '+') {
					int n = strlen(keypad_val_buf);
					for (int i=0; i<n; i++) {
						keypad_val_buf[i] = keypad_val_buf[i+1];  // Will copy the final null too
					}
				}
			}
			keypad_cb(GUI_KEYPAD_EVENT_CLOSE_ACCEPT);
		}
		lv_obj_del(win_keypad);
        win_keypad = NULL;
	} else if ((event == LV_EVENT_CANCEL) || ((event == LV_EVENT_CLICKED) && (obj == btn_kp_close))) {
		if (keypad_cb != NULL) {
			keypad_cb(GUI_KEYPAD_EVENT_CLOSE_CANCEL);
		}
		lv_obj_del(win_keypad);
        win_keypad = NULL;
	} else if (obj == btnm_kp) {
		// Let the default keyboard handler process all other key codes
		lv_keyboard_def_event_cb(btnm_kp, event);
	}
}


static void _cb_activity_pu(lv_obj_t *obj, lv_event_t event)
{
	if (event == LV_EVENT_DELETE) {
		// Redisplay our parent
		lv_obj_set_hidden(act_pu_parent, false);
		
		act_pu_bg = NULL;
	}
}


static void _cb_task_card_update_timer(lv_task_t* task)
{
	// Request current card present status
	(void) cmd_send(CMD_GET, CMD_CARD_PRESENT);
}


static void _cb_task_act_pu_timer(lv_task_t* task)
{
	// Delete the popup
	lv_obj_del(act_pu_bg);
	
	// Terminate the timer
	lv_task_del(task_act_pu_timer);
	task_act_pu_timer = NULL;
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
