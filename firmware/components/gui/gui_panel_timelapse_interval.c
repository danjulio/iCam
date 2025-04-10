/*
 * GUI timelapse settings interval control panel
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
#include "gui_panel_timelapse_interval.h"
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
static lv_obj_t* rlr_interval;

// Interval roller values
#define NUM_I_PARM_VALS 12
static const char* parm_i_list = "2 sec\n5 sec\n10 sec\n15 sec\n30 sec\n1 min\n2 min\n5 min\n10 min\n15 min\n30 min\n1 hour";
static const uint32_t parm_i_value[] = {2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600};



//
// Forward declarations for internal functions
//
static void _cb_rlr_interval(lv_obj_t* obj, lv_event_t event);
static int _interval_to_rlr_index(int interval);



//
// API
//
void gui_panel_timelapse_interval_init(lv_obj_t* parent_cont)
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
	lv_label_set_static_text(lbl_name, "Interval");
	
	// Emissivity selection roller
	rlr_interval = lv_roller_create(my_panel, NULL);
	lv_roller_set_options(rlr_interval, parm_i_list, LV_ROLLER_MODE_NORMAL);
	lv_roller_set_auto_fit(rlr_interval, false);
	lv_obj_set_size(rlr_interval, GUIPN_TIMELAPSE_INTERVAL_RLR_W, GUIPN_TIMELAPSE_INTERVAL_RLR_H);
	lv_obj_set_style_local_bg_color(rlr_interval, LV_ROLLER_PART_SELECTED, LV_STATE_DEFAULT, GUI_THEME_RLR_BG_COLOR);
	lv_obj_set_event_cb(rlr_interval, _cb_rlr_interval);
    
    // Register with our parent page
	gui_sub_page_timelapse_register_panel(my_panel);
}


void gui_panel_timelapse_interval_set_active(bool is_active)
{
	int cur_index;
	
	if (is_active) {
		// Get the current value
		cur_index = _interval_to_rlr_index((int) gui_state.timelapse_interval_sec);
		lv_roller_set_selected(rlr_interval, (uint16_t) cur_index, LV_ANIM_OFF);
	}
}



//
// Internal functions
//
static void _cb_rlr_interval(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		gui_state.timelapse_interval_sec = parm_i_value[(int) lv_roller_get_selected(obj)];
		
		gui_sub_page_timelapse_note_change();
	}
}


static int _interval_to_rlr_index(int interval)
{
	// Look for the closest match
	if (interval <= parm_i_value[0]) {
		return 0;
	}

	for (int i=0; i<(NUM_I_PARM_VALS-1); i++) {
		if ((2*interval) < (parm_i_value[i] + parm_i_value[i+1])) {
			return i;
		}
	}
	
	return (NUM_I_PARM_VALS - 1);
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
