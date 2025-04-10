/*
 * GUI Live image display panel
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

#include "cmd_list.h"
#include "cmd_utilities.h"
#include "gui_panel_image_main.h"
#include "gui_render.h"
#include "gui_state.h"
#include "gui_utilities.h"
#include "lv_conf.h"
#include "palettes.h"
#ifdef ESP_PLATFORM
	#include "esp_log.h"
	#include "esp_heap_caps.h"
	#include "freertos/FreeRTOS.h"
	#include "freertos/task.h"
	#include "gui_task.h"
#else
	#include "gui_main.h"
	#include <stdio.h>
	#include <stdlib.h>
#endif
#include <math.h>
#include <string.h>



//
// Local constants
//

// Region selection state machine
#define REGION_SEL_IDLE         0
#define REGION_SEL_WAIT_PRESS   1
#define REGION_SEL_WAIT_RELEASE 2


//
// Local variables
//
static const char* TAG = "gui_panel_image_main";

// Message related
static bool message_displayed = false;
static char message_buf[GUIP_MAX_MSG_LEN+1];

// State
static bool is_portrait;
static int mag_level;
static uint16_t img_w, img_h;

// Region selection
static int region_sel_state = REGION_SEL_IDLE;
static uint16_t region_start_x, region_start_y;
static uint16_t region_end_x, region_end_y;

//
// LVGL Objects
//
static lv_obj_t* my_panel;

// Header bar
static lv_obj_t* lbl_batt_status;
static lv_obj_t* lbl_timelapse;
static lv_obj_t* lbl_env_info;
static lv_obj_t* lbl_spot_temp;
static lv_obj_t* lbl_region_temp;

// Palette bar
static lv_obj_t* canvas_colormap;
static lv_draw_line_dsc_t canvas_line_dsc;
static lv_obj_t* lbl_max_temp;
static lv_obj_t* lbl_min_temp;
static lv_obj_t* line_temp_marker;
static lv_style_t style_line_temp_marker;


// Message bar
static lv_obj_t* lbl_message;

// Image area
static lv_obj_t* canvas_image;

// Timer tasks
static lv_task_t* task_batt_timer;          // Interval between update battery state requests
static lv_task_t* task_timelapse_timer;     // Interval between alternating battery and timelapse labels
static lv_task_t* task_message_timer;       // Countdown timer for timed message display
static lv_task_t* task_pmessage_timer;      // Countdown timer for display of palette name
static lv_task_t* task_palette_upd_timer;   // Countdown timer after palette change for NVS update
static lv_task_t* task_region_sel_timer;    // Countdown timer to end region select with no selection

// Canvas image buffers
#ifdef ESP_PLATFORM
	static uint16_t* img_canvas_buffer;
	static uint16_t* cmap_canvas_buffer;
#else
	static uint32_t* img_canvas_buffer;
	static uint32_t* cmap_canvas_buffer;
#endif

//
// Externally accessible image structure
//
gui_img_buf_t gui_panel_image_buf;



//
// Forward declarations for internal functions
//
static void _configure_sizes();

static void _update_colormap();
static void _update_env_info(gui_img_buf_t* img_bufP);
static void _update_spot_temp(gui_img_buf_t* img_bufP);
static void _update_min_max_temps(gui_img_buf_t* img_bufP);
static void _update_region_temps(gui_img_buf_t* img_bufP);
static void _update_palette_marker(gui_img_buf_t* img_bufP);
static void _update_message_string(char* msg);

static void _cb_change_palette(lv_obj_t* obj, lv_event_t event);
static void _cb_canvas_event(lv_obj_t* obj, lv_event_t event);

static void _task_eval_batt_timer(lv_task_t* task);
static void _task_eval_timelapse_timer(lv_task_t* task);
static void _task_eval_message_timer(lv_task_t* task);
static void _task_eval_pmessage_timer(lv_task_t* task);
static void _task_eval_palette_upd_timer(lv_task_t* task);
static void _task_eval_region_sel_timer(lv_task_t* task);

static void _region_drag_coord_to_xy(uint16_t* x1, uint16_t* y1, uint16_t* x2, uint16_t* y2);


//
// API
//
void gui_panel_image_calculate_size(uint16_t max_w, uint16_t max_h, uint16_t* imgp_w, uint16_t* imgp_h)
{
	// Note: this routine embodies knowledge of the types of displays (smallest: gCore)
	// this code will run on.  It probably could be generalized some...
	
	// Calculate the size of the panel and the image canvas
	is_portrait = (max_w < max_h);
	if (is_portrait) {
		// Width controls dimensions for portrait displays (image is rotated so H & W are exchanged)
		if (max_w > (2*GUI_RAW_IMG_H + GUIPN_IMAGE_PAL_BAR_W)) {
			img_w = 2*GUI_RAW_IMG_H;
			img_h = 2*GUI_RAW_IMG_W;
			mag_level = GUI_MAGNIFICATION_2_0;
		} else {
			img_w = 3*GUI_RAW_IMG_H/2;
			img_h = 3*GUI_RAW_IMG_W/2;
			mag_level = GUI_MAGNIFICATION_1_5;
		}
	} else {
		// Height controls dimensions for landscape displays
		if (max_h > (2*GUI_RAW_IMG_H + GUIPN_IMAGE_STATUS_H)) {
			img_w = 2*GUI_RAW_IMG_W;
			img_h = 2*GUI_RAW_IMG_H;
			mag_level = GUI_MAGNIFICATION_2_0;
		} else {
			img_w = 3*GUI_RAW_IMG_W/2;
			img_h = 3*GUI_RAW_IMG_H/2;
			mag_level = GUI_MAGNIFICATION_1_5;
		}
	}
	
	*imgp_w = img_w + GUIPN_IMAGE_PAL_BAR_W;
	*imgp_h = img_h + GUIPN_IMAGE_STATUS_H;
}


lv_obj_t* gui_panel_image_init(lv_obj_t* page)
{
	// Allocate memory for the colormap in the palette bar
#ifdef ESP_PLATFORM
	cmap_canvas_buffer = (uint16_t*) heap_caps_calloc(GUIPN_IMAGE_PAL_W*GUIPN_IMAGE_PAL_H, sizeof(uint16_t), MALLOC_CAP_SPIRAM);
	if (cmap_canvas_buffer == NULL) {
		ESP_LOGE(TAG, "Could not allocate cmap_canvas_buffer");
		// Try to do anything else? Display a popup ???
		vTaskDelete(NULL);
	}
#else
	cmap_canvas_buffer = (uint32_t*) calloc(GUIPN_IMAGE_PAL_W*GUIPN_IMAGE_PAL_H, sizeof(uint32_t));
	if (cmap_canvas_buffer == NULL) {
		// ???
		printf("%s Could not allocate cmap_canvas_buffer", TAG);
	}
#endif
	
	// Allocate memory for the image
#ifdef ESP_PLATFORM
	img_canvas_buffer = (uint16_t*) heap_caps_calloc(2*GUI_LARGEST_MAG_FACTOR*GUI_RAW_IMG_W*GUI_RAW_IMG_W, sizeof(uint16_t), MALLOC_CAP_SPIRAM);
	if (img_canvas_buffer == NULL) {
		ESP_LOGE(TAG, "Could not allocate img_canvas_buffer");
		// Try to do anything else? Display a popup ???
		vTaskDelete(NULL);
	}
#else
	img_canvas_buffer = (uint32_t*) calloc(2*GUI_LARGEST_MAG_FACTOR*GUI_RAW_IMG_W*GUI_RAW_IMG_W, sizeof(uint32_t));
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
	// Header - Battery Status - Left side
	lbl_batt_status = lv_label_create(my_panel, NULL);
	lv_label_set_long_mode(lbl_batt_status, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_batt_status, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_pos(lbl_batt_status, GUIPN_IMAGE_BATT_X_OFFSET, GUIPN_IMAGE_BATT_Y_OFFSET);
	lv_obj_set_width(lbl_batt_status, GUIPN_IMAGE_PAL_BAR_W);
	lv_label_set_static_text(lbl_batt_status, LV_SYMBOL_BATTERY_EMPTY);
	
	// Header - Timelapse status - Left side with same position as Battery Status
	lbl_timelapse = lv_label_create(my_panel, NULL);
	lv_label_set_long_mode(lbl_timelapse, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_timelapse, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_pos(lbl_timelapse, GUIPN_IMAGE_BATT_X_OFFSET, GUIPN_IMAGE_BATT_Y_OFFSET);
	lv_obj_set_width(lbl_timelapse, GUIPN_IMAGE_PAL_BAR_W); 
	lv_label_set_static_text(lbl_timelapse, "TL");
	lv_obj_set_hidden(lbl_timelapse,  true);        // Starts hidden
	
	// Header - Environmental info - left justified
	lbl_env_info = lv_label_create(my_panel, NULL);
	lv_obj_set_pos(lbl_env_info, GUIPN_IMAGE_PAL_BAR_W + GUIPN_IMAGE_ENV_X_OFFSET, GUIPN_IMAGE_ENV_Y_OFFSET);
	lv_obj_set_style_local_text_font(lbl_env_info, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SMALL);
	lv_obj_set_width(lbl_env_info, GUIPN_IMAGE_ENV_W);
	lv_label_set_static_text(lbl_env_info, "");
	
	// Header - Spot - centered (position depends on image size so computed later)
	lbl_spot_temp = lv_label_create(my_panel, NULL);
	lv_label_set_long_mode(lbl_spot_temp, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_spot_temp, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_width(lbl_spot_temp, GUIPN_IMAGE_SPOT_W);
	lv_label_set_static_text(lbl_spot_temp, "");
	
	// Header - Region temps - right justified
	lbl_region_temp = lv_label_create(my_panel, NULL);
	lv_obj_set_style_local_text_font(lbl_region_temp, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SMALL);
	lv_label_set_long_mode(lbl_region_temp, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_region_temp, LV_LABEL_ALIGN_RIGHT);
	lv_obj_set_width(lbl_region_temp, GUIPN_IMAGE_RGN_W);
	lv_label_set_static_text(lbl_region_temp, "");
		
	// Palette bar - Left
	//
	// Line width for drawing colormap (palette)
	lv_draw_line_dsc_init(&canvas_line_dsc);
	canvas_line_dsc.width = 1;
	
	// Colormap canvas
	canvas_colormap = lv_canvas_create(my_panel, NULL);
	lv_canvas_set_buffer(canvas_colormap, cmap_canvas_buffer, GUIPN_IMAGE_PAL_W, GUIPN_IMAGE_PAL_H, LV_IMG_CF_TRUE_COLOR);
	lv_obj_set_pos(canvas_colormap, GUIPN_IMAGE_PAL_X_OFFSET, GUIPN_IMAGE_PAL_Y_OFFSET);
	lv_obj_set_size(canvas_colormap, GUIPN_IMAGE_PAL_W, GUIPN_IMAGE_PAL_H);
	lv_obj_set_click(canvas_colormap, true);
	lv_obj_set_event_cb(canvas_colormap, _cb_change_palette);
	
	// Line style for drawing temp marker
	lv_style_init(&style_line_temp_marker);
	lv_style_set_line_width(&style_line_temp_marker, LV_STATE_DEFAULT, 1);
    lv_style_set_line_color(&style_line_temp_marker, LV_STATE_DEFAULT, LV_THEME_DEFAULT_COLOR_PRIMARY);
    
    // Temp marker line
    line_temp_marker = lv_line_create(my_panel, NULL);
	lv_obj_add_style(line_temp_marker, LV_LINE_PART_MAIN, &style_line_temp_marker);
	lv_obj_set_hidden(line_temp_marker,  true);
	
	// Max Temp - above colormap
	lbl_max_temp = lv_label_create(my_panel, NULL);
	lv_obj_set_style_local_text_font(lbl_max_temp, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SMALL);
	lv_label_set_long_mode(lbl_max_temp, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_max_temp, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_pos(lbl_max_temp, GUIPN_IMAGE_MINMAX_X_OFFSET, GUIPN_IMAGE_MAX_T_Y_OFFSET);
	lv_obj_set_width(lbl_max_temp, GUIPN_IMAGE_PAL_BAR_W);
	lv_obj_set_click(lbl_max_temp, true);
	lv_obj_set_event_cb(lbl_max_temp, _cb_change_palette);
	lv_label_set_static_text(lbl_max_temp, "");
	
	// Min Temp - below colormap
	lbl_min_temp = lv_label_create(my_panel, NULL);
	lv_obj_set_style_local_text_font(lbl_min_temp, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SMALL);
	lv_label_set_long_mode(lbl_min_temp, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_min_temp, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_pos(lbl_min_temp, GUIPN_IMAGE_MINMAX_X_OFFSET, GUIPN_IMAGE_MIN_T_Y_OFFSET);
	lv_obj_set_width(lbl_min_temp, GUIPN_IMAGE_PAL_BAR_W);
	lv_obj_set_click(lbl_min_temp, true);
	lv_obj_set_event_cb(lbl_min_temp, _cb_change_palette);
	lv_label_set_static_text(lbl_min_temp, "");
	
	// Image area - to the right of palette bar, below header and above message bar
	canvas_image = lv_canvas_create(my_panel, NULL);
	lv_obj_set_pos(canvas_image, GUIPN_IMAGE_IMG_X_OFFSET, GUIPN_IMAGE_IMG_Y_OFFSET);
	lv_obj_set_click(canvas_image, true);
	lv_obj_set_event_cb(canvas_image, _cb_canvas_event);
		
	// Message bar - below header at top of image (size depends on image size so computed later)
	// Note: defined after canvas image because it needs to display on top
	lbl_message = lv_label_create(my_panel, NULL);
	lv_obj_set_y(lbl_message, GUIPN_IMAGE_IMG_Y_OFFSET + GUIPN_IMAGE_MSG_OFFSET_Y);
	lv_obj_set_style_local_bg_color(lbl_message, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUI_THEME_BG_COLOR);
	lv_obj_set_style_local_bg_opa(lbl_message, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
	lv_label_set_long_mode(lbl_message, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_message, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_x(lbl_message, GUIPN_IMAGE_MSG_OFFSET_X);
	_update_message_string("");

	// Setup dimensions of objects we computed in gui_panel_image_calculate_size()
	_configure_sizes();
	
	return my_panel;
}


void gui_panel_image_reset_size()
{
	// Setup dimensions of objects we computed in gui_panel_image_calculate_size()
	_configure_sizes();
}


void gui_panel_image_set_active(bool is_active)
{
	if (is_active) {
		// Initialize display elements
		_update_colormap();
		
		// Start the battery status update timer (the first time it immediately times out)
		if (task_batt_timer == NULL) {
			task_batt_timer = lv_task_create(_task_eval_batt_timer, 500, LV_TASK_PRIO_LOW, NULL);
		}
		
		// Start the timelapse indicate timer if necessary
		if (gui_state.timelapse_running) {
			if (task_timelapse_timer == NULL) {
				task_timelapse_timer = lv_task_create(_task_eval_timelapse_timer, GUIPN_IMAGE_TIMELAPSE_MSEC, LV_TASK_PRIO_MID, NULL);
			}
		}
	} else {
		// Stop the battery status update timer
		if (task_batt_timer != NULL) {
			lv_task_del(task_batt_timer);
			task_batt_timer = NULL;
		}
		
		// Stop the battery status update timer (and redisplay battery icon for next time)
		if (task_timelapse_timer != NULL) {
			lv_task_del(task_timelapse_timer);
			task_timelapse_timer = NULL;
			
			lv_obj_set_hidden(lbl_batt_status,  false);
			lv_obj_set_hidden(lbl_timelapse,  true);
		}
	}
}


void gui_panel_image_set_batt_percent(int percent)
{
	if (percent == 0) {
		lv_label_set_static_text(lbl_batt_status, LV_SYMBOL_BATTERY_EMPTY);
	} else if (percent <= 25) {
		lv_label_set_static_text(lbl_batt_status, LV_SYMBOL_BATTERY_1);
	} else if (percent <= 50) {
		lv_label_set_static_text(lbl_batt_status, LV_SYMBOL_BATTERY_2);
	} else if (percent <= 75) {
		lv_label_set_static_text(lbl_batt_status, LV_SYMBOL_BATTERY_3);
	} else {
		lv_label_set_static_text(lbl_batt_status, LV_SYMBOL_BATTERY_FULL);
	}
}


void gui_panel_image_render_image()
{
	static bool halt_updates = false;
	
	if (gui_panel_image_buf.vid_frozen) {
		// Just render the video frozen marker over whatever image we're currently displaying
		if (!halt_updates) {
		
			gui_render_freeze_marker(img_canvas_buffer);
			lv_obj_invalidate(canvas_image);
		}
		halt_updates = true;
	} else {
		
		halt_updates = false;
		
		// Render the image into the frame buffer
		gui_render_image_data(&gui_panel_image_buf, img_canvas_buffer, &gui_state);
				
		// Render the spot meter if enabled
		if (gui_state.spotmeter_enable && gui_panel_image_buf.spot_valid) {
			gui_render_spotmeter(&gui_panel_image_buf, img_canvas_buffer);
		}
		
		// Render the min/max markers if enabled
		if (gui_state.min_max_mrk_enable && gui_panel_image_buf.minmax_valid) {
			gui_render_min_max_markers(&gui_panel_image_buf, img_canvas_buffer);
		}
		
		// Render the region marker if enabled or we dragging to select the region
		if (region_sel_state == REGION_SEL_WAIT_RELEASE) {
			// Draw drag marker (re-using gui_panel_image_buf state)
			_region_drag_coord_to_xy(&gui_panel_image_buf.region_x1, &gui_panel_image_buf.region_y1, 
			                         &gui_panel_image_buf.region_x2, &gui_panel_image_buf.region_y2);
			gui_render_region_drag_marker(&gui_panel_image_buf, img_canvas_buffer);
			
		} else if (gui_state.region_enable && gui_panel_image_buf.region_valid) {
			// Draw region marker
			gui_render_region_marker(&gui_panel_image_buf, img_canvas_buffer);
		}
		
		// Finally invalidate the object to force it to redraw from the buffer
		lv_obj_invalidate(canvas_image);
		
		// Update temps
		_update_env_info(&gui_panel_image_buf);
		_update_spot_temp(&gui_panel_image_buf);
		_update_min_max_temps(&gui_panel_image_buf);
		_update_region_temps(&gui_panel_image_buf);
		_update_palette_marker(&gui_panel_image_buf);
	}
}


void gui_panel_image_set_message(char* msg, int display_msec)
{
	// Only display this message if nothing else is being displayed
	if (!message_displayed) {
		strncpy(message_buf, msg, GUIP_MAX_MSG_LEN);
		message_buf[GUIP_MAX_MSG_LEN] = 0;
		_update_message_string(message_buf);
		
		if (display_msec != 0) {
			// Start a timer
			task_message_timer = lv_task_create(_task_eval_message_timer, display_msec, LV_TASK_PRIO_MID, NULL);
		}
		
		message_displayed = true;
	} else {
		// Take down a displayed message if we get a null message
		if (strlen(msg) == 0) {
			message_displayed = false;
			
			// Blank the message
			_update_message_string("");
			
			// Stop any running timer
			if (task_message_timer != NULL) {
				lv_task_del(task_message_timer);
				task_message_timer = NULL;
			}
		}
	}
}


void gui_panel_image_set_timelapse(bool en)
{
	if (en) {
		// Start the timer to alternate the battery and timelapse labels
		if (task_timelapse_timer == NULL) {
			task_timelapse_timer = lv_task_create(_task_eval_timelapse_timer, GUIPN_IMAGE_TIMELAPSE_MSEC, LV_TASK_PRIO_MID, NULL);
		}
	} else {
		// Stop the battery status update timer (and ensure battery icon is displayed)
		if (task_timelapse_timer != NULL) {
			lv_task_del(task_timelapse_timer);
			task_timelapse_timer = NULL;
			
			lv_obj_set_hidden(lbl_batt_status,  false);
			lv_obj_set_hidden(lbl_timelapse,  true);
		}
	}
}


void gui_panel_image_update_palette()
{
	_update_colormap();
}


void gui_panel_image_enable_region_selection(bool en)
{
	if (en && (region_sel_state == REGION_SEL_IDLE)) {
		// Start region selection
		region_sel_state = REGION_SEL_WAIT_PRESS;
		gui_panel_image_set_message("Drag to select region", 0);
		task_region_sel_timer = lv_task_create(_task_eval_region_sel_timer, GUIPN_IMAGE_REGION_SEL_MSEC, LV_TASK_PRIO_LOW, NULL);
	} else if (!en && (region_sel_state != REGION_SEL_IDLE)) {
		// End region selection
		region_sel_state = REGION_SEL_IDLE;
		gui_panel_image_set_message("", 0);
		if (task_region_sel_timer != NULL) {
			lv_task_del(task_region_sel_timer);
			task_region_sel_timer = NULL;
		}
	}
}


bool gui_panel_image_region_selection_in_progress()
{
	return (region_sel_state != REGION_SEL_IDLE);
}



//
// Internal functions
//
static void _configure_sizes()
{
	// Configure the size of the panel
	lv_obj_set_size(my_panel, img_w + GUIPN_IMAGE_PAL_BAR_W, img_h + GUIPN_IMAGE_STATUS_H);
	
	// Configure the size of the image canvas
	lv_canvas_set_buffer(canvas_image, img_canvas_buffer, img_w, img_h, LV_IMG_CF_TRUE_COLOR);
	
	// Configure the spot temp position
	lv_obj_set_pos(lbl_spot_temp, GUIPN_IMAGE_PAL_BAR_W + (img_w - GUIPN_IMAGE_SPOT_W)/2, GUIPN_IMAGE_SPOT_Y_OFFSET);
	
	// Configure the region temp position
	lv_obj_set_pos(lbl_region_temp, GUIPN_IMAGE_RGN_X_OFFSET + GUIPN_IMAGE_PAL_BAR_W + img_w - GUIPN_IMAGE_RGN_W, GUIPN_IMAGE_RGN_Y_OFFSET);
	
	// Conifigure the width of the message bar text
	lv_obj_set_width(lbl_message, img_w);
	
	// Configure the rendering engine
	gui_render_set_configuration(is_portrait ? GUI_RENDER_PORTRAIT : GUI_RENDER_LANDSCAPE, mag_level);
}


static void _update_colormap()
{
	int i;
	lv_point_t points[2];
	
	points[0].x = GUIPN_IMAGE_PAL_X_OFFSET;
	points[1].x = GUIPN_IMAGE_PAL_X_OFFSET + GUIPN_IMAGE_PAL_W;
	
	// Draw color map top -> bottom / hot -> cold
	for (i=0; i<256; i++) {
		points[0].y = i;
		points[1].y = points[0].y;
		canvas_line_dsc.color = (lv_color_t) PALETTE_LOOKUP(255-i);
		lv_canvas_draw_line(canvas_colormap, points, 2, &canvas_line_dsc);
	}
}


static void _update_env_info(gui_img_buf_t* img_bufP)
{
	static char buf[24] = { 0 };                  // ">-xxx°C xxx% x.xm" + null + safety
	char work_buf[24] = { 0 };
	int n = 0;
	
	// Environmental info displayed if available
	if (gui_state.use_auto_ambient && (img_bufP->amb_temp_valid || img_bufP->amb_hum_valid || img_bufP->distance_valid)) {
		// Indicate that we are using ambient correction
		work_buf[0] = '>';
		n = 1;
	}
	if (img_bufP->amb_temp_valid) {
		int t = (int) img_bufP->amb_temp;
		if (gui_state.temp_unit_C) {
			sprintf(&work_buf[n], "%d°C ", t);
		} else {
			t = t * 9.0 / 5.0 + 32.0;
			sprintf(&work_buf[n], "%d°F ", t);
		}
		n = strlen(work_buf);
	}
	if (img_bufP->amb_hum_valid) {
		sprintf(&work_buf[n], "%u%% ", img_bufP->amb_hum);
		n = strlen(work_buf);
	}
	if (img_bufP->distance_valid) {
		float d = gui_dist_to_disp_dist(img_bufP->distance, &gui_state);
		if (gui_state.temp_unit_C) {
			sprintf(&work_buf[n], "%1.2fm", d);
		} else {
			int ft = floor(d);
			int in = round((d - ft) * 12);
			if (in == 12) {
				in = 0;
				ft += 1;
			}
			sprintf(&work_buf[n], "%d' %d\"", ft, in);
		}
	}
	
	if (strcmp(work_buf, buf) != 0) {
		// Only update display when changed
		strcpy(buf, work_buf);
		lv_label_set_static_text(lbl_env_info, buf);
	}
}


static void _update_spot_temp(gui_img_buf_t* img_bufP)
{
	static char buf[8] = { 0 };                 // "-xxx °C" to "xxx °C" + null + safety
	char work_buf[8] = { 0 };
	int t;
	
	// Spot displayed if enabled and valid
	if (gui_state.spotmeter_enable && img_bufP->spot_valid) {
		t = round(gui_t1c_to_disp_temp(img_bufP->spot_temp, &gui_state));
		if (gui_state.temp_unit_C) {
			sprintf(work_buf, "%d °C", t);
		} else {
			sprintf(work_buf, "%d °F", t);
		}
	}
	
	if (strcmp(work_buf, buf) != 0) {
		strcpy(buf, work_buf);
		lv_label_set_static_text(lbl_spot_temp, buf);
	}
}


static void _update_min_max_temps(gui_img_buf_t* img_bufP)
{
	static char buf_min[6] = { 0 };            // "-xxx" to "xxx" + null + safety
	static char buf_max[6] = { 0 };
	static char work_buf_min[6] = { 0 };
	static char work_buf_max[6] = { 0 };
	int t;
	
	// Temps are always displayed if they are valid
	if (img_bufP->minmax_valid) {
		// Max temp
		t = round(gui_t1c_to_disp_temp(img_bufP->max_temp, &gui_state));
		sprintf(work_buf_max, "%d", t);
		
		// Min temp
		t = round(gui_t1c_to_disp_temp(img_bufP->min_temp, &gui_state));
		sprintf(work_buf_min, "%d", t);
	} else {
		// Blank displays
		work_buf_max[0] = 0;
		work_buf_min[0] = 0;
	}
	
	if (strcmp(work_buf_max, buf_max) != 0) {
		strcpy(buf_max, work_buf_max);
		lv_label_set_static_text(lbl_max_temp, buf_max);
	}
	
	if (strcmp(work_buf_min, buf_min) != 0) {
		strcpy(buf_min, work_buf_min);
		lv_label_set_static_text(lbl_min_temp, buf_min);
	}
}


static void _update_region_temps(gui_img_buf_t* img_bufP)
{
	static char buf[24] = { 0 };                 // "-xxx / -xxx / -xxx °C" + null + safety
	char work_buf[24] = { 0 };
	int min, avg, max;
	
	// Temps displayed if enabled and valid
	if (gui_state.region_enable && img_bufP->region_valid) {
		min = round(gui_t1c_to_disp_temp(img_bufP->region_min_temp, &gui_state));
		avg = round(gui_t1c_to_disp_temp(img_bufP->region_avg_temp, &gui_state));
		max = round(gui_t1c_to_disp_temp(img_bufP->region_max_temp, &gui_state));
		if (gui_state.temp_unit_C) {
			sprintf(work_buf, "%d / %d / %d °C", min, avg, max);
		} else {
			sprintf(work_buf, "%d / %d / %d °F", min, avg, max);
		}
	}
	if (strcmp(work_buf, buf) != 0) {
		strcpy(buf, work_buf);
		lv_label_set_static_text(lbl_region_temp, buf);
	}
}


static void _update_palette_marker(gui_img_buf_t* img_bufP)
{
	static bool displayed = false;
	static lv_point_t points[2];
	uint16_t y_offset;
	
	if (gui_state.spotmeter_enable && img_bufP->spot_valid) {
		if (!displayed) {
			lv_obj_set_hidden(line_temp_marker,  false);
		}
		y_offset = GUIPN_IMAGE_MRK_Y_OFFSET;
		y_offset += round(((float)(img_bufP->max_temp - img_bufP->spot_temp) / 
	                      (float) (img_bufP->max_temp - img_bufP->min_temp))
	                       * GUIPN_IMAGE_PAL_H);
	    points[0].x = GUIPN_IMAGE_MRK_X_OFFSET;
	    points[1].x = GUIPN_IMAGE_MRK_X_OFFSET + GUIPN_IMAGE_MRK_W - 1;
	    points[0].y = y_offset;
	    points[1].y = y_offset;
	    lv_line_set_points(line_temp_marker, points, 2);
		displayed = true;
	} else {
		if (displayed) {
			lv_obj_set_hidden(line_temp_marker,  true);
		}
		displayed = false;
	}
}


static void _update_message_string(char* msg)
{
	lv_obj_set_hidden(lbl_message, strlen(msg) == 0);
	lv_label_set_static_text(lbl_message, msg);
}


static void _cb_change_palette(lv_obj_t* obj, lv_event_t event)
{
	bool inc_palette = false;
	bool dec_palette = false;
	lv_indev_t* touch;          // Input device
	lv_point_t cur_point;
	
	if (event == LV_EVENT_PRESSED) {
		if (obj == lbl_max_temp) {
			inc_palette = true;
		} else if (obj == lbl_min_temp) {
			dec_palette = true;
		} else if (obj == canvas_colormap) {
			touch = lv_indev_get_act();
			lv_indev_get_point(touch, &cur_point);  // Touch is absolute so we may have to adjust
			if (cur_point.y < (lv_obj_get_y(my_panel) + GUIPN_IMAGE_PAL_Y_OFFSET + (GUIPN_IMAGE_PAL_H/2))) {
				inc_palette = true;
			} else {
				dec_palette = true;
			}
		}
		
		if (inc_palette) {
			if (++gui_state.palette_index >= PALETTE_COUNT) gui_state.palette_index = 0;
		}
		if (dec_palette) {
			if (gui_state.palette_index == 0) {
				gui_state.palette_index = PALETTE_COUNT - 1;
			} else {
				--gui_state.palette_index;
			}
		}
		
		if (inc_palette || dec_palette) {
			// Update the display palette and redraw
			set_palette(gui_state.palette_index);
			_update_colormap();
			
			// Update the save palette immediately too in the controller
			(void) cmd_send_int32(CMD_SET, CMD_SAVE_PALETTE, (int32_t) gui_state.palette_index);
			
			// Start display of palette name (possibly overwriting a current message
			// which will be restored if necessary when this timer expires)
			_update_message_string(get_palette_name(gui_state.palette_index));
			if (task_pmessage_timer == NULL) {
				// Start the timer
				task_pmessage_timer = lv_task_create(_task_eval_pmessage_timer, 1000, LV_TASK_PRIO_LOW, NULL);
			} else {
				// Reset timer
				lv_task_reset(task_pmessage_timer);
			}
			
			// Manage updating NVS - only make a permanent change a few seconds after
			// the last user change (to prevent excessive NVS flash writing due to a
			// user flipping through palettes deciding which one they want)
			if (task_palette_upd_timer == NULL) {
				// Start the timer
				task_palette_upd_timer = lv_task_create(_task_eval_palette_upd_timer, GUIPN_IMAGE_PALETTE_UPD_MSEC, LV_TASK_PRIO_MID, NULL);
			} else {
				// Restart the timer
				lv_task_reset(task_palette_upd_timer);
			}
		}
	}
}


static void _cb_canvas_event(lv_obj_t* obj, lv_event_t event)
{
	lv_indev_t* touch;          // Input device
	lv_point_t cur_point;
	uint16_t t, x1, y1, x2, y2;
	
	if ((event == LV_EVENT_PRESSED) || (event == LV_EVENT_PRESSING) || (event == LV_EVENT_RELEASED)) {
		// Get absolute image units (account for position of this page)
		touch = lv_indev_get_act();
		lv_indev_get_point(touch, &cur_point);
		x1 = cur_point.x - lv_obj_get_x(my_panel) - GUIPN_IMAGE_IMG_X_OFFSET;
		y1 = cur_point.y - lv_obj_get_y(my_panel) - GUIPN_IMAGE_IMG_Y_OFFSET;
	}
	
	if (event == LV_EVENT_PRESSED) {
		if (region_sel_state == REGION_SEL_WAIT_PRESS) {
			// Save initial position
			region_start_x = x1;
			region_start_y = y1;
			region_end_x = x1;
			region_end_y = y1;
			
			// Setup for drag
			region_sel_state = REGION_SEL_WAIT_RELEASE;
		} else if (gui_state.spotmeter_enable) {
			// Update spotmeter
			if (is_portrait) {
				// Rotate and de-scale coordinates
				t = gui_act_coord_to_real_coord(mag_level, y1);
				y1 = gui_act_coord_to_real_coord(mag_level, img_w - x1);
				x1 = t;
			} else {
				// De-scale native-orientation coordinates
				x1 = gui_act_coord_to_real_coord(mag_level, x1);
				y1 = gui_act_coord_to_real_coord(mag_level, y1);
			}
			(void) cmd_send_marker_location(CMD_SET, CMD_SPOT_LOC, x1, y1, 0, 0);
		}
	} else if (event == LV_EVENT_PRESSING) {
		if (region_sel_state == REGION_SEL_WAIT_RELEASE) {
			// Update end position
			region_end_x = x1;
			region_end_y = y1;
			
			// Stop timer once they start dragging
			if (task_region_sel_timer != NULL) {
				lv_task_del(task_region_sel_timer);
				task_region_sel_timer = NULL;
			}
		}
	} else if (event == LV_EVENT_RELEASED) {
		if (region_sel_state == REGION_SEL_WAIT_RELEASE) {
			// Done with region selection
			region_sel_state = REGION_SEL_IDLE;
			gui_panel_image_set_message("", 0);
			if (task_region_sel_timer != NULL) {
				lv_task_del(task_region_sel_timer);
				task_region_sel_timer = NULL;
			}
			
			// Get the final coordinates
			region_end_x = x1;
			region_end_y = y1;
			
			if ((region_start_x != region_end_x) && (region_start_y != region_end_y)) {
				// Adjust to unified format with (x1,y1) upper-left
				_region_drag_coord_to_xy(&x1, &y1, &x2, &y2);
				
				// Rotate if necessary and de-scale
				if (is_portrait) {
					t = gui_act_coord_to_real_coord(mag_level, y1);
					y1 = gui_act_coord_to_real_coord(mag_level, img_w - x2);
					x2 = gui_act_coord_to_real_coord(mag_level, y2);
					y2 = gui_act_coord_to_real_coord(mag_level, img_w - x1);
					x1 = t;
				} else {
					x1 = gui_act_coord_to_real_coord(mag_level, x1);
					y1 = gui_act_coord_to_real_coord(mag_level, y1);
					x2 = gui_act_coord_to_real_coord(mag_level, x2);
					y2 = gui_act_coord_to_real_coord(mag_level, y2);
				}
				
				// Tell the controller about the region to get and enable the marker
				(void) cmd_send_marker_location(CMD_SET, CMD_REGION_LOC, x1, y1, x2, y2);
				gui_state.region_enable = true;
				(void) cmd_send_int32(CMD_SET, CMD_REGION_EN, (int32_t) gui_state.region_enable);
			}
		}
	}
}


static void _task_eval_batt_timer(lv_task_t* task)
{
	// Reset our timer in case we're the first execution
	lv_task_set_period(task, GUIPN_IMAGE_BATT_UPD_MSEC);
	
	// Request battery status
	(void) cmd_send(CMD_GET, CMD_BATT_LEVEL);
}


static void _task_eval_timelapse_timer(lv_task_t* task)
{
	// Toggle label visibility
	if (lv_obj_get_hidden(lbl_batt_status)) {
		lv_obj_set_hidden(lbl_batt_status,  false);
		lv_obj_set_hidden(lbl_timelapse,  true);
	} else {
		lv_obj_set_hidden(lbl_batt_status,  true);
		lv_obj_set_hidden(lbl_timelapse,  false);
	}
}


static void _task_eval_message_timer(lv_task_t* task)
{
	// Blank the message
	_update_message_string("");
	message_displayed = false;
	
	// Delete the timer
	lv_task_del(task_message_timer);
	task_message_timer = NULL;
}


static void _task_eval_pmessage_timer(lv_task_t* task)
{
	if (message_displayed) {
		// Restore previous message (the task_message_timer may take it down later)
		_update_message_string(message_buf);
	} else {
		// Otherwise blank the display
		_update_message_string("");
	}
	
	// Delete the timer
	lv_task_del(task_pmessage_timer);
	task_pmessage_timer = NULL;
}


static void _task_eval_palette_upd_timer(lv_task_t* task)
{
	// Save the current palette to NVS
	(void) cmd_send_int32(CMD_SET, CMD_PALETTE, (int32_t) gui_state.palette_index);
	
	// Terminate the timer
	lv_task_del(task_palette_upd_timer);
	task_palette_upd_timer = NULL;
}


static void _task_eval_region_sel_timer(lv_task_t* task)
{
	// End region selection
	region_sel_state = REGION_SEL_IDLE;
	gui_panel_image_set_message("", 0);
	lv_task_del(task_region_sel_timer);
	task_region_sel_timer = NULL;
}


// Calculate box coordinates so that (x1, y1) is upper-left and (x2, y2) is lower right
//
// Four cases of point E (region_end_xy) relative to point S (region_start_xy):
//
//   (E)-----+-----(E)
//    |  r1  |  r2  |
//    |      |      |
//    +-----(S)-----+
//    |      |      |
//    |  r3  |  r4  |
//   (E)-----+-----(E)
//
static void _region_drag_coord_to_xy(uint16_t* x1, uint16_t* y1, uint16_t* x2, uint16_t* y2)
{
	if (region_start_x > region_end_x) {
		if (region_start_y > region_end_y) {
			// r1
			*x1 = region_end_x;
			*y1 = region_end_y;
			*x2 = region_start_x;
			*y2 = region_start_y;
		} else {
			// r3
			*x1 = region_end_x;
			*y1 = region_start_y;
			*x2 = region_start_x;
			*y2 = region_end_y;
		}
	} else {
		if (region_start_y > region_end_y) {
			// r2
			*x1 = region_start_x;
			*y1 = region_end_y;
			*x2 = region_end_x;
			*y2 = region_start_y;
		} else {
			// r4
			*x1 = region_start_x;
			*y1 = region_start_y;
			*x2 = region_end_x;
			*y2 = region_end_y;
		}
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
