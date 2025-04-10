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
#ifndef OUT_STATE_UTILITIES_H
#define OUT_STATE_UTILITIES_H

#include <stdbool.h>
#include <stdint.h>



//
// Typedefs
//
// Output module state
typedef struct {
	bool auto_ffc_en;                 // Automatic Shutter control enable
	bool high_gain;                   // Tiny1C gain
	bool is_portrait;                 // Output display landscape = 0, portrait = 1 (set by output task)
	bool min_max_mrk_enable;          // Min/Max Marker control
	bool min_max_tmp_enable;          // Min/Max Temp display control
	bool output_mode_PAL;             // Only used for video output
	bool refl_equals_ambient;         // Use the ambient temp for reflected temp
	bool region_enable;               // Region marker control
	bool save_ovl_en;                 // Save Overlay info on picture enable
	bool spotmeter_enable;            // Spotmeter marker control
	bool temp_unit_C;                 // Metric enable
	bool use_auto_ambient;            // Use sensor data for temp, humidity, distance if available
	uint32_t gui_palette_index;       // Used for LVGL output
	uint32_t sav_palette_index;       // Used for saving to file output
	uint32_t vid_palette_index;       // Used for video output
	int32_t atmospheric_temp;
	uint32_t brightness;
	uint32_t distance;
	uint32_t emissivity;
	uint32_t ffc_temp_threshold_x10;
	uint32_t humidity;
	uint32_t lcd_brightness;
	uint32_t min_ffc_interval;
	uint32_t max_ffc_interval;
	int32_t reflected_temp;
} out_state_t;



//
// Externally accessible state
//
extern out_state_t out_state;


//
// API
//
void out_state_init();
void out_state_save();

#endif /* OUT_STATE_UTILITIES_H */
