/*
 * GUI wifi settings mDNS enable control panel
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
#include "gui_sub_page_wifi.h"
#include "gui_panel_wifi_mdns_en.h"
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

//
// LVGL Objects
//
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* sw_enable;



//
// Forward declarations for internal functions
//
static void _cb_sw_enable(lv_obj_t* obj, lv_event_t event);



//
// API
//
void gui_panel_wifi_mdns_en_init(lv_obj_t* parent_cont)
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
	lv_label_set_static_text(lbl_name, "mDNS Discovery Enable");
		
	// Enable switch
	sw_enable = lv_switch_create(my_panel, NULL);
	lv_obj_add_protect(sw_enable, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_size(sw_enable, GUIPN_WIFI_MDNS_EN_SW_W, GUIPN_WIFI_MDNS_EN_SW_H);
    lv_obj_set_style_local_bg_color(sw_enable, LV_SWITCH_PART_BG, LV_STATE_DEFAULT, GUI_THEME_SLD_BG_COLOR);
	lv_obj_set_style_local_bg_color(sw_enable, LV_SWITCH_PART_INDIC, LV_STATE_DEFAULT, GUI_THEME_SLD_BG_COLOR);
	lv_obj_set_event_cb(sw_enable, _cb_sw_enable);
	
    // Register with our parent page
	gui_sub_page_wifi_register_panel(my_panel);
}


void gui_panel_wifi_mdns_en_set_active(bool is_active)
{
	if (is_active) {
		// Get the current switch value
		if (gui_state.mdns_en) {
			lv_switch_on(sw_enable, false);
		} else {
			lv_switch_off(sw_enable, false);
		}
	}
}



//
// Internal functions
//
static void _cb_sw_enable(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		gui_state.mdns_en = lv_switch_get_state(obj);
		
		// Note change
		gui_sub_page_wifi_note_change(false);
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
