/*
 * GUI Live image display page
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
#ifndef GUI_PAGE_IMAGE_H
#define GUI_PAGE_IMAGE_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>


//
// Constants
//

// Maximum spacing to the top, left and between panels in each page
#define GUIP_IMAGE_MAX_SPACING    32



//
// API
//
lv_obj_t* gui_page_image_init(lv_obj_t* screen, uint16_t page_w, uint16_t page_h, bool mobile);
void gui_page_image_set_active(bool is_active);
void gui_page_image_reset_screen_size(uint16_t page_w, uint16_t page_h);


#endif /* GUI_PAGE_IMAGE_H */
