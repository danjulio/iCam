/*
 * Memory frame buffer for LVGL - allocated in PSRAM
 *
 * Copyright 2022 Dan Julio
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
#if !CONFIG_BUILD_ICAM_MINI

#include <esp_log.h>
#include "esp_heap_caps.h"
#include "mem_fb.h"

// Variables
static const char* TAG = "mem_fb";
static lv_color_t* fb;



// API
void mem_fb_init()
{
	int mult;
	
#if MEM_FB_BPP == 8
	mult = 1;
#else
#if MEM_FB_BPP == 16
	mult = 2;
#else
	mult = 4;
#endif
#endif

	// Allocate the buffer in PSRAM
	fb = (lv_color_t*) heap_caps_malloc((MEM_FB_W*MEM_FB_H)*mult, MALLOC_CAP_SPIRAM);
	if (fb == NULL) {
		ESP_LOGE(TAG, "malloc %d mem_fb bytes failed", (MEM_FB_W*MEM_FB_H)*mult);
	}
}


void mem_fb_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map)
{
	uint16_t x, y;
	
	if (fb != NULL) {
#if MEM_FB_BPP == 8
		uint8_t* cmP = (uint8_t*) color_map;
		for (y=area->y1; y<=area->y2; y++) {
			fbP = (uint8_t*) fb + (y * MEM_FB_W) + area->x1;
			for (x=area->x1; x<=area->x2; x++) {
				*fbP++ = *cmP++;
			}
		}
#else
#if MEM_FB_BPP == 16
		uint16_t* cmP = (uint16_t*) color_map;
		uint16_t* fbP;
		for (y=area->y1; y<=area->y2; y++) {
			fbP = (uint16_t*) fb + (y * MEM_FB_W) + area->x1;
			for (x=area->x1; x<=area->x2; x++) {
				*fbP++ = *cmP++;
			}
		}
#else
		uint32_t* cmP = (uint32_t*) color_map;
		for (y=area->y1; y<=area->y2; y++) {
			fbP = (uint32_t*) fb + (y * MEM_FB_W) + area->x1;
			for (x=area->x1; x<=area->x2; x++) {
				*fbP++ = *cmP++;
			}
		}
#endif
#endif
	}
	
	lv_disp_flush_ready(drv);
}


uint8_t* mem_fb_get_buffer()
{
	return (uint8_t*) fb;
}

#endif /* CONFIG_BUILD_ICAM_MINI */
