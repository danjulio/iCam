/*
 * Tiny1C misc conversion functions
 *
 * Copyright 2024 Dan Julio
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef _TINY1C_H_
#define _TINY1C_H_

#include "falcon_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <stdint.h>



//
// Constants
//
#define T1C_WIDTH  256
#define T1C_HEIGHT 192



//
// Data Structures
//

// Tiny1C per-image data structure
typedef struct {
	bool high_gain;
	bool vid_frozen;
	bool spot_valid;
	bool minmax_valid;
	bool region_valid;
	bool amb_temp_valid;
	bool amb_hum_valid;
	bool distance_valid;
	int16_t amb_temp;
	uint16_t* img_data;
	uint16_t y16_min;
	uint16_t y16_max;
	uint16_t amb_hum;
	uint16_t distance;
	uint16_t spot_temp;
	IrPoint_t spot_point;
	MaxMinTempInfo_t max_min_temp_info;
	TpdLineRectTempInfo_t region_temp_info;
	IrRect_t region_points;
	SemaphoreHandle_t mutex;
} t1c_buffer_t;

// Tiny1C parameter configuration for file metadata
typedef struct {
	uint16_t image_params[IMAGE_PROP_SEL_MIRROR_FLIP+1];
	uint16_t tpd_params[TPD_PROP_GAIN_SEL+1];
} t1c_param_metadata_t;




//
// API
//
uint16_t brightness_to_param_value(uint32_t b);
uint32_t param_to_brightness_value(uint16_t p);
uint16_t dist_cm_to_param_value(uint32_t d);
uint32_t param_to_dist_cm_value(uint16_t p);
uint16_t emissivity_to_param_value(uint32_t e);
uint32_t param_to_emissivity_value(uint16_t p);
uint16_t temperature_to_param_value(int32_t t);
int32_t param_to_temperature_value(uint16_t p);
float temp_to_float_temp(uint16_t v, bool temp_unit_C);

#endif /* _TINY1C_H_ */