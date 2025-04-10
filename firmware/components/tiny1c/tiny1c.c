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
#include "tiny1c.h"
#include <math.h>



//
// API
//
uint16_t brightness_to_param_value(uint32_t b)
{
	float t;
	
	// Convert incoming 0 - 100 range to 0 - 255 for Tiny1C
	t = (float) b * 255.0 / 100.0;
	if (t > 255.0) t = 255.0;
	
	return (uint16_t) round(t);
}


uint32_t param_to_brightness_value(uint16_t p)
{
	float t;
	
	// Convert 0 - 255 to 0 - 100
	t = (float) p * 100.0 / 255.0;
	if (t > 100.0) t = 100.0;
	
	return (uint32_t) round(t);
}


uint16_t dist_cm_to_param_value(uint32_t d)
{
	float t;
	
	// Convert incoming distance 0 - 20000cm to 0 - 25600 (128 count/m)
	t = (float) d * 128.0 / 100.0;
	if (t > 25600.0) t = 25600.0;
	
	return (uint16_t) round(t);
}


uint32_t param_to_dist_cm_value(uint16_t p)
{
	// Convert 0 - 25600 to 0 - 20000
	return (uint32_t) (p * 20000 / 25600);
}


uint16_t emissivity_to_param_value(uint32_t e)
{
	float t;
	
	// Convert incoming 0 - 100 range to 0 - 128
	t = (float) e * 128.0 / 100.0;
	if (t > 128.0) t = 128.0;
	return (uint16_t) round(t);
}


uint32_t param_to_emissivity_value(uint16_t p)
{
	float t;
	
	// Convert 0 - 128 to 0 - 100
	t = (float) p * 100.0 / 128.0;
	if (t > 100.0) t = 100.0;
	
	return (uint32_t) round(t);
}


uint16_t temperature_to_param_value(int32_t t)
{
	// Convert incoming t (°C) to 230 - 900 (°K)
	if (t < -43) t = -43;
	if (t > 627) t = 627;
	return (uint16_t) (t + 273);
}


int32_t param_to_temperature_value(uint16_t p)
{
	// Convert incoming °K to °C
	return (int32_t) (p - 273);
}


float temp_to_float_temp(uint16_t v, bool temp_unit_C)
{
	float t;
	
	t = ((float) v) / 16.0 - 273.15;

	// Convert to F if required
	if (!temp_unit_C) {
		t = t * 9.0 / 5.0 + 32.0;
	}
	
	return t;
}
