/*
 * GUI timelapse settings number of images control panel
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
#include "gui_sub_page_timelapse.h"
#include "gui_panel_timelapse_num_img.h"
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

// Value string for "1" to "NNNN" (theoretically)
#define MAX_VAL_LEN 4


//
// Local variables
//

// State
static char val_buf[MAX_VAL_LEN+1];
static char working_val_buf[MAX_VAL_LEN+1];

//
// LVGL Objects
//
static lv_obj_t* my_parent_page;
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* btn_val;
static lv_obj_t* lbl_val;



//
// Forward declarations for internal functions
//
static void _cb_btn_val(lv_obj_t* obj, lv_event_t event);
static void _cb_keypad_handler(int kp_event);


//
// API
//
void gui_panel_timelapse_num_img_init(lv_obj_t* parent_cont)
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
	lv_label_set_static_text(lbl_name, "Number of Images");
	
	// Button to hold value
	btn_val = lv_btn_create(my_panel, NULL);
	lv_obj_set_y(btn_val, 5);
	lv_obj_add_protect(btn_val, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_size(btn_val, GUIPN_TIMELAPSE_NUM_IMG_BTN_W, GUIPN_TIMELAPSE_NUM_IMG_BTN_H);
	lv_obj_set_event_cb(btn_val, _cb_btn_val);
	
	// Button Label - value set when displayed
	lbl_val = lv_label_create(btn_val, NULL);
	lv_label_set_long_mode(lbl_val, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_val, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_width(lbl_val, GUIPN_TIMELAPSE_NUM_IMG_BTN_W);
	lv_label_set_static_text(lbl_val, "");
	
    // Register with our parent page
	gui_sub_page_timelapse_register_panel(my_panel);
}


void gui_panel_timelapse_num_img_set_active(bool is_active)
{
	if (is_active) {
		// Get the current value
		sprintf(val_buf, "%d", (int) gui_state.timelapse_num_img);
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
			gui_display_keypad(my_parent_page, GUI_KEYPAD_TYPE_NUMERIC, "Enter Num Images", working_val_buf, MAX_VAL_LEN, _cb_keypad_handler);
		}
	}
}


static void _cb_keypad_handler(int kp_event)
{	
	int n;
	
	if (kp_event == GUI_KEYPAD_EVENT_CLOSE_ACCEPT) {
		if (strlen(working_val_buf) == 0) {
			gui_display_message_box(my_parent_page, "Cannot enter blank value", GUI_MSG_BOX_1_BTN, NULL);
		} else if (!gui_validate_numeric_text(working_val_buf)) {
			gui_display_message_box(my_parent_page, "Must enter numeric value", GUI_MSG_BOX_1_BTN, NULL);
		} else {
			// Convert the string back to a numeric value
			n = round(atof(working_val_buf));
			
			// Validate and update state if valid
			if ((n <= 0) || (n > 9999)) {
				gui_display_message_box(my_parent_page, "Number must be between 1 and 9999", GUI_MSG_BOX_1_BTN, NULL);
			} else {
				gui_state.timelapse_num_img = (uint32_t) n;
				sprintf(val_buf, "%d", (int) gui_state.timelapse_num_img);
				lv_obj_invalidate(lbl_val);
				
				gui_sub_page_timelapse_note_change();
			}
		}
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
