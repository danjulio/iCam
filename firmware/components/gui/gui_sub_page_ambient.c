/*
 * GUI system ambient correction sub-page
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
#include "gui_sub_page_ambient.h"
#include "gui_page_settings.h"
#include "gui_panel_ambient_sensor_comp.h"
#include "gui_panel_ambient_atmospheric_t.h"
#include "gui_panel_ambient_distance.h"
#include "gui_panel_ambient_reflected_t.h"
#include "gui_state.h"
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
#define MAX_CONTROL_PANELS  4

// This must match code below and in cmd set handler
#define CMD_AMBIENT_CORRECT_LEN 18



//
// Local variables
//
static const char* TAG = "gui_sub_page_ambient";

// State
static bool my_page_active = false;
static bool note_change = false;
static int num_control_panels = 0;

// LVGL objects
//
// Page
static lv_obj_t* my_page;
static lv_obj_t* btn_back;
static lv_obj_t* lbl_btn_back;
static lv_obj_t* lbl_title;
static lv_obj_t* page_controls;
static lv_obj_t* page_controls_scrollable;

// Control panels
static lv_obj_t* panel_array[MAX_CONTROL_PANELS];



//
// Forward declarations for internal routines
//
static void _init_panel_arrays();
static void _cb_back_button(lv_obj_t* obj, lv_event_t event);
static bool _send_ambient_info();



//
// API
//
lv_obj_t* gui_sub_page_ambient_init(lv_obj_t* screen)
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
	lv_obj_set_pos(btn_back, GUISP_AMBIENT_BACK_X, GUISP_AMBIENT_BACK_Y);
	lv_obj_set_size(btn_back, GUISP_AMBIENT_BACK_W, GUISP_AMBIENT_BACK_H);
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
	lv_obj_set_pos(lbl_title, GUISP_AMBIENT_TITLE_X, GUISP_AMBIENT_TITLE_Y);
	lv_label_set_static_text(lbl_title, "Ambient Correction");
	
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
	
	// Add our control panel
	gui_panel_ambient_sensor_comp_init(page_controls);
	gui_panel_ambient_atmospheric_t_init(page_controls);
	gui_panel_ambient_distance_init(page_controls);
	gui_panel_ambient_reflected_t_init(page_controls);
	
	// We start off disabled
	lv_obj_set_hidden(my_page, true);
	
	return my_page;
}


void gui_sub_page_ambient_set_active(bool is_active)
{
	// Set our visibility
	my_page_active = is_active;
	lv_obj_set_hidden(my_page, !is_active);
	
	// Inform controls so they can manage state
	gui_panel_ambient_sensor_comp_set_active(is_active);
	gui_panel_ambient_atmospheric_t_set_active(is_active);
	gui_panel_ambient_distance_set_active(is_active);
	gui_panel_ambient_reflected_t_set_active(is_active);
	
	if (is_active) {
		// Start off with no changes when made visible
		note_change = false;
	}
}


void gui_sub_page_ambient_reset_screen_size(uint16_t page_w, uint16_t page_h)
{
	// Set page dimensions
	lv_obj_set_size(my_page, page_w, page_h);
	
	// Always start off just to the right of the calling page when disabled
	if (!my_page_active) {
		lv_obj_set_pos(my_page, page_w, 0);
	}
	
	// Set title max width
	lv_obj_set_width(lbl_title, page_w - (2 * GUISP_AMBIENT_TITLE_X));
	
	// Set control page dimensions
	lv_obj_set_size(page_controls, page_w, page_h - GUISP_AMBIENT_CONTROL_Y);
	lv_page_set_scrl_width(page_controls, page_w);
}


void gui_sub_page_ambient_register_panel(lv_obj_t* panel)
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


void gui_sub_page_ambient_note_change()
{
	note_change = true;
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
		
			// Send update parameters back to main controller
			if (note_change) {
				(void) _send_ambient_info();
				note_change = false;
			}
		}
	}
}


static bool _send_ambient_info()
{
	uint8_t buf[CMD_AMBIENT_CORRECT_LEN];
	
	// Note: this routine must match order and size of cmd_handler_set_ambient_correct
	// (cmd_handlers.c)
	buf[0] = (uint8_t) gui_state.use_auto_ambient;
	buf[1] = (uint8_t) gui_state.refl_equals_ambient;
	*(uint32_t*)&buf[2] = htonl((uint32_t) gui_state.atmospheric_temp);
	*(uint32_t*)&buf[6] = htonl(gui_state.distance);
	*(uint32_t*)&buf[10] = htonl(gui_state.humidity);
	*(uint32_t*)&buf[14] = htonl((uint32_t) gui_state.reflected_temp);
	
	return cmd_send_binary(CMD_SET, CMD_AMBIENT_CORRECT, CMD_AMBIENT_CORRECT_LEN, buf);
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
