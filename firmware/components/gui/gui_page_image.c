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
#include "esp_system.h"
#ifndef CONFIG_BUILD_ICAM_MINI

#include "cmd_utilities.h"
#include "gui_page_image.h"
#include "gui_panel_image_main.h"
#include "gui_panel_image_controls.h"



//
// Local variables
//

// State
static bool is_mobile;
static bool is_portrait;
static bool prev_is_portrait;
static uint16_t my_width;
static uint16_t my_height;
static uint16_t image_panel_major_dim;
static uint16_t left_spacing;
static uint16_t top_spacing;
static uint16_t inner_spacing;

// LVGL objects
static lv_obj_t* my_page;
static lv_obj_t* panel_image;
static lv_obj_t* panel_image_controls;



//
// Forward declarations for internal routines
//
static void _calculate_layout(uint16_t page_w, uint16_t page_h);
static void _layout_page(uint16_t page_w, uint16_t page_h);
static void _set_controller_orientation();


//
// API
//
lv_obj_t* gui_page_image_init(lv_obj_t* screen, uint16_t page_w, uint16_t page_h, bool mobile)
{
	is_mobile = mobile;

	// Create the top-level container for the page
	my_page = lv_cont_create(screen, NULL);
	lv_obj_set_pos(my_page, 0, 0);
	lv_obj_set_click(my_page, false);
		
	// Setup the page layout
	_calculate_layout(page_w, page_h);
	
	// Add panels to the page
	panel_image = gui_panel_image_init(my_page);
	panel_image_controls = gui_panel_image_controls_init(my_page);
	
	// Setup the page layout
	_layout_page(page_w, page_h);
	
	// Inform the controller of our orientation
	_set_controller_orientation();
	
	return my_page;
}


void gui_page_image_set_active(bool is_active)
{
	// Enable image streaming when page is visible
	(void) cmd_send_int32(CMD_SET, CMD_STREAM_EN, is_active ? 1 : 0);
	
	// Set page visibility
	lv_obj_set_hidden(my_page, !is_active);
	
	// Let our children know
	gui_panel_image_set_active(is_active);
	gui_panel_image_controls_set_active(is_active);
}


void gui_page_image_reset_screen_size(uint16_t page_w, uint16_t page_h)
{
	// Reconfigure page layout
	_calculate_layout(page_w, page_h);
	_layout_page(page_w, page_h);
	
	// Allow panels to resize
	gui_panel_image_reset_size();
	gui_panel_image_controls_reset_size();
	
	// Inform our controller of any orientation changes
	if (is_portrait != prev_is_portrait) {
		_set_controller_orientation();
	}
}



//
// Internal routines
//
void _calculate_layout(uint16_t page_w, uint16_t page_h)
{
	uint16_t img_w, img_h;
	uint16_t imgc_w, imgc_h;
	
	// Get the image panel w and h for this set of dimensions
	gui_panel_image_calculate_size(page_w, page_h, &img_w, &img_h);
	
	// The image control panel will take the image panel's minor dimension since 
	// its major dimension will be the same
	if (img_h > img_w) {
		// portrait
		is_portrait = true;
		imgc_w = img_w;
		gui_panel_image_controls_calculate_size(imgc_w, &imgc_h, true);
		image_panel_major_dim = img_h;
	} else {
		// landscape
		is_portrait = false;
		imgc_h = img_h;
		gui_panel_image_controls_calculate_size(imgc_h, &imgc_w, false);
		image_panel_major_dim = img_w;
	}
	
	// Compute the spacing around the panels in this page
	if (is_portrait) {
		// Compute width related
		if (page_w > (2 * GUIP_IMAGE_MAX_SPACING + img_w)) {
			left_spacing = GUIP_IMAGE_MAX_SPACING;
		} else {
			if (img_w > page_w) {
				left_spacing = 0;
			} else {
				left_spacing = (page_w - img_w) / 2;
			}
		}
		
		// Compute height related
		if (page_h > (3 * GUIP_IMAGE_MAX_SPACING + (img_h + imgc_h))) {
			top_spacing = GUIP_IMAGE_MAX_SPACING;
			inner_spacing = GUIP_IMAGE_MAX_SPACING;
		} else {
			if ((img_h + imgc_h) > page_h) {
				top_spacing = 0;
				inner_spacing = 0;
			} else {
				top_spacing = (page_h - (img_h + imgc_h)) / 3;
				inner_spacing = top_spacing;
			}
		}
		
		// Calculate this page dimensions
		my_width = left_spacing + img_w + left_spacing;
		if (my_width > page_w) {
			// Cut down to minimum possible (whack off right-most spacing)
				my_width -= left_spacing;
		}
		
		my_height = top_spacing + img_h + inner_spacing + imgc_h + top_spacing;
		if (my_height > page_h) {
			my_height -= top_spacing;
		}
	} else {
		// Compute width related
		if (page_w > (3 * GUIP_IMAGE_MAX_SPACING + (img_w + imgc_w))) {
			left_spacing = GUIP_IMAGE_MAX_SPACING;
			inner_spacing = GUIP_IMAGE_MAX_SPACING;
		} else {
			if ((img_w + imgc_w) > page_w) {
				left_spacing = 0;
				inner_spacing = 0;
			} else {
				left_spacing = (page_w - (img_w + imgc_w)) / 3;
				inner_spacing = left_spacing;
			}
		}
		
		// Compute height related
		if (page_h > (2 * GUIP_IMAGE_MAX_SPACING + img_h)) {
			top_spacing = GUIP_IMAGE_MAX_SPACING;
		} else {
			if (img_h > page_h) {
				top_spacing = 0;
			} else {
				top_spacing = (page_h - img_h) / 2;
			}
		}
		
		// Calculate this page dimensions
		my_width = left_spacing + img_w + inner_spacing  + imgc_w + left_spacing;
		if (my_width > page_w) {
			// Cut down to minimum possible (whack off right-most spacing)
			my_width -= left_spacing;
		}
		
		my_height = top_spacing + img_h + top_spacing;
		if (my_height > page_h) {
			my_height -= top_spacing;
		}
	}
}


static void _layout_page(uint16_t page_w, uint16_t page_h)
{	
	lv_obj_set_size(my_page, my_width, my_height);
	
	lv_obj_set_pos(panel_image, left_spacing, top_spacing);
	
	if (is_portrait) {
		lv_obj_set_pos(panel_image_controls, left_spacing, top_spacing + image_panel_major_dim + inner_spacing);
	} else {
		lv_obj_set_pos(panel_image_controls, left_spacing + image_panel_major_dim + inner_spacing, top_spacing);
	}
	
}


static void _set_controller_orientation()
{
	(void) cmd_send_int32(CMD_SET, CMD_ORIENTATION, is_portrait ? 1 : 0);
	prev_is_portrait = is_portrait;
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
