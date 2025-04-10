/*
 * GUI system settings Tiny1C high point of 2-point calibration control panel
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
#include "gui_sub_page_system.h"
#include "gui_panel_system_tiny1c_cal_2H.h"
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

// Value string for "-XXX" to "XXXX.X" (theoretically)
#define MAX_CAL_T_LEN 6


//
// Local variables
//

// State
static char val_buf[MAX_CAL_T_LEN+1];
static char working_val_buf[MAX_CAL_T_LEN+1];
static float blackbody_temp = GUIPN_SYSTEM_TINY1C_CAL_2H_TEMP;

//
// LVGL Objects
//
static lv_obj_t* my_parent_page;
static lv_obj_t* my_panel;

static lv_obj_t* row1;
static lv_obj_t* lbl_name;
static lv_obj_t* btn_cal;
static lv_obj_t* lbl_btn_cal;

static lv_obj_t* row2;
static lv_obj_t* ctrl_assy;
static lv_obj_t* lbl_blackbody;
static lv_obj_t* btn_val;
static lv_obj_t* lbl_val;
static lv_obj_t* lbl_val_units;



//
// Forward declarations for internal functions
//
static void _cb_btn_cal(lv_obj_t* obj, lv_event_t event);
static void _cb_btn_val(lv_obj_t* obj, lv_event_t event);
static void _cb_keypad_handler(int kp_event);
static void _cb_messagebox(int btn_id);


//
// API
//
void gui_panel_system_tiny1c_cal_2H_init(lv_obj_t* parent_cont)
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
	
	// Parent for objects in first row
	row1 = lv_cont_create(my_panel, NULL);
	lv_obj_set_click(row1, false);
	lv_obj_set_auto_realign(row1, true);
	lv_cont_set_fit2(row1, LV_FIT_PARENT, LV_FIT_TIGHT);
	lv_cont_set_layout(row1, LV_LAYOUT_PRETTY_MID);
	lv_obj_set_style_local_pad_top(row1, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_bottom(row1, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_left(row1, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_right(row1, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_border_width(row1, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

	// Panel name
	lbl_name = lv_label_create(row1, NULL);
	lv_label_set_static_text(lbl_name, "H 2 point calibration");
	
	// Restore Factory Defaults Button
	btn_cal = lv_btn_create(row1, NULL);
	lv_obj_add_protect(btn_cal, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_size(btn_cal, GUIPN_SYSTEM_TINY1C_CAL_2H_BTN_W, GUIPN_SYSTEM_TINY1C_CAL_2H_BTN_H);
	lv_obj_set_event_cb(btn_cal, _cb_btn_cal);
	
	// Button Label
	lbl_btn_cal = lv_label_create(btn_cal, NULL);
	lv_label_set_align(lbl_btn_cal, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_btn_cal, "Start");
	
	// Parent for objects in second row
	row2 = lv_cont_create(my_panel, NULL);
	lv_obj_set_click(row2, false);
	lv_obj_set_auto_realign(row2, true);
	lv_cont_set_fit2(row2, LV_FIT_PARENT, LV_FIT_TIGHT);
	lv_cont_set_layout(row2, LV_LAYOUT_PRETTY_MID);
	lv_obj_set_style_local_pad_top(row2, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_bottom(row2, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_left(row2, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_right(row2, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_border_width(row2, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

	// Blackbody temp control label
	lbl_blackbody = lv_label_create(row2, NULL);
	lv_label_set_static_text(lbl_blackbody, "Blackbody Temp");
	
	// Control assembly for blackbody temp (so my_panel container spaces it correctly)
	ctrl_assy = lv_obj_create(row2, NULL);
	lv_obj_set_click(ctrl_assy, false);
	lv_obj_set_height(ctrl_assy, GUIPN_SYSTEM_TINY1C_CAL_2H_BTN_H + 10);
	lv_obj_set_width(ctrl_assy, GUIPN_SYSTEM_TINY1C_CAL_2H_BTN_W + GUIPN_SYSTEM_TINY1C_CAL_2H_UNIT_W);
	lv_obj_set_style_local_border_width(ctrl_assy, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
	
	// Button to hold blackbody temp value
	btn_val = lv_btn_create(ctrl_assy, NULL);
	lv_obj_set_y(btn_val, 5);
	lv_obj_add_protect(btn_val, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_size(btn_val, GUIPN_SYSTEM_TINY1C_CAL_2H_BTN_W, GUIPN_SYSTEM_TINY1C_CAL_2H_BTN_H);
	lv_obj_set_event_cb(btn_val, _cb_btn_val);
	
	// Button Label - value set when displayed
	lbl_val = lv_label_create(btn_val, NULL);
	lv_label_set_long_mode(lbl_val, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_val, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_width(lbl_val, GUIPN_SYSTEM_TINY1C_CAL_2H_BTN_W);
	lv_label_set_static_text(lbl_val, "");

	// Add value units label to the right - units set when displayed
	lbl_val_units = lv_label_create(ctrl_assy, NULL);
	lv_obj_set_width(lbl_val_units, GUIPN_SYSTEM_TINY1C_CAL_2H_UNIT_W);
	lv_obj_align(lbl_val_units, btn_val, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
	if (gui_state.temp_unit_C) {
		lv_label_set_static_text(lbl_val_units, "°C");
	} else {
		lv_label_set_static_text(lbl_val_units, "°F");
	}
	
    // Register with our parent page
	gui_sub_page_system_register_panel(my_panel);
}


void gui_panel_system_tiny1c_cal_2H_set_active(bool is_active)
{
	if (is_active) {
		// Set the current temp units
		sprintf(val_buf, "%1.1f", gui_float_c_to_disp_temp(blackbody_temp, &gui_state));
		lv_label_set_static_text(lbl_val, val_buf);
		if (gui_state.temp_unit_C) {
			lv_label_set_static_text(lbl_val_units, "°C");
		} else {
			lv_label_set_static_text(lbl_val_units, "°F");
		}
	}
}



//
// Internal functions
//
static void _cb_btn_cal(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		// We check keypad displayed so that this won't trigger if pressed while the
		// keypad is displayed
		if (!gui_keypad_displayed()) {
			// Display the "are you really sure?" messagebox
			gui_display_message_box(my_parent_page, "Start Calibration?", GUI_MSG_BOX_2_BTN, _cb_messagebox);
		}
	}
}


static void _cb_btn_val(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		if (!gui_keypad_displayed()) {
			strcpy(working_val_buf, val_buf);
			gui_display_keypad(my_parent_page, GUI_KEYPAD_TYPE_NUMERIC, "Enter Blackbody Temp", working_val_buf, MAX_CAL_T_LEN, _cb_keypad_handler);
		}
	}
}


static void _cb_keypad_handler(int kp_event)
{	
	float t;
	
	if (kp_event == GUI_KEYPAD_EVENT_CLOSE_ACCEPT) {
		if (strlen(working_val_buf) == 0) {
			gui_display_message_box(my_parent_page, "Cannot enter blank value", GUI_MSG_BOX_1_BTN, NULL);
		} else if (!gui_validate_numeric_text(working_val_buf)) {
			gui_display_message_box(my_parent_page, "Must enter numeric value", GUI_MSG_BOX_1_BTN, NULL);
		} else {
			// Convert the string back to a numeric °C temperature
			t = atof(working_val_buf);		
			if (!gui_state.temp_unit_C) {
				t = (t - 32.0) * 5 / 9;
			}
			
			// Validate and update state if valid
			if ((t < -40) || (t > 900)) {
				gui_display_message_box(my_parent_page, "Temperature out of range -40 to 900°C", GUI_MSG_BOX_1_BTN, NULL);
			} else {
				blackbody_temp = t;
				sprintf(val_buf, "%1.1f", gui_float_c_to_disp_temp(blackbody_temp, &gui_state));
				lv_obj_invalidate(lbl_val);
			}
		}
	}
}


static void _cb_messagebox(int btn_id)
{
	if (btn_id == GUI_MSG_BOX_BTN_AFFIRM) {
		// Send command to start calibration
		gui_send_activity_command(CMD_CTRL_ACT_TINY1C_CAL_2H, (int32_t) round(blackbody_temp + 273.15), my_parent_page, "Calibration in progress");
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
