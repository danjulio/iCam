/*
 * GUI file browse image display panel
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

#include "gui_panel_file_browser_files.h"
#include "gui_panel_file_browser_image.h"
#include "gui_render.h"
#include "gui_state.h"
#include "gui_utilities.h"
#ifdef ESP_PLATFORM
	#include "gui_task.h"
//	#include "esp_system.h"
	#include "esp_log.h"
	#include "esp_heap_caps.h"
	#include "freertos/FreeRTOS.h"
	#include "freertos/task.h"
	#include "sys_utilities.h"
#else
	#include "gui_main.h"
	#include <stdio.h>
	#include <stdlib.h>
#endif
#include <string.h>



//
// Local constants
//



//
// Local variables
//
static const char* TAG = "gui_panel_file_browser_image";

// State
static uint16_t img_w, img_h;


//
// LVGL Objects
//
static lv_obj_t* my_panel;

// Message bar
static lv_obj_t* lbl_message;

// Image area
static lv_obj_t* canvas_image;

// Controls
static lv_obj_t* ctrl_assy;
static lv_obj_t* btn_prev;
static lv_obj_t* lbl_btn_prev;
static lv_obj_t* btn_delete;
static lv_obj_t* lbl_btn_delete;
static lv_obj_t* btn_next;
static lv_obj_t* lbl_btn_next;


// Canvas image buffers
#ifdef ESP_PLATFORM
	static uint16_t* img_canvas_buffer;
#else
	static uint32_t* img_canvas_buffer;
#endif



//
// Forward declarations for internal functions
//
static void _configure_sizes();
static void _cb_btn(lv_obj_t* obj, lv_event_t event);



//
// API
//
void gui_panel_file_browser_image_calculate_size(uint16_t max_w, uint16_t max_h, uint16_t* imgp_w, uint16_t* imgp_h)
{
	// Note: this routine embodies knowledge of the types of displays (smallest: gCore)
	// this code will run on.  It currently only scales by 1x and always renders in 
	// landscape mode.  It probably could be generalized some...
	
	// Calculate the size of the panel and the image canvas
	img_w = GUI_RAW_IMG_W;
	img_h = GUI_RAW_IMG_H;
	
	*imgp_w = img_w;
	*imgp_h = img_h + GUIPN_FILE_BROWSER_CTRL_H;
}


lv_obj_t* gui_panel_file_browser_image_init(lv_obj_t* page)
{
	// Allocate memory for the image
#ifdef ESP_PLATFORM
	// We've already allocated this buffer (RGB565)
	img_canvas_buffer = rgb_file_image;
#else
	// We have to allocate it on the web platform (ARGB8888)
	img_canvas_buffer = (uint32_t*) malloc(GUI_RAW_IMG_W * GUI_RAW_IMG_H * sizeof(uint32_t));
	if (img_canvas_buffer == NULL) {
		// ???
		printf("%s Could not allocate img_canvas_buffer", TAG);
	}
#endif
	
	// Panel
	my_panel = lv_obj_create(page, NULL);
	lv_obj_set_style_local_border_width(my_panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_click(my_panel, false);
	
	// Panel objects
	//	
	// Image area
	canvas_image = lv_canvas_create(my_panel, NULL);
	lv_obj_set_pos(canvas_image, GUIPN_FILE_BROWSER_IMG_X_OFFSET, GUIPN_FILE_BROWSER_IMG_Y_OFFSET);
		
	// Nothing to display message bar - centered in image at top
	// Note: defined after canvas image because it needs to display on top
	lbl_message = lv_label_create(my_panel, NULL);
	lv_obj_set_y(lbl_message, GUIPN_FILE_BROWSER_IMG_Y_OFFSET + GUIPN_FILE_BROWSER_MSG_OFFSET_Y);
	lv_obj_set_style_local_bg_color(lbl_message, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUI_THEME_BG_COLOR);
	lv_obj_set_style_local_bg_opa(lbl_message, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
	lv_label_set_long_mode(lbl_message, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_message, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_message, "");
	lv_obj_set_hidden(lbl_message, true);
	
	// Control assembly to hold buttons so they space themselves dynamically
	ctrl_assy = lv_obj_create(my_panel, NULL);
	lv_obj_set_click(ctrl_assy, false);
	lv_obj_set_height(ctrl_assy, GUIPN_FILE_BROWSER_CTRL_H);
	lv_obj_set_style_local_border_width(ctrl_assy, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
	
	// Navigate Back button
	btn_prev = lv_btn_create(ctrl_assy, NULL);
	lv_obj_set_size(btn_prev, GUIPN_FILE_BROWSER_BTN_W, GUIPN_FILE_BROWSER_BTN_H);
	lv_obj_align(btn_prev, ctrl_assy, LV_ALIGN_IN_LEFT_MID, 0, 0);
	lv_obj_set_auto_realign(btn_prev, true);
	lv_obj_add_protect(btn_prev, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_border_width(btn_prev, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_border_width(btn_prev, LV_BTN_PART_MAIN, LV_STATE_PRESSED, 0);
	lv_obj_set_state(btn_prev, LV_STATE_DISABLED);
	lv_obj_set_event_cb(btn_prev, _cb_btn);
	
	lbl_btn_prev = lv_label_create(btn_prev, NULL);
	lv_label_set_align(lbl_btn_prev, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_btn_prev, LV_SYMBOL_LEFT);
	
	// Delete button
	btn_delete = lv_btn_create(ctrl_assy, NULL);
	lv_obj_set_size(btn_delete, GUIPN_FILE_BROWSER_BTN_W, GUIPN_FILE_BROWSER_BTN_H);
	lv_obj_align(btn_delete, ctrl_assy, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_auto_realign(btn_delete, true);
	lv_obj_add_protect(btn_delete, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_border_width(btn_delete, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_border_width(btn_delete, LV_BTN_PART_MAIN, LV_STATE_PRESSED, 0);
	lv_obj_set_state(btn_delete, LV_STATE_DISABLED);
	lv_obj_set_event_cb(btn_delete, _cb_btn);
	
	lbl_btn_delete = lv_label_create(btn_delete, NULL);
	lv_label_set_align(lbl_btn_delete, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_btn_delete, LV_SYMBOL_TRASH);
	
	// Navigate Forward button
	btn_next = lv_btn_create(ctrl_assy, NULL);
	lv_obj_set_size(btn_next, GUIPN_FILE_BROWSER_BTN_W, GUIPN_FILE_BROWSER_BTN_H);
	lv_obj_align(btn_next, ctrl_assy, LV_ALIGN_IN_RIGHT_MID, 0, 0);
	lv_obj_set_auto_realign(btn_next, true);
	lv_obj_add_protect(btn_next, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_border_width(btn_next, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_border_width(btn_next, LV_BTN_PART_MAIN, LV_STATE_PRESSED, 0);
	lv_obj_set_state(btn_next, LV_STATE_DISABLED);
	lv_obj_set_event_cb(btn_next, _cb_btn);
	
	lbl_btn_next = lv_label_create(btn_next, NULL);
	lv_label_set_align(lbl_btn_next, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_btn_next, LV_SYMBOL_RIGHT);

	// Setup dimensions of objects we computed in gui_panel_file_browser_image_calculate_size()
	_configure_sizes();
	
	// Initialize
	gui_panel_file_browser_image_set_valid(false);
	
	return my_panel;
}


void gui_panel_file_browser_image_reset_size()
{
	// Setup dimensions of objects we computed in gui_panel_file_browser_image_calculate_size()
	_configure_sizes();
}


void gui_panel_file_browser_image_set_active(bool is_active)
{
	static bool prev_active = false;
	
	if (is_active) {
		prev_active = true;
	} else {
		if (prev_active) {
			gui_panel_file_browser_image_set_valid(false);
			
			lv_obj_set_state(btn_prev, LV_STATE_DISABLED);
			lv_obj_set_state(btn_delete, LV_STATE_DISABLED);
			lv_obj_set_state(btn_next, LV_STATE_DISABLED);
			
			prev_active = false;
		}
	}
}


void gui_panel_file_browser_image_set_valid(bool valid)
{
	static bool message_displayed = false;
	
	if (valid) {
		if (message_displayed) {
			lv_obj_set_hidden(lbl_message, true);
			message_displayed = false;
		}
	} else {
		if (gui_state.card_present) {
			lv_label_set_static_text(lbl_message, "Nothing selected");
		} else {
			lv_label_set_static_text(lbl_message, "No SD Card");
		}
		
		if (!message_displayed) {
			lv_obj_set_hidden(lbl_message, false);
			message_displayed = true;
			
			// Blank the canvas so only the message shows
#ifdef ESP_PLATFORM
			memset(img_canvas_buffer, 0, sizeof(uint16_t) * GUI_RAW_IMG_W * GUI_RAW_IMG_H);
#else
			memset(img_canvas_buffer, 0, sizeof(uint32_t) * GUI_RAW_IMG_W * GUI_RAW_IMG_H);
#endif
			lv_obj_invalidate(canvas_image);
		}
	}
}


#ifdef ESP_PLATFORM
void gui_panel_file_browser_image_set_image()
{
	// Buffer is already loaded so just update
	lv_obj_invalidate(canvas_image);
}
#else
void gui_panel_file_browser_image_set_image(uint32_t len, uint8_t* src)
{
	uint8_t* end = src + len;
	uint32_t* dst = (uint32_t*) img_canvas_buffer;
	
	// Copy the src into our canvas buffer converting 24-bit RGB to 32-bit ARGB8888
	while (src < end) {
		*dst++ = 0xFF000000 | (*(src+0) << 16) | (*(src+1) << 8) | (*(src+2));
		src += 3;
	}
	
	// Then update
	lv_obj_invalidate(canvas_image);
}
#endif


void gui_panel_file_browser_image_set_ctrl_en(int index, bool en)
{
	switch (index) {
		case GUIPN_FILE_BROWSER_IMAGE_NEXT:
			lv_obj_set_state(btn_next, en ? LV_STATE_DEFAULT : LV_STATE_DISABLED);
			break;
		case GUIPN_FILE_BROWSER_IMAGE_PREV:
			lv_obj_set_state(btn_prev, en ? LV_STATE_DEFAULT : LV_STATE_DISABLED);
			break;
		case GUIPN_FILE_BROWSER_IMAGE_DEL:
			lv_obj_set_state(btn_delete, en ? LV_STATE_DEFAULT : LV_STATE_DISABLED);
			break;
	}
}



//
// Internal functions
//
static void _configure_sizes()
{
	// Configure the size of the panel
	lv_obj_set_size(my_panel, img_w, img_h + GUIPN_FILE_BROWSER_CTRL_H);
	
	// Configure the size of the image canvas
	lv_canvas_set_buffer(canvas_image, img_canvas_buffer, img_w, img_h, LV_IMG_CF_TRUE_COLOR);
	
	// Conifigure the width of the message bar text
	lv_obj_set_width(lbl_message, img_w);
	
	// Configure position of ctrl_assy
	lv_obj_set_width(ctrl_assy, img_w - GUIPN_FILE_BROWSER_CTRL_OFFSET_X);
	lv_obj_set_pos(ctrl_assy, GUIPN_FILE_BROWSER_CTRL_OFFSET_X, img_h);
}


static void _cb_btn(lv_obj_t* obj, lv_event_t event)
{
	if ((event == LV_EVENT_CLICKED) && !gui_message_box_displayed()) {
		if (obj == btn_prev) {
			gui_panel_file_browser_files_action(GUIPN_FILE_BROWSER_FILES_ACT_PREV);
		} else if (obj == btn_next) {
			gui_panel_file_browser_files_action(GUIPN_FILE_BROWSER_FILES_ACT_NEXT);
		} else if (obj == btn_delete) {
			gui_panel_file_browser_files_action(GUIPN_FILE_BROWSER_FILES_ACT_DEL);
		}
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
