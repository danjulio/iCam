/*
 * Colormap structure for converting 8-bit indexed data to 16- or 24-bit RGB
 *
 * Copyright 2020-2024 Dan Julio
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
#include "palettes.h"
#include "arctic.h"
#include "banded.h"
#include "blackhot.h"
#include "double_rainbow.h"
#include "fusion.h"
#include "gray.h"
#include "ironblack.h"
#include "isotherm.h"
#include "rainbow.h"
#include "sepia.h"


//
// Palette Variables
//
static palette_t palettes[PALETTE_COUNT] = {
	{
		.name = "White Hot",
		.map_ptr = &gray_palette_map
	},
	{
		.name = "Black Hot",
		.map_ptr = &blackhot_palette_map
	},
	{
		.name = "Arctic",
		.map_ptr = &arctic_palette_map
	},
	{
		.name = "Fusion",
		.map_ptr = &fusion_palette_map
	},
	{
		.name = "Iron Black",
		.map_ptr = &ironblack_palette_map
	},
	{
		.name = "Rainbow",
		.map_ptr = &rainbow_palette_map
	},
	{
		.name = "Rainbow HC",
		.map_ptr = &double_rainbow_palette_map
	},	
	{
		.name = "Sepia",
		.map_ptr = &sepia_palette_map
	},
	{
		.name = "Banded",
		.map_ptr = &banded_palette_map
	},
	{
		.name = "IsoTherm",
		.map_ptr = &isotherm_palette_map
	}
};



//
// Palette variables
//
#ifdef ESP_PLATFORM
uint16_t palette16[256];                 // Current palette for display fast lookup
uint32_t palette24[256];                 // Current palette for file save fast lookup
#else
uint32_t palette24[256];                 // Current palette for file save fast lookup
uint32_t palette32[256];                 // Current palette for display fast lookup
#endif
int cur_palette;
int cur_save_palette;



//
// Palette API
//
void set_palette(int n)
{
	int i;
	
	if (n < PALETTE_COUNT) {
		for (i=0; i<256; i++) {
#ifdef ESP_PLATFORM
			palette16[i] = RGB_TO_16BIT_SWAP(
				(*(palettes[n].map_ptr))[i][0],
				(*(palettes[n].map_ptr))[i][1],
				(*(palettes[n].map_ptr))[i][2]
			);
#else
			palette32[i] = RGB_TO_32BIT(
				(*(palettes[n].map_ptr))[i][0],
				(*(palettes[n].map_ptr))[i][1],
				(*(palettes[n].map_ptr))[i][2]
			);
#endif
		}
		cur_palette = n;
	}
}


void set_save_palette(int n)
{
	int i;
	
	if (n < PALETTE_COUNT) {		
		for (i=0; i<256; i++) {
			palette24[i] = RGB_TO_24BIT(
				(*(palettes[n].map_ptr))[i][0],
				(*(palettes[n].map_ptr))[i][1],
				(*(palettes[n].map_ptr))[i][2]
			);
		}
		cur_save_palette = n;
	}
}


char* get_palette_name(int n)
{
	return palettes[n].name;
}
