/*
 * Output (display) state and management performed on the camera.
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
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "out_state_utilities.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "t1c_task.h"


//
// Variables
//
static const char* TAG = "out_state_utilities";

// Globally visible state
out_state_t out_state;

// PS config
static t1c_config_t t1c_config;
static out_config_t out_config;



//
// API
//
void out_state_init()
{
	// Setup the initial GUI state
	(void) ps_get_config(PS_CONFIG_TYPE_T1C, &t1c_config);
	(void) ps_get_config(PS_CONFIG_TYPE_OUT, &out_config);

	out_state.auto_ffc_en = t1c_config.auto_ffc_en;
	out_state.high_gain = t1c_config.high_gain;
	out_state.is_portrait = false;  // Will be set by output_task
	out_state.min_max_mrk_enable = (out_config.config_flags & PS_EN_FLAG_MINMAX_MRK) != 0;
	out_state.min_max_tmp_enable = (out_config.config_flags & PS_EN_FLAG_MINMAX_TMP) != 0;
	out_state.output_mode_PAL = (out_config.config_flags & PS_EN_FLAG_VID_IS_PAL) != 0;
	out_state.refl_equals_ambient = t1c_config.refl_equals_ambient;
	out_state.region_enable = false; // Region always starts out disabled
	out_state.save_ovl_en = (out_config.config_flags & PS_EN_FLAG_SAVE_OVL) != 0;
	out_state.spotmeter_enable = (out_config.config_flags & PS_EN_FLAG_SPOTMETER) != 0;
	out_state.temp_unit_C = (out_config.config_flags & PS_EN_FLAG_UNITS_METRIC) != 0;
	out_state.use_auto_ambient = t1c_config.use_auto_ambient;
	
	out_state.gui_palette_index = out_config.gui_palette_index;
	out_state.sav_palette_index = out_config.sav_palette_index;
	out_state.vid_palette_index = out_config.vid_palette_index;

	out_state.atmospheric_temp = t1c_config.atmospheric_temp;
	out_state.brightness = t1c_config.brightness;
	out_state.distance = t1c_config.distance;
	out_state.emissivity = t1c_config.emissivity;
	out_state.ffc_temp_threshold_x10 = t1c_config.ffc_temp_threshold_x10;
	out_state.humidity = t1c_config.humidity;
	out_state.lcd_brightness = out_config.lcd_brightness;
	out_state.min_ffc_interval = t1c_config.min_ffc_interval;
	out_state.max_ffc_interval = t1c_config.max_ffc_interval;
	out_state.reflected_temp = t1c_config.reflected_temp;
}


// Note: We don't save out_state.region_enable or out_state.is_portrait as they are
// run-time only state
void out_state_save()
{
	bool t1c_parm_changed = false;
	bool gui_parm_changed = false;
	
	// Look for conditions that would update the T1C PS entry
	if (out_state.auto_ffc_en != t1c_config.auto_ffc_en) {
		t1c_parm_changed = true;
		t1c_config.auto_ffc_en = out_state.auto_ffc_en;
	}
	if (out_state.high_gain != t1c_config.high_gain) {
		t1c_parm_changed = true;
		t1c_config.high_gain = out_state.high_gain;
	}
	if (out_state.use_auto_ambient != t1c_config.use_auto_ambient) {
		t1c_parm_changed = true;
		t1c_config.use_auto_ambient = out_state.use_auto_ambient;
	}
	if (out_state.refl_equals_ambient != t1c_config.refl_equals_ambient) {
		t1c_parm_changed = true;
		t1c_config.refl_equals_ambient = out_state.refl_equals_ambient;
	}
	if (out_state.atmospheric_temp != t1c_config.atmospheric_temp) {
		t1c_parm_changed = true;
		t1c_config.atmospheric_temp = out_state.atmospheric_temp;
	}
	if (out_state.brightness != t1c_config.brightness) {
		t1c_parm_changed = true;
		t1c_config.brightness = out_state.brightness;
	}
	if (out_state.distance != t1c_config.distance) {
		t1c_parm_changed = true;
		t1c_config.distance = out_state.distance;
	}
	if (out_state.emissivity != t1c_config.emissivity) {
		t1c_parm_changed = true;
		t1c_config.emissivity = out_state.emissivity;
	}
	if (out_state.ffc_temp_threshold_x10 != t1c_config.ffc_temp_threshold_x10) {
		t1c_parm_changed = true;
		t1c_config.ffc_temp_threshold_x10 = out_state.ffc_temp_threshold_x10;
	}
	if (out_state.humidity != t1c_config.humidity) {
		t1c_parm_changed = true;
		t1c_config.humidity = out_state.humidity;
	}
	if (out_state.min_ffc_interval != t1c_config.min_ffc_interval) {
		t1c_parm_changed = true;
		t1c_config.min_ffc_interval = out_state.min_ffc_interval;
	}
	if (out_state.max_ffc_interval != t1c_config.max_ffc_interval) {
		t1c_parm_changed = true;
		t1c_config.max_ffc_interval = out_state.max_ffc_interval;
	}
	if (out_state.reflected_temp != t1c_config.reflected_temp) {
		t1c_parm_changed = true;
		t1c_config.reflected_temp = out_state.reflected_temp;
	}
	
	// Look for conditions that would update the Output PS entry
	if (out_state.gui_palette_index != out_config.gui_palette_index) {
		gui_parm_changed = true;
		out_config.gui_palette_index = out_state.gui_palette_index;
	}
	if (out_state.sav_palette_index != out_config.sav_palette_index) {
		gui_parm_changed = true;
		out_config.sav_palette_index = out_state.sav_palette_index;
	}
	if (out_state.vid_palette_index != out_config.vid_palette_index) {
		gui_parm_changed = true;
		out_config.vid_palette_index = out_state.vid_palette_index;
	}
	if (out_state.lcd_brightness != out_config.lcd_brightness) {
		gui_parm_changed = true;
		out_config.lcd_brightness = out_state.lcd_brightness;
	}
	if (out_state.spotmeter_enable != ((out_config.config_flags & PS_EN_FLAG_SPOTMETER) != 0)) {
		gui_parm_changed = true;
		if (out_state.spotmeter_enable) {
			out_config.config_flags |= PS_EN_FLAG_SPOTMETER;
		} else {
			out_config.config_flags &= ~PS_EN_FLAG_SPOTMETER;
		}
	}
	if (out_state.temp_unit_C != ((out_config.config_flags & PS_EN_FLAG_UNITS_METRIC) != 0)) {
		gui_parm_changed = true;
		if (out_state.temp_unit_C) {
			out_config.config_flags |= PS_EN_FLAG_UNITS_METRIC;
		} else {
			out_config.config_flags &= ~PS_EN_FLAG_UNITS_METRIC;
		}
	}
	if (out_state.min_max_mrk_enable != ((out_config.config_flags & PS_EN_FLAG_MINMAX_MRK) != 0)) {
		gui_parm_changed = true;
		if (out_state.min_max_mrk_enable) {
			out_config.config_flags |= PS_EN_FLAG_MINMAX_MRK;
		} else {
			out_config.config_flags &= ~PS_EN_FLAG_MINMAX_MRK;
		}
	}
	if (out_state.min_max_tmp_enable != ((out_config.config_flags & PS_EN_FLAG_MINMAX_TMP) != 0)) {
		gui_parm_changed = true;
		if (out_state.min_max_tmp_enable) {
			out_config.config_flags |= PS_EN_FLAG_MINMAX_TMP;
		} else {
			out_config.config_flags &= ~PS_EN_FLAG_MINMAX_TMP;
		}
	}
	if (out_state.output_mode_PAL != ((out_config.config_flags & PS_EN_FLAG_VID_IS_PAL) != 0)) {
		gui_parm_changed = true;
		if (out_state.output_mode_PAL) {
			out_config.config_flags |= PS_EN_FLAG_VID_IS_PAL;
		} else {
			out_config.config_flags &= ~PS_EN_FLAG_VID_IS_PAL;
		}
	}
	if (out_state.save_ovl_en != ((out_config.config_flags & PS_EN_FLAG_SAVE_OVL) != 0)) {
		gui_parm_changed = true;
		if (out_state.save_ovl_en) {
			out_config.config_flags |= PS_EN_FLAG_SAVE_OVL;
		} else {
			out_config.config_flags &= ~PS_EN_FLAG_SAVE_OVL;
		}
	}
	
	if (t1c_parm_changed) {		
		// Store changes
		ESP_LOGI(TAG, "Update T1C PS");
		ps_set_config(PS_CONFIG_TYPE_T1C, &t1c_config);
		
		// Let t1c_task know about updated config
		 xTaskNotify(task_handle_t1c, T1C_NOTIFY_UPD_T1C_CONFIG, eSetBits);
	}
	
	if (gui_parm_changed) {
		// Store changes
		ESP_LOGI(TAG, "Update OUT PS");
		ps_set_config(PS_CONFIG_TYPE_OUT, &out_config);
	}
}
