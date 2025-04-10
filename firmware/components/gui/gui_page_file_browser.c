/*
 * GUI file browser display page
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

#include "gui_page_file_browser.h"
#include "gui_panel_file_browser_image.h"
#include "gui_panel_file_browser_files.h"
#include "gui_utilities.h"

#ifdef ESP_PLATFORM
	#include "gui_task.h"
#else
	#include "gui_main.h"
#endif


//
// Local variables
//

// State
static bool is_mobile;
static bool is_portrait;
static uint16_t my_width;
static uint16_t my_height;
static uint16_t image_panel_spacing_dim;
static uint16_t left_spacing;
static uint16_t top_spacing;
static uint16_t inner_spacing;

// LVGL objects
static lv_obj_t* my_page;
static lv_obj_t* btn_back;
static lv_obj_t* lbl_btn_back;
static lv_obj_t* lbl_title;
static lv_obj_t* panel_image;
static lv_obj_t* panel_file_controls;



//
// Forward declarations for internal routines
//
static void _calculate_layout(uint16_t page_w, uint16_t page_h);
static void _layout_page();
static void _cb_back_button(lv_obj_t* obj, lv_event_t event);


//
// API
//
lv_obj_t* gui_page_file_browser_init(lv_obj_t* screen, uint16_t page_w, uint16_t page_h, bool mobile)
{
	is_mobile = mobile;

	// Create the top-level object for the page
	my_page = lv_obj_create(screen, NULL);
	lv_obj_set_pos(my_page, 0, 0);
	lv_obj_set_click(my_page, false);
	
	// Back button (fixed location)
	btn_back = lv_btn_create(my_page, NULL);
	lv_obj_set_pos(btn_back, GUIP_FILE_BROWSER_BACK_X, GUIP_FILE_BROWSER_BACK_Y);
	lv_obj_set_size(btn_back, GUIP_FILE_BROWSER_BACK_W, GUIP_FILE_BROWSER_BACK_H);
	lv_obj_add_protect(btn_back, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_border_width(btn_back, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_border_width(btn_back, LV_BTN_PART_MAIN, LV_STATE_PRESSED, 0);
	lv_obj_set_event_cb(btn_back, _cb_back_button);
	
	lbl_btn_back = lv_label_create(btn_back, NULL);
	lv_obj_set_style_local_text_font(lbl_btn_back, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
	lv_label_set_static_text(lbl_btn_back, LV_SYMBOL_LEFT);
	
	// Title (dynamic width)
	lbl_title = lv_label_create(my_page, NULL);
	lv_obj_set_style_local_text_font(lbl_title, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
	lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_title, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_pos(lbl_title, GUIP_FILE_BROWSER_TITLE_X, GUIP_FILE_BROWSER_TITLE_Y);
	lv_label_set_static_text(lbl_title, "File Browser");
		
	// Setup the page layout
	_calculate_layout(page_w, page_h);
	
	// Add panels to the page
	panel_image = gui_panel_file_browser_image_init(my_page);
	panel_file_controls = gui_panel_file_browser_files_init(my_page);
	
	// Setup the page layout
	_layout_page();
	
	return my_page;
}


void gui_page_file_browser_set_active(bool is_active)
{
	// Set page visibility
	lv_obj_set_hidden(my_page, !is_active);
	
	// Let our children know
	gui_panel_file_browser_image_set_active(is_active);
	gui_panel_file_browser_files_set_active(is_active);
}


void gui_page_file_browser_reset_screen_size(uint16_t page_w, uint16_t page_h)
{
	// Reconfigure page layout
	_calculate_layout(page_w, page_h);
	
	// Allow panels to resize
	gui_panel_file_browser_image_reset_size();
	gui_panel_file_browser_files_reset_size();
	
	// Reset our layout
	_layout_page();
}



//
// Internal routines
//
void _calculate_layout(uint16_t page_w, uint16_t page_h)
{
	uint16_t img_w, img_h;
	uint16_t imgf_w, imgf_h;
	uint16_t max_panel_h = page_h - GUIP_FILE_BROWSER_CONTROL_Y;
	
	// Our height is page height
	my_height = page_h;
	
	// Our width is constained
	my_width = page_w;
	if (my_width < GUIP_FILE_BROWSER_MIN_WIDTH) {
		my_width = GUIP_FILE_BROWSER_MIN_WIDTH;
	} else if (my_width > GUIP_FILE_BROWSER_MAX_WIDTH) {
		my_width = GUIP_FILE_BROWSER_MAX_WIDTH;
	}
	
	// Get the image panel w and h for this set of dimensions
	gui_panel_file_browser_image_calculate_size(page_w, max_panel_h, &img_w, &img_h);
	
	// The files panel will take the image panel's width (portrait) or height (landscape)
	// and compute its other dimension, possibly limited by page size
	if (page_h > page_w) {
		// portrait
		is_portrait = true;
		
		imgf_w = my_width;
		gui_panel_file_browser_files_calculate_size(max_panel_h - img_h, imgf_w, &imgf_h, true);
		image_panel_spacing_dim = img_h;
	} else {
		// landscape
		is_portrait = false;
		
		// Get browse files control panel width
		imgf_h = max_panel_h;
		gui_panel_file_browser_files_calculate_size(my_width - img_w, imgf_h, &imgf_w, false);
		image_panel_spacing_dim = img_w;
	}
	
	// Compute the spacing around the panels in this page
	if (is_portrait) {
		// Compute width related
		left_spacing = (my_width - img_w) / 2;
		
		// Compute height related
		if (max_panel_h > (2 * GUIP_FILE_BROWSER_PAD_Y + (img_h + imgf_h))) {
			top_spacing = GUIP_FILE_BROWSER_PAD_Y;
			inner_spacing = GUIP_FILE_BROWSER_PAD_Y;
		} else {
			if ((img_h + imgf_h) > max_panel_h) {
				top_spacing = 0;
				inner_spacing = 0;
			} else {
				top_spacing = (max_panel_h - (img_h + imgf_h)) / 2;
				inner_spacing = top_spacing;
			}
		}
	} else {
		// Compute width related
		if (my_width > (2 * GUIP_FILE_BROWSER_PAD_X + (img_w + imgf_w))) {
			left_spacing = GUIP_FILE_BROWSER_PAD_X;
			inner_spacing = GUIP_FILE_BROWSER_PAD_X;
		} else {
			if ((img_w + imgf_w) > my_width) {
				left_spacing = 0;
				inner_spacing = 0;
			} else {
				left_spacing = (my_width - (img_w + imgf_w)) / 2;
				inner_spacing = left_spacing;
			}
		}
		
		// Compute height related
		if (max_panel_h > (GUIP_FILE_BROWSER_PAD_Y + img_h)) {
			top_spacing = GUIP_FILE_BROWSER_PAD_Y;
		} else {
			if (img_h > max_panel_h) {
				top_spacing = 0;
			} else {
				top_spacing = (max_panel_h - img_h) / 2;
			}
		}
	}
}


static void _layout_page()
{
	// Set page dimensions
	lv_obj_set_size(my_page, my_width, my_height);
	
	// Set title max width
	lv_obj_set_width(lbl_title, my_width - (2 * GUIP_FILE_BROWSER_TITLE_X));
	
	// Set panel positions
	lv_obj_set_pos(panel_image, left_spacing,  GUIP_FILE_BROWSER_CONTROL_Y + top_spacing);
	
	if (is_portrait) {
		lv_obj_set_pos(panel_file_controls, 0, GUIP_FILE_BROWSER_CONTROL_Y + top_spacing + image_panel_spacing_dim + inner_spacing);
	} else {
		lv_obj_set_pos(panel_file_controls, left_spacing + image_panel_spacing_dim + inner_spacing, GUIP_FILE_BROWSER_CONTROL_Y + top_spacing);
	}
}


static void _cb_back_button(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		if (!gui_popup_displayed()) {
			gui_main_set_page(GUI_MAIN_PAGE_IMAGE);
		}
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
