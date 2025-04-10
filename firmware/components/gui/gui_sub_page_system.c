/*
 * GUI system utilities sub-page
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
#include "gui_sub_page_system.h"
#include "gui_page_settings.h"
#include "gui_panel_system_restore.h"
#include "gui_panel_system_format.h"
#include "gui_panel_system_tiny1c_cal_1.h"
#include "gui_panel_system_tiny1c_cal_2L.h"
#include "gui_panel_system_tiny1c_cal_2H.h"
#include "gui_utilities.h"
#include <string.h>

#ifdef ESP_PLATFORM
	#include "esp_log.h"
	#include "gui_task.h"
#else
	#include "gui_main.h"
	#include <stdio.h>
#endif



//
// Local constants
//

// Maximum number of control panels we can add to this page
#define MAX_CONTROL_PANELS  5

//
// Local variables
//
static const char* TAG = "gui_sub_page_system";

// State
static bool my_page_active = false;
static int num_control_panels = 0;

// LVGL objects
static lv_obj_t* my_page;
static lv_obj_t* btn_back;
static lv_obj_t* lbl_btn_back;
static lv_obj_t* lbl_title;
static lv_obj_t* lbl_instructions;
static lv_obj_t* page_controls;
static lv_obj_t* page_controls_scrollable;

// Control panels
static lv_obj_t* panel_array[MAX_CONTROL_PANELS];

// Instructions to the user ('cause this page can kinda screw things up...)
static char* instructions = "Please refer to the instructions before executing the utility functions here.";



//
// Forward declarations for internal routines
//
static void _init_panel_arrays();
static void _cb_back_button(lv_obj_t* obj, lv_event_t event);



//
// API
//
lv_obj_t* gui_sub_page_system_init(lv_obj_t* screen)
{
	// Initialize the arrays we use to manage panels and sub-pages
	_init_panel_arrays();
	
	// Create the top-level container for the page
	my_page = lv_obj_create(screen, NULL);
	lv_obj_set_pos(my_page, 0, 0);
	lv_obj_set_click(my_page, false);
		
	// Add the top-level controls
	//
	// Back button (fixed location)
	btn_back = lv_btn_create(my_page, NULL);
	lv_obj_set_pos(btn_back, GUISP_SYSTEM_BACK_X, GUISP_SYSTEM_BACK_Y);
	lv_obj_set_size(btn_back, GUISP_SYSTEM_BACK_W, GUISP_SYSTEM_BACK_H);
	lv_obj_add_protect(btn_back, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_border_width(btn_back, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_border_width(btn_back, LV_BTN_PART_MAIN, LV_STATE_PRESSED, 0);
	lv_obj_set_event_cb(btn_back, _cb_back_button);
	
	lbl_btn_back = lv_label_create(btn_back, NULL);
	lv_obj_set_style_local_text_font(lbl_btn_back, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
	lv_label_set_static_text(lbl_btn_back, LV_SYMBOL_LEFT);
	
	// Title (dynamic width)
	lbl_title = lv_label_create(my_page, NULL);
	lv_obj_set_style_local_text_font(lbl_title, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
	lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_title, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_pos(lbl_title, GUISP_SYSTEM_TITLE_X, GUISP_SYSTEM_TITLE_Y);
	lv_label_set_static_text(lbl_title, "System Utilities");
	
	// Instructions (dynamic width)
	lbl_instructions = lv_label_create(my_page, NULL);
	lv_label_set_long_mode(lbl_instructions, LV_LABEL_LONG_BREAK);
	lv_label_set_align(lbl_instructions, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_pos(lbl_instructions, GUISP_SYSTEM_INST_X, GUISP_SYSTEM_INST_Y);
	lv_label_set_static_text(lbl_instructions, instructions);
	
	// Control page (dynamic size)
	page_controls = lv_page_create(my_page, NULL);
	lv_page_set_scrollable_fit2(page_controls, LV_FIT_PARENT, LV_FIT_TIGHT);
	lv_page_set_scrl_layout(page_controls, LV_LAYOUT_COLUMN_LEFT);
	lv_obj_set_auto_realign(page_controls, true);
	lv_obj_align_origo(page_controls, NULL, LV_ALIGN_CENTER, 0, GUIP_SETTINGS_CONTROL_Y);
	lv_obj_set_style_local_pad_top(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_TOP_PAD);
	lv_obj_set_style_local_pad_bottom(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_BTM_PAD);
	lv_obj_set_style_local_pad_left(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_LEFT_PAD);
	lv_obj_set_style_local_pad_right(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_RIGHT_PAD);
	lv_obj_set_style_local_pad_inner(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_INNER_PAD);
	lv_obj_set_style_local_border_width(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
		
	page_controls_scrollable = lv_page_get_scrollable(page_controls);
	lv_obj_add_protect(page_controls_scrollable, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_pad_inner(page_controls_scrollable, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_INNER_PAD);
	
	//Add our control panel
	gui_panel_system_format_init(page_controls);
	gui_panel_system_restore_init(page_controls);
	gui_panel_system_tiny1c_cal_1_init(page_controls);
	gui_panel_system_tiny1c_cal_2L_init(page_controls);
	gui_panel_system_tiny1c_cal_2H_init(page_controls);
	
	// We start off disabled
	lv_obj_set_hidden(my_page, true);
	
	return my_page;
}


void gui_sub_page_system_set_active(bool is_active)
{
	// Set our visibility
	my_page_active = is_active;
	lv_obj_set_hidden(my_page, !is_active);
	
	// Inform controls so they can manage state
	gui_panel_system_format_set_active(is_active);
	gui_panel_system_restore_set_active(is_active);
	gui_panel_system_tiny1c_cal_1_set_active(is_active);
	gui_panel_system_tiny1c_cal_2L_set_active(is_active);
	gui_panel_system_tiny1c_cal_2H_set_active(is_active);
}


void gui_sub_page_system_reset_screen_size(uint16_t page_w, uint16_t page_h)
{
	// Set page dimensions
	lv_obj_set_size(my_page, page_w, page_h);
	
	// Always start off just to the right of the calling page when disabled
	if (!my_page_active) {
		lv_obj_set_pos(my_page, page_w, 0);
	}
	
	// Set title max width
	lv_obj_set_width(lbl_title, page_w - (2 * GUISP_SYSTEM_TITLE_X));
	
	// Set instructions max width
	lv_obj_set_width(lbl_instructions, page_w - (2 * GUISP_SYSTEM_INST_X));
	
	// Set control page dimensions
	lv_obj_set_size(page_controls, page_w, page_h - GUISP_SYSTEM_CONTROL_Y);
	lv_page_set_scrl_width(page_controls, page_w);
}


void gui_sub_page_system_register_panel(lv_obj_t* panel)
{
	if (num_control_panels < MAX_CONTROL_PANELS) {
		panel_array[num_control_panels] = panel;
		num_control_panels += 1;
	} else {
#ifdef ESP_PLATFORM
		ESP_LOGE(TAG, "Add control panel failed");
#else
		printf("%s Add control panel failed\n", TAG);
#endif
	}
}



//
// Internal functions
//
static void _init_panel_arrays()
{
	for (int i=0; i<MAX_CONTROL_PANELS; i++) {
		panel_array[i] = NULL;
	}
}


static void _cb_back_button(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		if (!gui_popup_displayed()) {
			gui_page_settings_close_sub_page(my_page);
		}
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
