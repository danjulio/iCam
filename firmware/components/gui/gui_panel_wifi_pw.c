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
#include "esp_system.h"
#ifndef CONFIG_BUILD_ICAM_MINI

#include "cmd_utilities.h"
#include "gui_page_settings.h"
#include "gui_sub_page_wifi.h"
#include "gui_panel_wifi_pw.h"
#include "gui_state.h"
#include "gui_utilities.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM
	#include "gui_task.h"
#else
	#include "gui_main.h"
#endif


//
// Local constants
//



//
// Local variables
//

// State
static bool password_hidden = true;
static char disp_buf[GUI_PW_MAX_LEN+1] = { 0 };
static char val_buf[GUI_PW_MAX_LEN+1] = { 0 };
static char working_val_buf[GUI_PW_MAX_LEN+1];

//
// LVGL Objects
//
static lv_obj_t* my_parent_page;
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* ctrl_assy;
static lv_obj_t* btn_val;
static lv_obj_t* lbl_btn_val;
static lv_obj_t* btn_hide;
static lv_obj_t* lbl_btn_hide;



//
// Forward declarations for internal functions
//
static void _render_password_string();
static void _cb_btn_val(lv_obj_t* obj, lv_event_t event);
static void _cb_btn_hide(lv_obj_t* obj, lv_event_t event);
static void _cb_keypad_handler(int kp_event);


//
// API
//
void gui_panel_wifi_pw_init(lv_obj_t* parent_cont)
{
	// Get the top-level displayed page holding us for our pop-ups
	my_parent_page = lv_obj_get_parent(parent_cont);
	
	// Control panel - width fits parent, height fits contents with padding
	my_panel = lv_cont_create(parent_cont, NULL);
	lv_obj_set_click(my_panel, false);
	lv_obj_set_auto_realign(my_panel, true);
	lv_cont_set_fit2(my_panel, LV_FIT_PARENT, LV_FIT_TIGHT);
	lv_cont_set_layout(my_panel, LV_LAYOUT_PRETTY_MID);
	lv_obj_set_style_local_pad_top(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_TOP_PAD);
	lv_obj_set_style_local_pad_bottom(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_BTM_PAD);
	lv_obj_set_style_local_pad_left(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_LEFT_PAD);
	lv_obj_set_style_local_pad_right(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_RIGHT_PAD);
	
	// Panel name
	lbl_name = lv_label_create(my_panel, NULL);
	lv_label_set_static_text(lbl_name, "Password");
	
	// Control assembly (so my_panel container spaces it correctly)
	ctrl_assy = lv_obj_create(my_panel, NULL);
	lv_obj_set_click(ctrl_assy, false);
	lv_obj_set_height(ctrl_assy, GUIPN_WIFI_PW_BTN_H + 10);
	lv_obj_set_width(ctrl_assy, GUIPN_WIFI_PW_BTN_W + GUIPN_WIFI_PW_HIDE_BTN_W + GUIPN_WIFI_PW_BTN_SPACE);
	lv_obj_set_style_local_border_width(ctrl_assy, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
	
	// Button to hold value
	btn_val = lv_btn_create(ctrl_assy, NULL);
	lv_obj_set_size(btn_val, GUIPN_WIFI_PW_BTN_W, GUIPN_WIFI_PW_BTN_H);
	lv_obj_align(btn_val, ctrl_assy, LV_ALIGN_IN_LEFT_MID, GUIPN_WIFI_PW_BTN_SPACE, 0);
	lv_obj_add_protect(btn_val, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_event_cb(btn_val, _cb_btn_val);
	
	// Button Label - value set when displayed
	lbl_btn_val = lv_label_create(btn_val, NULL);
	lv_label_set_long_mode(lbl_btn_val, LV_LABEL_LONG_SROLL_CIRC);
	lv_label_set_align(lbl_btn_val, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_width(lbl_btn_val, GUIPN_WIFI_PW_BTN_W);
	lv_label_set_static_text(lbl_btn_val, disp_buf);
	
	// Hide button
	btn_hide = lv_btn_create(ctrl_assy, NULL);
	lv_obj_set_size(btn_hide, GUIPN_WIFI_PW_HIDE_BTN_W, GUIPN_WIFI_PW_HIDE_BTN_H);
	lv_obj_align(btn_hide, ctrl_assy, LV_ALIGN_IN_RIGHT_MID, 0, 0);
	lv_obj_add_protect(btn_hide, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_border_width(btn_hide, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_border_width(btn_hide, LV_BTN_PART_MAIN, LV_STATE_PRESSED, 0);
	lv_obj_set_event_cb(btn_hide, _cb_btn_hide);

	// Hide button label
	lbl_btn_hide = lv_label_create(btn_hide, NULL);
	lv_obj_set_width(lbl_btn_hide, GUIPN_WIFI_PW_HIDE_BTN_W);
	lv_label_set_static_text(lbl_btn_hide, password_hidden ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
	
    // Register with our parent page
	gui_sub_page_wifi_register_panel(my_panel);
}


void gui_panel_wifi_pw_set_active(bool is_active)
{
	if (is_active) {
		// Get the current value
		if (gui_state.sta_mode) {
			strcpy(val_buf, gui_state.sta_pw);
		} else {
			strcpy(val_buf, gui_state.ap_pw);
		}
		_render_password_string();
	}
}


void gui_panel_wifi_pw_note_updated_mode()
{
	if (gui_state.sta_mode) {
		strcpy(val_buf, gui_state.sta_pw);
	} else {
		strcpy(val_buf, gui_state.ap_pw);
	}
	_render_password_string();
}



//
// Internal functions
//
static void _render_password_string()
{
	if (password_hidden) {
		int n = strlen(val_buf);
		for (int i=0; i<n; i++) disp_buf[i] = '*';
		disp_buf[n] = 0;
	} else {
		strcpy(disp_buf, val_buf);
	}
	lv_obj_invalidate(lbl_btn_val);
}


static void _cb_btn_val(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		if (!gui_keypad_displayed()) {
			if (password_hidden) {
				// Start off with a blank string
				working_val_buf[0] = 0;
			} else {
				// Start off with the actual password
				strcpy(working_val_buf, val_buf);
			}	
			gui_display_keypad(my_parent_page, GUI_KEYPAD_TYPE_ALPHA, "Enter Password", working_val_buf, GUI_PW_MAX_LEN, _cb_keypad_handler);
		}
	}
}


static void _cb_btn_hide(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		password_hidden = !password_hidden;
		lv_label_set_static_text(lbl_btn_hide, password_hidden ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
		_render_password_string();
	}
}


static void _cb_keypad_handler(int kp_event)
{	
	int n;
	
	if (kp_event == GUI_KEYPAD_EVENT_CLOSE_ACCEPT) {
		n = strlen(working_val_buf);
		if (((n > 0) && (n < 8)) || (n > GUI_PW_MAX_LEN)) {
			gui_display_message_box(my_parent_page, "Password must be 0, or between 8 and 63 characters in length", GUI_MSG_BOX_1_BTN, NULL);
		} else {
			// Update if changed
			if (strcmp(val_buf, working_val_buf) != 0) {
				strcpy(val_buf, working_val_buf);
				if (gui_state.sta_mode) {
					strcpy(gui_state.sta_pw, working_val_buf);
				} else {
					strcpy(gui_state.ap_pw, working_val_buf);
				}
				_render_password_string();
				
				gui_sub_page_wifi_note_change(false);
			}
		}
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
