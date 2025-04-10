/*
 * Renderers for file images
 *
 * Note about portrait vs landscape rendering: The buffer is treated in a landscape (native
 * Tiny1C) orientation for all drawing routines except draw_char.  This means that the coordinates
 * passed to them must change based on orientation.  draw_char automatically renders differently
 * based on orientation.
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
#ifndef FILE_RENDER_H
#define FILE_RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include "palettes.h"
#include "out_state_utilities.h"
#include "tiny1c.h"


//
// File Render Constants
//

// Palette display area (on left edge of image)
#define FILE_IMG_PALETTE_WIDTH    16
#define FILE_IMG_PAL_TEXT_HEIGHT  16
#define FILE_IMG_PAL_TEXT_WIDTH   32
#define FILE_IMG_CMAP_WIDTH       (FILE_IMG_PALETTE_WIDTH/2)
#define FILE_IMG_PALETTE_MRK_X    (FILE_IMG_CMAP_WIDTH + 1)
#define FILE_IMG_PALETTE_MRK_W    (FILE_IMG_PALETTE_WIDTH/4 + 2)

// Region temp display area (on upper right of image)
#define FILE_IMG_REG_TEXT_HEIGHT  16

// Environmental conditions (on lower right of image)
#define FILE_IMG_ENV_TEXT_HEIGHT  16

// Spot meter
#define FILE_IMG_SPOT_SIZE        6

// Min/Max markers
#define FILE_IMG_MARKER_SIZE      8

// Palette background color
#define FILE_PALETTE_BG_COLOR     RGB_TO_24BIT(0x20, 0x20, 0x20)

// Marker color
#define FILE_MARKER_COLOR         RGB_TO_24BIT(0xF0, 0xF0, 0xF0)

// Text color
#define FILE_TEXT_COLOR           RGB_TO_24BIT(0xF0, 0xF0, 0xF0)

// Text background color
#define FILE_TEXT_BG_COLOR        RGB_TO_24BIT(0x20, 0x20, 0x20)



//
// File Render API
//
void file_render_set_orientation(bool is_portrait);
void file_render_t1c_data(t1c_buffer_t* t1c, uint32_t* img);
void file_render_spotmeter(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g);
void file_render_min_max_markers(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g);
void file_render_region_marker(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g);
void file_render_region_temps(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g);
void file_render_palette(uint32_t* img, out_state_t* g);  // Render Palette related after markers
void file_render_palette_marker(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g);
void file_render_min_max_temps(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g);
void file_render_env_info(t1c_buffer_t* t1c, uint32_t* img, out_state_t* g);

#endif /* FILE_RENDER_H */
