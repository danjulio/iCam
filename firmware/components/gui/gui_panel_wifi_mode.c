/*
 * GUI wifi settings STA/AP mode select control panel
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
#include "gui_panel_wifi_mode.h"
#include "gui_state.h"

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
static lv_obj_t* sw_assy;
static lv_obj_t* sw_mode;
static lv_obj_t* lbl_ap;
static lv_obj_t* lbl_sta;



//
// Forward declarations for internal functions
//
static void _cb_sw_mode(lv_obj_t* obj, lv_event_t event);



//
// API
//
void gui_panel_wifi_mode_init(lv_obj_t* parent_cont)
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
	lv_label_set_static_text(lbl_name, "Wi-Fi Mode");
	
	// Switch assembly (labels + switch so my_panel container spaces it correctly)
	sw_assy = lv_obj_create(my_panel, NULL);
	lv_obj_set_click(sw_assy, false);
	lv_obj_set_height(sw_assy, GUIPN_WIFI_MODE_SW_H + 10);
	lv_obj_set_width(sw_assy, 2*GUIPN_WIFI_MODE_TYP_W + GUIPN_WIFI_MODE_SW_W);
	lv_obj_set_style_local_border_width(sw_assy, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
		
	// Unit selection switch
	sw_mode = lv_switch_create(sw_assy, NULL);
	lv_obj_align(sw_mode, sw_assy, LV_ALIGN_CENTER, 0, 0);
	lv_obj_add_protect(sw_mode, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_size(sw_mode, GUIPN_WIFI_MODE_SW_W, GUIPN_WIFI_MODE_SW_H);
    lv_obj_set_style_local_bg_color(sw_mode, LV_SWITCH_PART_BG, LV_STATE_DEFAULT, GUI_THEME_SLD_BG_COLOR);
	lv_obj_set_style_local_bg_color(sw_mode, LV_SWITCH_PART_INDIC, LV_STATE_DEFAULT, GUI_THEME_SLD_BG_COLOR);
	lv_obj_set_event_cb(sw_mode, _cb_sw_mode);
	
	// Add "AP" to the left
	lbl_ap = lv_label_create(sw_assy, NULL);
	lv_obj_set_width(lbl_ap, GUIPN_WIFI_MODE_TYP_W);
	lv_obj_align(lbl_ap, sw_mode, LV_ALIGN_OUT_LEFT_MID, 5, 0);
	lv_label_set_static_text(lbl_ap, "AP");

	// Add "STA" to the right
	lbl_sta = lv_label_create(sw_assy, NULL);
	lv_obj_set_width(lbl_sta, GUIPN_WIFI_MODE_TYP_W);
	lv_obj_align(lbl_sta, sw_mode, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
	lv_label_set_static_text(lbl_sta, "STA");
	
    // Register with our parent page
	gui_sub_page_wifi_register_panel(my_panel);
}


void gui_panel_wifi_mode_set_active(bool is_active)
{
	if (is_active) {
		if (gui_state.sta_mode) {
			lv_switch_on(sw_mode, false);
		} else {
			lv_switch_off(sw_mode, false);
		}
	}
}



//
// Internal functions
//
static void _cb_sw_mode(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		gui_state.sta_mode = lv_switch_get_state(obj);
		
		// Let our parent know so it can inform the other wifi panels of the change
		gui_sub_page_wifi_note_change(true);
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
