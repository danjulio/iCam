/*
 * GUI shutter maximum interval control panel
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

#include "gui_page_settings.h"
#include "gui_sub_page_shutter.h"
#include "gui_panel_shutter_max_interval.h"
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

#define MAX_VAL_LEN 3


//
// Local variables
//

// State
static char val_buf[11];           // Sized to hold a uint32 as decimal number
static char working_val_buf[11];

//
// LVGL Objects
//
static lv_obj_t* my_parent_page;
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* ctrl_assy;
static lv_obj_t* btn_val;
static lv_obj_t* lbl_val;
static lv_obj_t* lbl_val_units;

// Buffer for pop-up string
static char msg_box_buf[80];


//
// Forward declarations for internal functions
//
static void _cb_btn_val(lv_obj_t* obj, lv_event_t event);
static void _cb_keypad_handler(int kp_event);


//
// API
//
void gui_panel_shutter_max_interval_init(lv_obj_t* parent_cont)
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
	lv_label_set_static_text(lbl_name, "Maximum Shutter Interval");
	
	// Control assembly (so my_panel container spaces it correctly)
	ctrl_assy = lv_obj_create(my_panel, NULL);
	lv_obj_set_click(ctrl_assy, false);
	lv_obj_set_height(ctrl_assy, GUIPN_SHUTTER_MAX_INTERVAL_BTN_H + 10);
	lv_obj_set_width(ctrl_assy, GUIPN_SHUTTER_MAX_INTERVAL_BTN_W + GUIPN_SHUTTER_MAX_INTERVAL_UNIT_W);
	lv_obj_set_style_local_border_width(ctrl_assy, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
	
	// Button to hold value
	btn_val = lv_btn_create(ctrl_assy, NULL);
	lv_obj_set_y(btn_val, 5);
	lv_obj_add_protect(btn_val, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_size(btn_val, GUIPN_SHUTTER_MAX_INTERVAL_BTN_W, GUIPN_SHUTTER_MAX_INTERVAL_BTN_H);
	lv_obj_set_event_cb(btn_val, _cb_btn_val);
	
	// Button Label - value set when displayed
	lbl_val = lv_label_create(btn_val, NULL);
	lv_label_set_long_mode(lbl_val, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_val, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_width(lbl_val, GUIPN_SHUTTER_MAX_INTERVAL_BTN_W);
	lv_label_set_static_text(lbl_val, "");

	// Add value units label to the right - units set when displayed
	lbl_val_units = lv_label_create(ctrl_assy, NULL);
	lv_obj_set_width(lbl_val_units, GUIPN_SHUTTER_MAX_INTERVAL_UNIT_W);
	lv_obj_align(lbl_val_units, btn_val, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
	lv_label_set_static_text(lbl_val_units, "sec");
	
    // Register with our parent page
	gui_sub_page_shutter_register_panel(my_panel);
}


void gui_panel_shutter_max_interval_set_active(bool is_active)
{
	if (is_active) {
		// Get the current value
#ifdef ESP_PLATFORM
		sprintf(val_buf, "%lu", gui_state.max_ffc_interval);
#else
		sprintf(val_buf, "%u", gui_state.max_ffc_interval);
#endif
		lv_label_set_static_text(lbl_val, val_buf);
	}
}



//
// Internal functions
//
static void _cb_btn_val(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		if (!gui_keypad_displayed()) {
			strcpy(working_val_buf, val_buf);
			gui_display_keypad(my_parent_page, GUI_KEYPAD_TYPE_NUMERIC, "Enter Maximum FFC Interval (secs)", working_val_buf, MAX_VAL_LEN, _cb_keypad_handler);
		}
	}
}


static void _cb_keypad_handler(int kp_event)
{	
	uint32_t t;
	
	if (kp_event == GUI_KEYPAD_EVENT_CLOSE_ACCEPT) {
		if (strlen(working_val_buf) == 0) {
			gui_display_message_box(my_parent_page, "Cannot enter blank value", GUI_MSG_BOX_1_BTN, NULL);
		} else if (!gui_validate_numeric_text(working_val_buf)) {
			gui_display_message_box(my_parent_page, "Must enter numeric value", GUI_MSG_BOX_1_BTN, NULL);
		} else {
			// Convert the string back to a numeric value
			t = round(atof(working_val_buf));
			
			// Validate and update state if valid
			if ((t < gui_state.min_ffc_interval) || (t > 655)) {
#ifdef ESP_PLATFORM
				sprintf(msg_box_buf, "Interval out of range %lu to 655 seconds", gui_state.min_ffc_interval);
#else
				sprintf(msg_box_buf, "Interval out of range %u to 655 seconds", gui_state.min_ffc_interval);
#endif
				gui_display_message_box(my_parent_page, msg_box_buf, GUI_MSG_BOX_1_BTN, NULL);
			} else {
#ifdef ESP_PLATFORM
				sprintf(val_buf, "%lu", t);
#else
				sprintf(val_buf, "%u", t);
#endif
				lv_obj_invalidate(lbl_val);
				gui_state.max_ffc_interval = t;
				
				gui_sub_page_shutter_note_change();
			}
		}
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
