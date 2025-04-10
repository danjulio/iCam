/*
 * GUI settings units type control panel
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
#include "gui_panel_settings_units.h"
#include "gui_state.h"

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
static bool init_temp_unit_C;

//
// LVGL Objects
//
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* sw_assy;
static lv_obj_t* sw_units;
static lv_obj_t* lbl_c;
static lv_obj_t* lbl_f;



//
// Forward declarations for internal functions
//
static void _cb_sw_units(lv_obj_t* obj, lv_event_t event);



//
// API
//
void gui_panel_settings_units_init(lv_obj_t* parent_cont)
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
	lv_label_set_static_text(lbl_name, "Units");
	
	// Switch assembly (labels + switch so my_panel container spaces it correctly)
	sw_assy = lv_obj_create(my_panel, NULL);
	lv_obj_set_click(sw_assy, false);
	lv_obj_set_height(sw_assy, GUIPN_SETTINGS_UNITS_SW_H + 10);
	lv_obj_set_width(sw_assy, 2*GUIPN_SETTINGS_UNITS_TYP_W + GUIPN_SETTINGS_UNITS_SW_W);
	lv_obj_set_style_local_border_width(sw_assy, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
		
	// Unit selection switch
	sw_units = lv_switch_create(sw_assy, NULL);
	lv_obj_align(sw_units, sw_assy, LV_ALIGN_CENTER, 0, 0);
	lv_obj_add_protect(sw_units, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_size(sw_units, GUIPN_SETTINGS_UNITS_SW_W, GUIPN_SETTINGS_UNITS_SW_H);
    lv_obj_set_style_local_bg_color(sw_units, LV_SWITCH_PART_BG, LV_STATE_DEFAULT, GUI_THEME_SLD_BG_COLOR);
	lv_obj_set_style_local_bg_color(sw_units, LV_SWITCH_PART_INDIC, LV_STATE_DEFAULT, GUI_THEME_SLD_BG_COLOR);
	lv_obj_set_event_cb(sw_units, _cb_sw_units);

	// Add "°F" to the left
	lbl_f = lv_label_create(sw_assy, NULL);
	lv_obj_set_width(lbl_f, GUIPN_SETTINGS_UNITS_TYP_W);
	lv_obj_align(lbl_f, sw_units, LV_ALIGN_OUT_LEFT_MID, 10, 0);
	lv_label_set_static_text(lbl_f, "°F");
	
	// Add "°C" to the right
	lbl_c = lv_label_create(sw_assy, NULL);
	lv_obj_set_width(lbl_c, GUIPN_SETTINGS_UNITS_TYP_W);
	lv_obj_align(lbl_c, sw_units, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
	lv_label_set_static_text(lbl_c, "°C");
	
    // Register with our parent page
	gui_page_settings_register_panel(my_panel, NULL, NULL, NULL);
}


void gui_panel_settings_units_set_active(bool is_active)
{
	if (is_active) {
		init_temp_unit_C = gui_state.temp_unit_C;
		if (init_temp_unit_C) {
			lv_switch_on(sw_units, false);
		} else {
			lv_switch_off(sw_units, false);
		}
	} else {
		if (prev_active) {
			// Update the controller units flag if there was a change
			if (init_temp_unit_C != gui_state.temp_unit_C) {
				(void) cmd_send_int32(CMD_SET, CMD_UNITS, (int32_t) gui_state.temp_unit_C);
			}
		}
	}
	
	prev_active = is_active;
}



//
// Internal functions
//
static void _cb_sw_units(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		gui_state.temp_unit_C = lv_switch_get_state(obj);
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
