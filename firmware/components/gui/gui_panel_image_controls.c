/*
 * GUI Live image control panel
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
#include "gui_panel_image_main.h"
#include "gui_panel_image_controls.h"
#include "gui_state.h"
#include <arpa/inet.h>

#ifdef ESP_PLATFORM
	#include "gui_task.h"
#else
	#include "gui_main.h"
#endif



//
// Local variables
//

// State
static bool imgc_portrait;
static uint16_t imgc_w, imgc_h;


//
// LVGL Objects
//
static lv_obj_t* my_panel;

static lv_obj_t* cont_markers;
static lv_obj_t* btn_minmax_en;
static lv_obj_t* lbl_btn_minmax_en;
static lv_obj_t* btn_spot_en;
static lv_obj_t* lbl_btn_spot_en;
static lv_obj_t* btn_area_en;
static lv_obj_t* lbl_btn_area_en;

static lv_obj_t* btn_take_pic;
static lv_obj_t* lbl_btn_take_pic;

static lv_obj_t* cont_page_sel;
static lv_obj_t* btn_settings;
static lv_obj_t* lbl_btn_settings;
static lv_obj_t* btn_files;
static lv_obj_t* lbl_btn_files;

// Timer tasks
static lv_task_t* task_minmax_upd_timer;
static lv_task_t* task_spot_upd_timer;



//
// Forward declarations for internal functions
//
static void _draw_take_pic_label(bool en_timelapse);
static void _configure_sizes();
static void _cb_btn_minmax_en(lv_obj_t* btn, lv_event_t event);
static void _cb_btn_spot_en(lv_obj_t* btn, lv_event_t event);
static void _cb_btn_region_en(lv_obj_t* btn, lv_event_t event);
static void _cb_btn_take_pic(lv_obj_t* btn, lv_event_t event);
static void _cb_btn_set_page(lv_obj_t* btn, lv_event_t event);

static void _task_eval_minmax_upd_timer(lv_task_t* task);
static void _task_eval_spot_upd_timer(lv_task_t* task);



//
// API
//
void gui_panel_image_controls_calculate_size(uint16_t major_dim, uint16_t* minor_dim, bool is_portrait)
{
	if (is_portrait) {
		imgc_w = major_dim;
		imgc_h = GUIPN_IMAGEC_BTN_H + 2 * GUIPN_IMAGEC_BTN_SPACING;
		*minor_dim = imgc_h;
	} else {
		imgc_w = GUIPN_IMAGEC_BTN_W + 2 * GUIPN_IMAGEC_BTN_SPACING;
		imgc_h = major_dim;
		*minor_dim = imgc_w;
	}
	imgc_portrait = is_portrait;
}


lv_obj_t* gui_panel_image_controls_init(lv_obj_t* page)
{
	// Panel
	my_panel = lv_cont_create(page, NULL);
	lv_obj_set_click(my_panel, false);
	lv_cont_set_fit(my_panel, LV_FIT_NONE);
	lv_obj_set_style_local_pad_top(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	lv_obj_set_style_local_pad_bottom(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	lv_obj_set_style_local_pad_left(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	lv_obj_set_style_local_pad_right(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	
	// Create a background color for the button containers
	lv_color_t bg_color = lv_obj_get_style_bg_color(my_panel, LV_STATE_DEFAULT);
	bg_color = lv_color_darken(bg_color, LV_OPA_20);
	
	// Create a border color for the buttons
	lv_color_t border_color = lv_obj_get_style_bg_color(my_panel, LV_STATE_DEFAULT);
	border_color = lv_color_lighten(border_color, LV_OPA_30);
	
	// Marker controls (position of all buttons and containers set in _configure_sizes())
	//
	// Container
	cont_markers = lv_cont_create(my_panel, NULL);
	lv_cont_set_fit(cont_markers, LV_FIT_NONE);
	lv_obj_set_style_local_bg_color(cont_markers, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, bg_color);
	lv_obj_set_click(cont_markers, false);
	lv_obj_set_style_local_pad_inner(cont_markers, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING);
	lv_obj_set_style_local_pad_top(cont_markers, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	lv_obj_set_style_local_pad_bottom(cont_markers, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	lv_obj_set_style_local_pad_left(cont_markers, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	lv_obj_set_style_local_pad_right(cont_markers, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	
	// Min/Max Marker Enable button
	btn_minmax_en = lv_btn_create(cont_markers, NULL);
	lv_obj_set_size(btn_minmax_en, GUIPN_IMAGEC_BTN_W, GUIPN_IMAGEC_BTN_H);
	lv_obj_set_style_local_border_color(btn_minmax_en, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, border_color);
	lv_obj_add_protect(btn_minmax_en, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_event_cb(btn_minmax_en, _cb_btn_minmax_en);
	lbl_btn_minmax_en = lv_label_create(btn_minmax_en, NULL);
	lv_label_set_static_text(lbl_btn_minmax_en, LV_SYMBOL_UP LV_SYMBOL_DOWN);
	
	// Spot Marker Enable button
	btn_spot_en = lv_btn_create(cont_markers, NULL);
	lv_obj_set_size(btn_spot_en, GUIPN_IMAGEC_BTN_W, GUIPN_IMAGEC_BTN_H);
	lv_obj_set_style_local_border_color(btn_spot_en, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, border_color);
	lv_obj_add_protect(btn_spot_en, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_event_cb(btn_spot_en, _cb_btn_spot_en);
	lbl_btn_spot_en = lv_label_create(btn_spot_en, NULL);
	lv_label_set_static_text(lbl_btn_spot_en, "O");
	
	// Area Marker Enable button
	btn_area_en = lv_btn_create(cont_markers, NULL);
	lv_obj_set_size(btn_area_en, GUIPN_IMAGEC_BTN_W, GUIPN_IMAGEC_BTN_H);
	lv_obj_set_style_local_border_color(btn_area_en, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, border_color);
	lv_obj_add_protect(btn_area_en, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_event_cb(btn_area_en, _cb_btn_region_en);
	lbl_btn_area_en = lv_label_create(btn_area_en, NULL);
	lv_label_set_static_text(lbl_btn_area_en, LV_SYMBOL_LOOP);
	
	// Take Picture button
	btn_take_pic = lv_btn_create(my_panel, NULL);
	lv_obj_set_size(btn_take_pic, GUIPN_IMAGEC_BTN_W, GUIPN_IMAGEC_BTN_H);
	lv_obj_set_style_local_border_color(btn_take_pic, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, border_color);
	lv_obj_add_protect(btn_take_pic, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_event_cb(btn_take_pic, _cb_btn_take_pic);
	lbl_btn_take_pic = lv_label_create(btn_take_pic, NULL);
	lv_label_set_recolor(lbl_btn_take_pic, true);                   // Enable recolor for timelapse indication
	_draw_take_pic_label(gui_state.timelapse_running);
	
	// Page Navigation controls
	//
	// Container
	cont_page_sel = lv_cont_create(my_panel, NULL);
	lv_cont_set_fit(cont_page_sel, LV_FIT_NONE);
	lv_obj_set_style_local_bg_color(cont_page_sel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, bg_color);
	lv_obj_set_click(cont_page_sel, false);
	lv_obj_set_style_local_pad_inner(cont_page_sel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING);
	lv_obj_set_style_local_pad_top(cont_page_sel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	lv_obj_set_style_local_pad_bottom(cont_page_sel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	lv_obj_set_style_local_pad_left(cont_page_sel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	lv_obj_set_style_local_pad_right(cont_page_sel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIPN_IMAGEC_BTN_SPACING/2);
	
	// Settings Page button
	btn_settings = lv_btn_create(cont_page_sel, NULL);
	lv_obj_set_size(btn_settings, GUIPN_IMAGEC_BTN_W, GUIPN_IMAGEC_BTN_H);
	lv_obj_set_style_local_border_color(btn_settings, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, border_color);
	lv_obj_add_protect(btn_settings, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_event_cb(btn_settings, _cb_btn_set_page);
	lbl_btn_settings = lv_label_create(btn_settings, NULL);
	lv_label_set_static_text(lbl_btn_settings, LV_SYMBOL_SETTINGS);
	
	// File Page button
	btn_files = lv_btn_create(cont_page_sel, NULL);
	lv_obj_set_size(btn_files, GUIPN_IMAGEC_BTN_W, GUIPN_IMAGEC_BTN_H);
	lv_obj_set_style_local_border_color(btn_files, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, border_color);
	lv_obj_add_protect(btn_files, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_event_cb(btn_files, _cb_btn_set_page);
	lbl_btn_files = lv_label_create(btn_files, NULL);
	lv_label_set_static_text(lbl_btn_files, LV_SYMBOL_FILE);
	
	// Setup dimensions of objects we computed in gui_panel_image_controls_calculate_size()
	_configure_sizes();
	
	return my_panel;
}


void gui_panel_image_controls_reset_size()
{
	// Setup dimensions of objects we computed in gui_panel_image_controls_calculate_size()
	_configure_sizes();
}


void gui_panel_image_controls_set_active(bool is_active)
{
}


void gui_panel_image_controls_set_timelapse(bool en)
{
	_draw_take_pic_label(en);
}


//
// Internal functions
//
static void _draw_take_pic_label(bool en_timelapse)
{
	if (en_timelapse) {
		// Set label red to indicate timelapse running
		lv_label_set_static_text(lbl_btn_take_pic, "#ff0000 " LV_SYMBOL_IMAGE "#");
	} else {
		lv_label_set_static_text(lbl_btn_take_pic, "#ffffff " LV_SYMBOL_IMAGE "#");
	}
}


static void _configure_sizes()
{
	uint16_t d;
	
	// Configure the size of the panel
	lv_obj_set_size(my_panel, imgc_w, imgc_h);
	
	if (imgc_portrait) {
		// Align marker buttons horizontally in their container
		lv_cont_set_layout(cont_markers, LV_LAYOUT_PRETTY_MID);
		lv_obj_set_size(cont_markers, 3*GUIPN_IMAGEC_BTN_W + 3*GUIPN_IMAGEC_BTN_SPACING, GUIPN_IMAGEC_BTN_H + GUIPN_IMAGEC_BTN_SPACING);
		
		// Align page buttons horizontally in their container
		lv_cont_set_layout(cont_page_sel, LV_LAYOUT_PRETTY_MID);
		lv_obj_set_size(cont_page_sel, 2*GUIPN_IMAGEC_BTN_W + 2*GUIPN_IMAGEC_BTN_SPACING, GUIPN_IMAGEC_BTN_H + GUIPN_IMAGEC_BTN_SPACING);
		
		// Align top-level objects horizontally
		lv_cont_set_layout(my_panel, LV_LAYOUT_PRETTY_MID);
		d = lv_obj_get_width(my_panel);
		d -= (lv_obj_get_width(cont_markers) + lv_obj_get_width(btn_take_pic)
		      + lv_obj_get_width(cont_page_sel) + GUIPN_IMAGEC_BTN_SPACING);		
		lv_obj_set_style_local_pad_inner(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, d/2);
	} else {
		// Align marker buttons vertically in their container
		lv_cont_set_layout(cont_markers, LV_LAYOUT_COLUMN_MID);
		lv_obj_set_size(cont_markers, GUIPN_IMAGEC_BTN_W + GUIPN_IMAGEC_BTN_SPACING, 3*GUIPN_IMAGEC_BTN_H + 3*GUIPN_IMAGEC_BTN_SPACING);
		
		// Align page buttons vertically in their container
		lv_cont_set_layout(cont_page_sel, LV_LAYOUT_COLUMN_MID);
		lv_obj_set_size(cont_page_sel, GUIPN_IMAGEC_BTN_W + GUIPN_IMAGEC_BTN_SPACING, 2*GUIPN_IMAGEC_BTN_H + 2*GUIPN_IMAGEC_BTN_SPACING);
		
		// Align top-level objects vertically
		lv_cont_set_layout(my_panel, LV_LAYOUT_COLUMN_MID);
		d = lv_obj_get_height(my_panel);
		d -= (lv_obj_get_height(cont_markers) + lv_obj_get_height(btn_take_pic)
		      + lv_obj_get_height(cont_page_sel) + GUIPN_IMAGEC_BTN_SPACING);
		lv_obj_set_style_local_pad_inner(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, d/2);
	}
}


static void _cb_btn_minmax_en(lv_obj_t* btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		gui_state.min_max_mrk_enable = !gui_state.min_max_mrk_enable;
		if (gui_state.min_max_mrk_enable) {
			gui_panel_image_set_message("Min/Max Marker On", GUIPN_IMAGEC_MARKER_MSG_MSEC);
		} else {
			gui_panel_image_set_message("Min/Max Marker Off", GUIPN_IMAGEC_MARKER_MSG_MSEC);
		}
				
		// Start or update a timer to update NVS after last change
		if (task_minmax_upd_timer == NULL) {
			// Start the timer
			task_minmax_upd_timer = lv_task_create(_task_eval_minmax_upd_timer, GUIPN_IMAGEC_MINMAX_UPD_MSEC, LV_TASK_PRIO_LOW, NULL);
		} else {
			// Reset timer
			lv_task_reset(task_minmax_upd_timer);
		}
	}
}


static void _cb_btn_spot_en(lv_obj_t* btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		gui_state.spotmeter_enable = !gui_state.spotmeter_enable;
		if (gui_state.spotmeter_enable) {
			gui_panel_image_set_message("Spot Marker On", GUIPN_IMAGEC_MARKER_MSG_MSEC);
		} else {
			gui_panel_image_set_message("Spot Marker Off", GUIPN_IMAGEC_MARKER_MSG_MSEC);
		}
				
		// Start or update a timer to update NVS after last change
		if (task_spot_upd_timer == NULL) {
			// Start the timer
			task_spot_upd_timer = lv_task_create(_task_eval_spot_upd_timer, GUIPN_IMAGEC_SPOT_UPD_MSEC, LV_TASK_PRIO_LOW, NULL);
		} else {
			// Reset timer
			lv_task_reset(task_spot_upd_timer);
		}
	}
}


static void _cb_btn_region_en(lv_obj_t* btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		if (!gui_state.region_enable) {
			if (gui_panel_image_region_selection_in_progress()) {
				// User ended region selection before it was complete
				gui_panel_image_enable_region_selection(false);
			} else {
				// Start region selection
				gui_panel_image_enable_region_selection(true);
			}
		} else {
			// Disable region marker display
			gui_state.region_enable = false;
			gui_panel_image_set_message("Region Marker Off", GUIPN_IMAGEC_MARKER_MSG_MSEC);
			(void) cmd_send_int32(CMD_SET, CMD_REGION_EN, (int32_t) gui_state.region_enable);
		}
	}
}


static void _cb_btn_take_pic(lv_obj_t* btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		// Take picture or start timelapse
		(void) cmd_send(CMD_SET, CMD_TAKE_PICTURE);
	}
}


static void _cb_btn_set_page(lv_obj_t* btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		if (btn == btn_settings) {
			gui_main_set_page(GUI_MAIN_PAGE_SETTINGS);
		} else if (btn == btn_files) {
			gui_main_set_page(GUI_MAIN_PAGE_LIBRARY);
		}
	}
}


static void _task_eval_minmax_upd_timer(lv_task_t* task)
{
	// Save the current minmax enable to NVS
	(void) cmd_send_int32(CMD_SET, CMD_MIN_MAX_EN, (int32_t) gui_state.min_max_mrk_enable);
	
	// Terminate the timer
	lv_task_del(task_minmax_upd_timer);
	task_minmax_upd_timer = NULL;
}


static void _task_eval_spot_upd_timer(lv_task_t* task)
{
	// Save the current minmax enable to NVS
	(void) cmd_send_int32(CMD_SET, CMD_SPOT_EN, (int32_t) gui_state.spotmeter_enable);
	
	// Terminate the timer
	lv_task_del(task_spot_upd_timer);
	task_spot_upd_timer = NULL;
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
