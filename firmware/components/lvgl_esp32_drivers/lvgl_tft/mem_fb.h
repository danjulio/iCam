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
#ifndef MEM_FB_H_
#define MEM_FB_H_
#include "lvgl.h"


// Dimensions
#define MEM_FB_W LV_HOR_RES_MAX
#define MEM_FB_H LV_VER_RES_MAX

// Bits per pixel (supports 8, 16 or 32)
#define MEM_FB_BPP 16


// API
void mem_fb_init();
void mem_fb_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);
uint8_t* mem_fb_get_buffer();


#endif // MEM_FB_H_