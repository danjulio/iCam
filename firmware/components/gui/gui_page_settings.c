/*
 * GUI settings display page
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
#include "gui_panel_settings_ambient.h"
#include "gui_panel_settings_backlight.h"
#include "gui_panel_settings_brightness.h"
#include "gui_panel_settings_emissivity.h"
#include "gui_panel_settings_gain.h"
#include "gui_panel_settings_info.h"
#include "gui_panel_settings_palette.h"
#include "gui_panel_settings_poweroff.h"
#include "gui_panel_settings_save.h"
#include "gui_panel_settings_shutter.h"
#include "gui_panel_settings_system.h"
#include "gui_panel_settings_time.h"
#include "gui_panel_settings_timelapse.h"
#include "gui_panel_settings_units.h"
#include "gui_panel_settings_wifi.h"
#include "gui_utilities.h"

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
#define MAX_CONTROL_PANELS  15


//
// Local variables
//
static const char* TAG = "gui_page_settings";

// State
static bool is_mobile;
static uint16_t my_width;
static uint16_t my_height;
static uint16_t controls_height = 0;
static int num_control_panels = 0;
static int active_animation_index;

// LVGL objects
static lv_obj_t* my_page;
static lv_obj_t* btn_back;
static lv_obj_t* lbl_btn_back;
static lv_obj_t* lbl_title;
static lv_obj_t* page_controls;
static lv_obj_t* page_controls_scrollable;

static lv_anim_t sub_page_animator;

// Control panels
static lv_obj_t* panel_array[MAX_CONTROL_PANELS];

// Sub-pages associated with individual control panels (entry may be NULL if there is none)
static lv_obj_t* sub_page_array[MAX_CONTROL_PANELS];

// Open sub-page index (when > 0) - we use this to close it if we're navigated away from by
// another source (e.g. loss of client connection)
static int open_sub_page_index = -1;


// Sub-page visible enable function array
static sub_page_activation_handler sub_page_activation_array[MAX_CONTROL_PANELS];

// Sub-page resize function array
static sub_page_resize_handler sub_page_resize_array[MAX_CONTROL_PANELS];



//
// Forward declarations for internal routines
//
static void _init_panel_arrays();
static void _calculate_width(uint16_t page_w);
static void _calculate_height(uint16_t page_h);
static void _layout_page();
static void _cb_back_button(lv_obj_t* obj, lv_event_t event);
static void _cb_sub_page_open_done(struct _lv_anim_t * a);
static void _cb_sub_page_close_done(struct _lv_anim_t * a);



//
// API
//
lv_obj_t* gui_page_settings_init(lv_obj_t* screen, uint16_t page_w, uint16_t page_h, bool mobile)
{
	is_mobile = mobile;
	
	// Initialize the arrays we use to manage panels and sub-pages
	_init_panel_arrays();

	// Create the top-level container for the page
	my_page = lv_obj_create(screen, NULL);
	lv_obj_set_pos(my_page, 0, 0);
	lv_obj_set_click(my_page, false);
		
	// Setup the page width
	_calculate_width(page_w);
	
	// Add the top-level controls
	//
	// Back button (fixed location)
	btn_back = lv_btn_create(my_page, NULL);
	lv_obj_set_pos(btn_back, GUIP_SETTINGS_BACK_X, GUIP_SETTINGS_BACK_Y);
	lv_obj_set_size(btn_back, GUIP_SETTINGS_BACK_W, GUIP_SETTINGS_BACK_H);
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
	lv_obj_set_pos(lbl_title, GUIP_SETTINGS_TITLE_X, GUIP_SETTINGS_TITLE_Y);
	lv_label_set_static_text(lbl_title, "Settings");
	
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

	// Add panels to the control page (they will register themselves with us)
	gui_panel_settings_info_init(screen, page_controls);
	gui_panel_settings_ambient_init(screen, page_controls);
#ifdef ESP_PLATFORM
	gui_panel_settings_backlight_init(page_controls);
#endif
	gui_panel_settings_brightness_init(page_controls);
	gui_panel_settings_emissivity_init(page_controls);
	gui_panel_settings_gain_init(page_controls);
	gui_panel_settings_palette_init(page_controls);
	gui_panel_settings_poweroff_init(page_controls);
	gui_panel_settings_save_init(page_controls);
	gui_panel_settings_shutter_init(screen, page_controls);
	gui_panel_settings_system_init(screen, page_controls);
	gui_panel_settings_time_init(screen, page_controls);
	gui_panel_settings_timelapse_init(screen, page_controls);
	gui_panel_settings_units_init(page_controls);
#ifndef ESP_PLATFORM
	gui_panel_settings_wifi_init(screen, page_controls);
#endif
	
	// Setup the page height after adding all content
	_calculate_height(page_h);
	
	// Setup the page layout
	_layout_page();
	
	return my_page;
}


void gui_page_settings_set_active(bool is_active)
{
	// Set page visibility
	lv_obj_set_hidden(my_page, !is_active);
	
	// Close any open sub-page if we're being closed
	if (open_sub_page_index >= 0) {
		sub_page_activation_array[open_sub_page_index](false);
	}
	
	// Inform controls so they can manage state
	gui_panel_settings_info_set_active(is_active);
	gui_panel_settings_ambient_set_active(is_active);
#ifdef ESP_PLATFORM
	gui_panel_settings_backlight_set_active(is_active);
#endif
	gui_panel_settings_brightness_set_active(is_active);
	gui_panel_settings_emissivity_set_active(is_active);
	gui_panel_settings_gain_set_active(is_active);
	gui_panel_settings_palette_set_active(is_active);
	gui_panel_settings_poweroff_set_active(is_active);
	gui_panel_settings_save_set_active(is_active);
	gui_panel_settings_shutter_set_active(is_active);
	gui_panel_settings_system_set_active(is_active);
	gui_panel_settings_time_set_active(is_active);
	gui_panel_settings_timelapse_set_active(is_active);
	gui_panel_settings_units_set_active(is_active);
#ifndef ESP_PLATFORM
	gui_panel_settings_wifi_set_active(is_active);
#endif
}


void gui_page_settings_reset_screen_size(uint16_t page_w, uint16_t page_h)
{
	// Reset the width and height if necessary
	_calculate_width(page_w);
	_calculate_height(page_h);
	
	// Reset the page layout
	_layout_page();
}


void gui_page_settings_register_panel(lv_obj_t* panel, lv_obj_t* sub_page, sub_page_activation_handler activate_func, sub_page_resize_handler resize_func)
{
	if (num_control_panels < MAX_CONTROL_PANELS) {
		panel_array[num_control_panels] = panel;
		sub_page_array[num_control_panels] = sub_page;
		sub_page_activation_array[num_control_panels] = activate_func;
		sub_page_resize_array[num_control_panels] = resize_func;
		
		controls_height += lv_obj_get_height(panel) - 2*GUIP_SETTINGS_INNER_PAD;
		num_control_panels += 1;
	} else {
#ifdef ESP_PLATFORM
		ESP_LOGE(TAG, "Add control panel failed");
#else
		printf("%s Add control panel failed\n", TAG);
#endif
	}
}


void gui_page_settings_open_sub_page(lv_obj_t* sub_page)
{
	int n = 0;
	
	// Find the sub-page entry index
	do {
		if (sub_page_array[n] == sub_page) break;
	} while (++n < num_control_panels);
	
	// Start an animation bringing the sub-page into view
	if (n < num_control_panels) {
		active_animation_index = n;
		
		// First, activate the sub-page
		sub_page_activation_array[n](true);
		
		// Start our animator to translate its X location
		lv_anim_init(&sub_page_animator);
		lv_anim_set_exec_cb(&sub_page_animator, (lv_anim_exec_xcb_t) lv_obj_set_x); 
		lv_anim_set_var(&sub_page_animator, sub_page_array[n]);
		lv_anim_set_time(&sub_page_animator, GUIP_SETTINGS_SUB_PG_MSEC);
		lv_anim_set_values(&sub_page_animator, my_width, 0);
		lv_anim_set_ready_cb(&sub_page_animator, _cb_sub_page_open_done);
		lv_anim_start(&sub_page_animator);
		
		// Save this sub-page away
		open_sub_page_index = n;
	}
}


void gui_page_settings_close_sub_page(lv_obj_t* sub_page)
{
	int n = 0;
	
	// Find the sub-page entry index
	do {
		if (sub_page_array[n] == sub_page) break;
	} while (++n < num_control_panels);
	
	// Start an animation moving the sub-page out of view
	if (n < MAX_CONTROL_PANELS) {
		active_animation_index = n;
		
		// Start our animator to translate its X location
		lv_anim_init(&sub_page_animator);
		lv_anim_set_exec_cb(&sub_page_animator, (lv_anim_exec_xcb_t) lv_obj_set_x); 
		lv_anim_set_var(&sub_page_animator, sub_page_array[n]);
		lv_anim_set_time(&sub_page_animator, GUIP_SETTINGS_SUB_PG_MSEC);
		lv_anim_set_values(&sub_page_animator, 0, my_width);
		lv_anim_set_ready_cb(&sub_page_animator, _cb_sub_page_close_done);
		lv_anim_start(&sub_page_animator);
		
		// Note we have no open sub-pages
		open_sub_page_index = -1;
	}
}



//
// Internal routines
//
static void _init_panel_arrays()
{
	for (int i=0; i<MAX_CONTROL_PANELS; i++) {
		panel_array[i] = NULL;
		sub_page_array[i] = NULL;
		sub_page_activation_array[i] = NULL;
		sub_page_resize_array[i] = NULL;
	}
}


static void _calculate_width(uint16_t page_w)
{
	if (page_w >= GUIP_SETTINGS_MAX_WIDTH) {
		my_width = GUIP_SETTINGS_MAX_WIDTH;
	} else if (page_w < GUIP_SETTINGS_MIN_WIDTH) {
		my_width = GUIP_SETTINGS_MIN_WIDTH;
	} else {
		my_width = page_w;
	}
}


static void _calculate_height(uint16_t page_h)
{
	// Start with ideal height (all controls visible)
	my_height = GUIP_SETTINGS_CONTROL_Y + controls_height;
	
	// Limit to visible region if necessary
	if (my_height > page_h) {
		my_height = page_h;
	}
}


static void _layout_page()
{
	// Set page dimensions
	lv_obj_set_size(my_page, my_width, my_height);
	
	// Set title max width
	lv_obj_set_width(lbl_title, my_width - (2 * GUIP_SETTINGS_TITLE_X));
	
	// Set control page dimensions
	lv_obj_set_size(page_controls, my_width, my_height - GUIP_SETTINGS_CONTROL_Y - (GUIP_SETTINGS_TOP_PAD + GUIP_SETTINGS_BTM_PAD));
	lv_page_set_scrl_width(page_controls, my_width);
	
	// (re)set the layout of all registered sub-pages
	for (int i=0; i<MAX_CONTROL_PANELS; i++) {
		if (sub_page_resize_array[i] != NULL) {
			sub_page_resize_array[i](my_width, my_height);
		}
	}
}


static void _cb_back_button(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		if (!gui_popup_displayed()) {
			gui_main_set_page(GUI_MAIN_PAGE_IMAGE);
		}
	}
}


static void _cb_sub_page_open_done(struct _lv_anim_t * a)
{
	// Delete the animation
	lv_anim_del(&sub_page_animator, NULL);
}


static void _cb_sub_page_close_done(struct _lv_anim_t * a)
{
	// Disable the page after the animation moves it away
	sub_page_activation_array[active_animation_index](false);
	
	// Delete the animation
	lv_anim_del(&sub_page_animator, NULL);
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
