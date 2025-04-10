/*
 * GUI settings camera emissivity control panel
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
#include "gui_panel_settings_emissivity.h"
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
static int cur_emissivity_index;

//
// LVGL Objects
//
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* rlr_emissivity;

// Emissivity roller values
#define NUM_E_PARM_VALS 24
static const char* parm_e_list = "5\n10\n20\n30\n40\n50\n60\n70\n80\n82\n84\n86\n88\n90\n91\n92\n93\n94\n95\n96\n97\n98\n99\n100";
static const int parm_e_value[] = {5, 10, 20, 30, 40, 50, 60, 70, 80, 82, 84, 86, 88,
                                   90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};

//
// Forward declarations for internal functions
//
static void _cb_rlr_emissivity(lv_obj_t* obj, lv_event_t event);
static int _emissivity_to_rlr_index(int e);



//
// API
//
void gui_panel_settings_emissivity_init(lv_obj_t* parent_cont)
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
	lv_label_set_static_text(lbl_name, "Emissivity");
	
	// Emissivity selection roller
	rlr_emissivity = lv_roller_create(my_panel, NULL);
	lv_roller_set_options(rlr_emissivity, parm_e_list, LV_ROLLER_MODE_NORMAL);
	lv_roller_set_auto_fit(rlr_emissivity, false);
	lv_obj_set_size(rlr_emissivity, GUIPN_SETTINGS_EMISSIVITY_RLR_W, GUIPN_SETTINGS_EMISSIVITY_RLR_H);
	lv_obj_set_style_local_bg_color(rlr_emissivity, LV_ROLLER_PART_SELECTED, LV_STATE_DEFAULT, GUI_THEME_RLR_BG_COLOR);
	lv_obj_set_event_cb(rlr_emissivity, _cb_rlr_emissivity);
    
    // Register with our parent page
	gui_page_settings_register_panel(my_panel, NULL, NULL, NULL);
}


void gui_panel_settings_emissivity_set_active(bool is_active)
{
	if (is_active) {
		// Get the current emissivity
		cur_emissivity_index = _emissivity_to_rlr_index((int) gui_state.emissivity);
		lv_roller_set_selected(rlr_emissivity, (uint16_t) cur_emissivity_index, LV_ANIM_OFF);
	} else {
		if (prev_active) {
			// Update emissivity if there was a change
			if (parm_e_value[cur_emissivity_index] != (int) gui_state.emissivity) {
				gui_state.emissivity = (uint32_t) parm_e_value[cur_emissivity_index];
				(void) cmd_send_int32(CMD_SET, CMD_EMISSIVITY, (int32_t) gui_state.emissivity);
			}
		}
	}
	
	prev_active = is_active;
}



//
// Internal functions
//
static void _cb_rlr_emissivity(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		cur_emissivity_index = (int) lv_roller_get_selected(obj);
	}
}


static int _emissivity_to_rlr_index(int e)
{
	// Look for the closest match
	if (e <= parm_e_value[0]) {
		return 0;
	}

	for (int i=0; i<(NUM_E_PARM_VALS-1); i++) {
		if ((2*e) < (parm_e_value[i] + parm_e_value[i+1])) {
			return i;
		}
	}
	
	return (NUM_E_PARM_VALS - 1);
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
