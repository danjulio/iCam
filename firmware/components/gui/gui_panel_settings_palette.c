/*
 * GUI settings camera palette control panel
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
#include "gui_panel_image_main.h"
#include "gui_panel_settings_palette.h"
#include "gui_state.h"
#include "palettes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
	#include "esp_heap_caps.h"
	#include "esp_log.h"
	#include "gui_task.h"
#else
	#include "gui_main.h"
#endif



//
// Local variables
//
static const char* TAG = "gui_panel_settings_palette";

// State
static bool prev_active = false;
static int cur_palette_index;

//
// LVGL Objects
//
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* rlr_palette;

// Roller string
static char rlr_string[128];



//
// Forward declarations for internal functions
//
static void _build_rlr_string();
static void _cb_rlr_palette(lv_obj_t* obj, lv_event_t event);



//
// API
//
void gui_panel_settings_palette_init(lv_obj_t* parent_cont)
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
	lv_label_set_static_text(lbl_name, "Palette");
	
	// Palette selection roller
	_build_rlr_string();
	rlr_palette = lv_roller_create(my_panel, NULL);
	lv_roller_set_options(rlr_palette, rlr_string, LV_ROLLER_MODE_NORMAL);
	lv_roller_set_auto_fit(rlr_palette, false);
	lv_obj_set_size(rlr_palette, GUIPN_SETTINGS_PALETTE_RLR_W, GUIPN_SETTINGS_PALETTE_RLR_H);
	lv_obj_set_style_local_bg_color(rlr_palette, LV_ROLLER_PART_SELECTED, LV_STATE_DEFAULT, GUI_THEME_RLR_BG_COLOR);
	lv_obj_set_event_cb(rlr_palette, _cb_rlr_palette);
    
    // Register with our parent page
	gui_page_settings_register_panel(my_panel, NULL, NULL, NULL);
}


void gui_panel_settings_palette_set_active(bool is_active)
{
	if (is_active) {
		// Get the current palette
		cur_palette_index = gui_state.palette_index;
		lv_roller_set_selected(rlr_palette, (uint16_t) cur_palette_index, LV_ANIM_OFF);
	} else {
		if (prev_active) {
			// Update the controller palette values if there was a change
			if (cur_palette_index != gui_state.palette_index) {
				gui_state.palette_index = cur_palette_index;
				set_palette(gui_state.palette_index);
				(void) cmd_send_int32(CMD_SET, CMD_SAVE_PALETTE, (int32_t) gui_state.palette_index);  // First so next updates both in NVS
				(void) cmd_send_int32(CMD_SET, CMD_PALETTE, (int32_t) gui_state.palette_index);
				gui_panel_image_update_palette();
			}
		}
	}
	
	prev_active = is_active;
}



//
// Internal functions
//
static void _build_rlr_string()
{
	size_t sum = 0;
/*	
	// First compute how much memory we need to hold a string of all palette names
	for (int i=0; i<PALETTE_COUNT; i++) {
		sum += (size_t) strlen(get_palette_name(i)) + 1;
	}
	
	// Allocate the string (we don't expect this to fail)
#ifdef ESP_PLATFORM
	ESP_LOGI(TAG, "Allocate %d", sum);
	rlr_string = heap_caps_malloc(sum, MALLOC_CAP_SPIRAM); // MALLOC_CAP_8BIT
	if (rlr_string == NULL) {
		ESP_LOGE(TAG, "Could not allocate palette string");
	}
#else
	rlr_string = malloc(sum);
	if (rlr_string == NULL) {
		printf("%s Could not allocate palette string\n", TAG);
	}
#endif
*/	
	// Add the strings (reuse sum for current length)
	for (int i=0; i<PALETTE_COUNT; i++) {
		if (i < (PALETTE_COUNT-1)) {
			sprintf((&rlr_string[0] + sum), "%s\n", get_palette_name(i));
		} else {
			sprintf((&rlr_string[0] + sum), "%s", get_palette_name(i));
		}
		sum = strlen(&rlr_string[0]);
	}
	rlr_string[sum] = 0;
}


static void _cb_rlr_palette(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		cur_palette_index = (int) lv_roller_get_selected(obj);
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
