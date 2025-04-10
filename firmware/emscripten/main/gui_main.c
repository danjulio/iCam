/*
 * Top-level GUI manager for web interface
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
#include "gui_main.h"
#include "gui_page_image.h"
#include "gui_page_settings.h"
#include "gui_page_file_browser.h"
#include "gui_render.h"
#include "gui_state.h"
#include "gui_utilities.h"
#include "lv_conf.h"
#include <stdio.h>


//
// Local constants
//

// Undefine to display LVGL memory utilization on page change
//#define DUMP_MEM_INFO

//
// Local variables
//

// LVGL page objects
static lv_obj_t* lv_pages[GUI_NUM_MAIN_PAGES];
static lv_obj_t* page_disconnected;
static lv_obj_t* lbl_disconnected;
static lv_obj_t* btn_reconnect;
static lv_obj_t* lbl_btn_reconnect;

static lv_task_t* task_init_wait = NULL;

// Page size
static uint16_t page_w;
static uint16_t page_h;

// Disconnect page state
static bool first_display;
static socket_activity_handler socket_connect_routine;
static socket_activity_handler socket_disconnect_routine;

// Global groups for LVGL objects wishing to get keypad or encoder input
lv_group_t* gui_keypad_group;
//lv_group_t* gui_encoder_group;



//
// Forward declarations for internal functions
//
static lv_obj_t* _gui_page_disconnected_init(lv_obj_t* screen, uint16_t page_w, uint16_t page_h);
static void _gui_page_disconnected_set_active(bool is_active);
static void _gui_page_disconnected_reset_screen_size(uint16_t page_w, uint16_t page_h);
static void _cb_btn_reconnect(lv_obj_t* btn, lv_event_t event);
static void _compute_page_size(uint16_t browser_w, uint16_t browser_h);
static void _check_init_done_task(lv_task_t * task);

//
// API
//
void gui_main_init(lv_obj_t* screen, uint16_t browser_w, uint16_t browser_h, bool is_mobile)
{
	_compute_page_size(browser_w, browser_h);
	
	// Initialize the rendering engine
	if (!gui_render_init()) {
		// ??? Anything else to do???
		printf("gui_render_init failed\n");
	}
	
	// Setup the actual application pages
	lv_pages[GUI_MAIN_PAGE_DISCONNECTED] = _gui_page_disconnected_init(screen, page_w, page_h);
	lv_pages[GUI_MAIN_PAGE_IMAGE] = gui_page_image_init(screen, page_w, page_h, is_mobile);
	lv_pages[GUI_MAIN_PAGE_SETTINGS] = gui_page_settings_init(screen, page_w, page_h, is_mobile);
	lv_pages[GUI_MAIN_PAGE_LIBRARY] = gui_page_file_browser_init(screen, page_w, page_h, is_mobile);

	gui_main_set_page(GUI_MAIN_PAGE_DISCONNECTED);
	
	// Display LVGL memory utilization
	gui_dump_mem_info();
}


void gui_main_set_connected(bool is_connected)
{
	// Start the display
	if (is_connected) {
		// Attempt to get the GUI state
		gui_state_init();
		
		// Start a timer to check for completion of gui_state initialization
		task_init_wait = lv_task_create(_check_init_done_task, 100, LV_TASK_PRIO_LOW, NULL);
	} else {
		// Display the disconnected page
		gui_main_set_page(GUI_MAIN_PAGE_DISCONNECTED);
		
		// Stop any running init timer
		if (task_init_wait != NULL) {
			lv_task_del(task_init_wait);
			task_init_wait = NULL;
		}
	}
}


void gui_main_reset_browser_dimensions(uint16_t browser_w, uint16_t browser_h)
{
	_compute_page_size(browser_w, browser_h);
	
	// Update the pages
	_gui_page_disconnected_reset_screen_size(page_w, page_h);
	gui_page_image_reset_screen_size(page_w, page_h);
	gui_page_settings_reset_screen_size(page_w, page_h);
	gui_page_file_browser_reset_screen_size(page_w, page_h);
}


void gui_main_set_page(uint32_t page)
{
	if (page > GUI_NUM_MAIN_PAGES) return;
	
	_gui_page_disconnected_set_active(page == GUI_MAIN_PAGE_DISCONNECTED);
	gui_page_image_set_active(page == GUI_MAIN_PAGE_IMAGE);
	gui_page_settings_set_active(page == GUI_MAIN_PAGE_SETTINGS);
	gui_page_file_browser_set_active(page == GUI_MAIN_PAGE_LIBRARY);
	
#ifdef DUMP_MEM_INFO
	gui_dump_mem_info();
#endif
}


void gui_main_register_socket_connect(socket_activity_handler connect_handler)
{
	socket_connect_routine = connect_handler;
}


void gui_main_register_socket_disconnect(socket_activity_handler disconnect_handler)
{
	socket_disconnect_routine = disconnect_handler;
}


void gui_main_shutdown()
{
	socket_disconnect_routine(); 
}



//
// Internal functions
//
static lv_obj_t* _gui_page_disconnected_init(lv_obj_t* screen, uint16_t page_w, uint16_t page_h)
{
	first_display = true;
	
	// Create the top-level container for the page
	page_disconnected = lv_cont_create(screen, NULL);
	lv_obj_set_pos(page_disconnected, 0, 0);
	lv_obj_set_size(page_disconnected, page_w, page_h);
	lv_obj_set_click(page_disconnected, false);
	
	// Disconnected status label
	lbl_disconnected = lv_label_create(page_disconnected, NULL);
	lv_label_set_long_mode(lbl_disconnected, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_disconnected, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_pos(lbl_disconnected, GUIP_DISC_LBL_OFFSET_X, GUIP_DISC_LBL_OFFSET_Y);
	lv_obj_set_width(lbl_disconnected, GUIP_DISC_LBL_W);
    lv_label_set_static_text(lbl_disconnected, "Disconnected");
	
	// Create the reconnect button
	btn_reconnect = lv_btn_create(page_disconnected, NULL);
	lv_obj_set_pos(btn_reconnect, GUIP_DISC_BTN_OFFSET_X, GUIP_DISC_BTN_OFFSET_Y);
	lv_obj_set_size(btn_reconnect, GUIP_DISC_BTN_W, GUIP_DISC_BTN_H);
	lv_obj_set_event_cb(btn_reconnect, _cb_btn_reconnect);
	
	// Reconnect button label
	lbl_btn_reconnect = lv_label_create(btn_reconnect, NULL);
	lv_label_set_static_text(lbl_btn_reconnect, "Reconnect");
	
	first_display = true;
	
	return page_disconnected;
}


static void _gui_page_disconnected_set_active(bool is_active)
{
	// Set page visibility
	lv_obj_set_hidden(page_disconnected, !is_active);
	
	// Display the reconnect button when the page is displayed after a socket was closed
	if (is_active) {
		if (first_display) {
			first_display = false;
			lv_obj_set_hidden(btn_reconnect, true);
		} else {
			lv_obj_set_hidden(btn_reconnect, false);
		}
	}
}


static void _cb_btn_reconnect(lv_obj_t* btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		// Call the registered socket connect routine
		socket_connect_routine();
	}
}


static void _gui_page_disconnected_reset_screen_size(uint16_t page_w, uint16_t page_h)
{
	lv_obj_set_size(page_disconnected, page_w, page_h);
}


static void _compute_page_size(uint16_t browser_w, uint16_t browser_h)
{
	uint16_t max_w, max_h;
	
	// Calculate the largest display we can generate
	page_w = (browser_w > LV_HOR_RES_MAX) ? LV_HOR_RES_MAX : browser_w;
	page_h = (browser_h > LV_VER_RES_MAX) ? LV_VER_RES_MAX : browser_h;
}


static void _check_init_done_task(lv_task_t * task)
{
	if (gui_state_init_complete()) {
		// Start the application display
		gui_main_set_page(GUI_MAIN_PAGE_IMAGE);
		
		// Stop the timer
		lv_task_del(task_init_wait);
		task_init_wait = NULL;
	}
}
