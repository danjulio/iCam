/*
 * GUI settings system information control panel
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
#include "gui_sub_page_info.h"
#include "gui_panel_settings_info.h"
#include "gui_state.h"
#include "gui_utilities.h"

#ifdef ESP_PLATFORM
	#include "gui_task.h"
#else
	#include "gui_main.h"
#endif



//
// Local variables
//

// State
static bool prev_active = false;

//
// LVGL Objects
//
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* btn_open_sub_page;
static lv_obj_t* lbl_btn_open_sub_page;
static lv_obj_t* my_sub_page;



//
// Forward declarations for internal functions
//
static void _cb_btn_open_sub_page(lv_obj_t* obj, lv_event_t event);



//
// API
//
void gui_panel_settings_info_init(lv_obj_t* screen, lv_obj_t* parent_cont)
{
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
	lv_label_set_static_text(lbl_name, "About");
	
	// Sub-page open button
	btn_open_sub_page = lv_btn_create(my_panel, NULL);
	lv_obj_set_size(btn_open_sub_page, GUIPN_SETTINGS_INFO_BTN_W, GUIPN_SETTINGS_INFO_BTN_H);
	lv_obj_add_protect(btn_open_sub_page, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_border_width(btn_open_sub_page, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_border_width(btn_open_sub_page, LV_BTN_PART_MAIN, LV_STATE_PRESSED, 0);
	lv_obj_set_event_cb(btn_open_sub_page, _cb_btn_open_sub_page);
	
	lbl_btn_open_sub_page = lv_label_create(btn_open_sub_page, NULL);
	lv_obj_set_style_local_text_font(lbl_btn_open_sub_page, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
	lv_label_set_long_mode(lbl_btn_open_sub_page, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_btn_open_sub_page, LV_LABEL_ALIGN_RIGHT);
	lv_label_set_static_text(lbl_btn_open_sub_page, LV_SYMBOL_RIGHT " ");
	
	// Create our sub-page
	my_sub_page = gui_sub_page_info_init(screen);

	// Register with our parent page
	gui_page_settings_register_panel(my_panel, my_sub_page, gui_sub_page_info_set_active, gui_sub_page_info_reset_screen_size);
}


void gui_panel_settings_info_set_active(bool is_active)
{
	// We don't need to handle any state
	prev_active = is_active;
}



//
// Internal functions
//
static void _cb_btn_open_sub_page(lv_obj_t* obj, lv_event_t event)
{
	if ((event == LV_EVENT_CLICKED) && !gui_popup_displayed()) {
		// Start the display of our sub-page
		gui_page_settings_open_sub_page(my_sub_page);
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
