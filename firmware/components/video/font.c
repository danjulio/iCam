/*
 * Video output font descriptor
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
#include "font.h"
#include <string.h>


//
// API
//
uint16_t font_get_string_width(const char *str, const Font_TypeDef *Font)
{
	int n = strlen(str);
	
	return (int16_t) n * (Font->font_Width + 1);
}
