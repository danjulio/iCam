/*
 * GUI settings gCore LCD backlight brightness control panel
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

#ifdef ESP_PLATFORM
#include "esp_system.h"
#include "cmd_utilities.h"
#include "gui_page_settings.h"
#include "gui_panel_settings_backlight.h"
#include "gui_state.h"
#include "gui_task.h"


//
// Local variables
//

// State
static bool prev_active = false;
static int cur_percent;

//
// LVGL Objects
//
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* sld_brightness;



//
// Forward declarations for internal functions
//
static void _cb_sld_brightness(lv_obj_t* obj, lv_event_t event);



void gui_panel_settings_backlight_init(lv_obj_t* parent_cont)
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
	
	lbl_name = lv_label_create(my_panel, NULL);
	lv_label_set_static_text(lbl_name, "Backlight");
	
	sld_brightness = lv_slider_create(my_panel, NULL);
	lv_obj_add_protect(sld_brightness, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_width(sld_brightness, GUIPN_SETTINGS_BACKLIGHT_SLD_W);
	lv_obj_set_style_local_bg_color(sld_brightness, LV_SLIDER_PART_BG, LV_STATE_DEFAULT, GUI_THEME_SLD_BG_COLOR);
	lv_obj_set_style_local_bg_color(sld_brightness, LV_SLIDER_PART_INDIC, LV_STATE_DEFAULT, GUI_THEME_SLD_BG_COLOR);
	lv_obj_set_event_cb(sld_brightness, _cb_sld_brightness);
    lv_slider_set_range(sld_brightness, 1, 100);
    
    // Register with our parent page
	gui_page_settings_register_panel(my_panel, NULL, NULL, NULL);
}


void gui_panel_settings_backlight_set_active(bool is_active)
{
	if (is_active) {
		// Get the current brightness
		cur_percent = (int) gui_state.lcd_brightness;
		if (cur_percent < 1) cur_percent = 1;
		if (cur_percent > 100) cur_percent = 100;
		lv_slider_set_value(sld_brightness, (int16_t) cur_percent, LV_ANIM_OFF);
	} else {
		if (prev_active) {
			// Update lcd_brightness in NVS if there was a change
			if (cur_percent != (int) gui_state.lcd_brightness) {
				gui_state.lcd_brightness = (uint32_t) cur_percent;
				(void) cmd_send_int32(CMD_SET, CMD_SAVE_BACKLIGHT, (int32_t) cur_percent);
			}
		}
	}
	
	prev_active = is_active;
}



//
// Internal functions
//
static void _cb_sld_brightness(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		cur_percent = (int) lv_slider_get_value(obj);
		
		// Real-time update
		(void) cmd_send_int32(CMD_SET, CMD_BACKLIGHT, (int32_t) cur_percent);
	}
}

#endif /* ESP_PLATFORM */

#endif /* !CONFIG_BUILD_ICAM_MINI */
