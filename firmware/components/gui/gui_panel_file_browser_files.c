/*
 * GUI file browser file selection control panel
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

#include <arpa/inet.h>
#include "cmd_utilities.h"
#include "gui_panel_file_browser_files.h"
#include "gui_panel_file_browser_image.h"
#include "gui_state.h"
#include "gui_utilities.h"

#ifdef ESP_PLATFORM
	#include "gui_task.h"
#else
	#include "gui_main.h"
#endif


//
// Local constants
//

// Scroll tables up/down a row
#define SCROLL_UP   0
#define SCROLL_DOWN 1

//
// Local variables
//
static const char* TAG = "gui_panel_file_browser_files";

// State
static bool portrait;
static uint16_t panel_w, panel_h;

// Screen state
static bool load_first_file;              // Special flag used when navigating forward to the first file in a dir
static bool load_last_file;               // Special flag used when navigating back to last file in a dir
static bool scroll_middle_entry;          // Special flag used when navigating back to previous position after a delete
static int prev_card_present;             // Used to initiate an action on insertion or removal detection
static int num_dirs;
static int num_files;
static int selected_dir;                  // Set to a non-negative value when a directory name has been selected
static int selected_file;                 // Set to a non-negative value when a file name has been selected
static int prev_tbl_dir_row;
static int prev_tbl_file_row;
static int middle_entry_row;

//
// LVGL Objects
//
static lv_obj_t* my_parent_page;
static lv_obj_t* my_panel;

static lv_obj_t* page_tbl_dir_scroll;
static lv_obj_t* page_tbl_file_scroll;
static lv_obj_t* lbl_tbl_dir;
static lv_obj_t* lbl_tbl_file;
static lv_obj_t* tbl_dir_browse;
static lv_obj_t* tbl_file_browse;

// Status update task
static lv_task_t* task_update;



//
// Forward declarations for internal functions
//
static lv_obj_t* _create_table(lv_obj_t* my_page, lv_event_cb_t event_cb);
static lv_obj_t* _destroy_table(lv_obj_t* page, lv_obj_t* tbl);
static void _configure_sizes();
static void _initialize_screen_values();
static void _status_update_task(lv_task_t * task);
static void _cb_tbl_dir(lv_obj_t * obj, lv_event_t event);
static void _cb_tbl_file(lv_obj_t * obj, lv_event_t event);
static void _cb_messagebox(int btn_id);
static void _request_dir_list();
static void _request_file_list(int file_index);
static void _request_image(int dir_index, int file_index);
static void _delete_dir(int dir_index);
static void _delete_file(int dir_index, int file_index);
static void _scroll_table(lv_obj_t* tbl, int dir);
static void _scroll_table_to_index(lv_obj_t* tbl, int index);
static void _update_selected_dir_indication(uint16_t row);
static void _update_selected_file_indication(uint16_t row);
static void _update_image_panel_controls();



//
// API
//
void gui_panel_file_browser_files_calculate_size(uint16_t minor_dim_max, uint16_t major_dim, uint16_t* minor_dim, bool is_portrait)
{
	if (is_portrait) {
		panel_w = major_dim;
		if (minor_dim_max > GUIPN_FILE_BROWSER_FILES_MAX_H) {
			panel_h = GUIPN_FILE_BROWSER_FILES_MAX_H;
		} else if (minor_dim_max < GUIPN_FILE_BROWSER_FILES_MIN_H) {
			panel_h = GUIPN_FILE_BROWSER_FILES_MIN_H;
		} else {
			panel_h = minor_dim_max;
		}
		*minor_dim = panel_h;
	} else {
		panel_h = major_dim;
		if (minor_dim_max > GUIPN_FILE_BROWSER_FILES_MAX_W) {
			panel_w = GUIPN_FILE_BROWSER_FILES_MAX_W;
		} else if (minor_dim_max < GUIPN_FILE_BROWSER_FILES_MIN_W) {
			panel_w = GUIPN_FILE_BROWSER_FILES_MIN_W;
		} else {
			panel_w = minor_dim_max;
		}
		*minor_dim = panel_w;
	}
	portrait = is_portrait;
}


lv_obj_t* gui_panel_file_browser_files_init(lv_obj_t* page)
{
	// Save the top-level displayed page holding us for our pop-ups
	my_parent_page = page;
	
	// Panel
	my_panel = lv_obj_create(page, NULL);
	lv_obj_add_protect(my_panel, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_border_width(my_panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

	// Scrollable page for directory list table with no internal body padding (dynamic size and x offset)
	page_tbl_dir_scroll = lv_page_create(my_panel, NULL);
	lv_obj_set_y(page_tbl_dir_scroll, GUIPN_FILE_BROWSER_FILES_TBL_Y);
	lv_page_set_scrollable_fit2(page_tbl_dir_scroll, LV_FIT_NONE, LV_FIT_TIGHT);
	lv_page_set_scrl_layout(page_tbl_dir_scroll, LV_LAYOUT_COLUMN_MID);
	lv_obj_set_auto_realign(page_tbl_dir_scroll, true);
	lv_obj_add_protect(page_tbl_dir_scroll, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_pad_top(page_tbl_dir_scroll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_bottom(page_tbl_dir_scroll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_left(page_tbl_dir_scroll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_right(page_tbl_dir_scroll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	
	// Directory table (created when needed)
	
	// Directory table label (dynamic width)
	lbl_tbl_dir = lv_label_create(my_panel, NULL);
	lv_obj_set_y(lbl_tbl_dir, GUIPN_FILE_BROWSER_FILES_LBL_Y);
	lv_label_set_long_mode(lbl_tbl_dir, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_tbl_dir, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_tbl_dir, "Folders");

	// Scrollable page for file list table with no internal body padding (dynamic size and x offset)
	page_tbl_file_scroll = lv_page_create(my_panel, NULL);
	lv_obj_set_y(page_tbl_file_scroll, GUIPN_FILE_BROWSER_FILES_TBL_Y);
	lv_page_set_scrollable_fit2(page_tbl_file_scroll, LV_FIT_NONE, LV_FIT_TIGHT);
	lv_page_set_scrl_layout(page_tbl_file_scroll, LV_LAYOUT_COLUMN_MID);
	lv_obj_set_auto_realign(page_tbl_file_scroll, true);
	lv_obj_add_protect(page_tbl_file_scroll, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_pad_top(page_tbl_file_scroll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_bottom(page_tbl_file_scroll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_left(page_tbl_file_scroll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_right(page_tbl_file_scroll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	
	// File table (created when needed)
	
	// File table label (dynamic width)
	lbl_tbl_file = lv_label_create(my_panel, NULL);
	lv_obj_set_y(lbl_tbl_file, GUIPN_FILE_BROWSER_FILES_LBL_Y);
	lv_label_set_long_mode(lbl_tbl_file, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_tbl_file, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_tbl_file, "Files");
	
	// Setup dimensions of objects we computed in gui_panel_file_browser_files_calculate_size()
	_configure_sizes();
	
	_initialize_screen_values();
	
	return my_panel;
}


void gui_panel_file_browser_files_reset_size()
{
	// Setup dimensions of objects we computed in gui_panel_file_browser_files_calculate_size()
	_configure_sizes();
}


void gui_panel_file_browser_files_set_active(bool is_active)
{
	static bool prev_active = false;
	
	if (is_active) {
		// Enable card present updating
		gui_enable_card_present_updating(true);
		
		// Our internal status update - first time trigger immediately
		if (task_update == NULL) {
			task_update = lv_task_create(_status_update_task, 10, LV_TASK_PRIO_LOW, NULL);
		}
		
		prev_active = true;
	} else {
		if (prev_active) {
			// Disable the card present update
			gui_enable_card_present_updating(false);
			
			// Delete the table objects if they exist
			tbl_dir_browse = _destroy_table(page_tbl_dir_scroll, tbl_dir_browse);
			tbl_file_browse = _destroy_table(page_tbl_file_scroll, tbl_file_browse);
			
			// Delete the update task if it exists
			if (task_update != NULL) {
				lv_task_del(task_update);
				task_update = NULL;
			}
			
			_initialize_screen_values();
			
			prev_active = false;
		}
	}
}


void gui_panel_file_browser_files_set_catalog(int type, int num_entries, char* entries)
{
	bool saw_dot;
	bool updating_file_list = (type >= 0);
	char c;
	char filename[GUI_FILE_NAME_LEN];  // Larger than longest expected name
	int i, r;
	lv_obj_t* tbl;
	
	// Create a new table on the appropriate scrollable page (tables should be NULL on entry here)
	if (updating_file_list) {
		tbl_file_browse = _destroy_table(page_tbl_file_scroll, tbl_file_browse);
		tbl_file_browse = _create_table(page_tbl_file_scroll, _cb_tbl_file);
		tbl = tbl_file_browse;
		num_files = num_entries;
	} else {
		tbl_dir_browse = _destroy_table(page_tbl_dir_scroll, tbl_dir_browse);
		tbl_dir_browse = _create_table(page_tbl_dir_scroll, _cb_tbl_dir);
		tbl = tbl_dir_browse;
		num_dirs = num_entries;
	}
	
    lv_table_set_row_cnt(tbl, num_entries);

	// Convert list of names into table entries (strip off .JPG suffix)
	r = 0;
	while (num_entries--) {
		i = 0;
		saw_dot = false;
		for (;;) {
			c = *entries++;
			if ((c == ',') || (i == (GUI_FILE_NAME_LEN-1))) {
				filename[i] = 0;
				break;
			} else if (c == '.') {
				filename[i] = 0;
				saw_dot = true;
			} else if (!saw_dot) {
				filename[i] = c;
			}
			i++;
		}
		lv_table_set_cell_value(tbl, r, 0, filename);
		lv_table_set_cell_align(tbl, r, 0, LV_LABEL_ALIGN_CENTER);
		lv_table_set_cell_crop(tbl, r, 0, true);
		r++;
	}

	// Handle special cases
	if (updating_file_list) {
		if (load_first_file || load_last_file) {
			if (load_first_file) {
				load_first_file = false;
				_update_selected_file_indication(0);
			} else {
				load_last_file = false;
				_update_selected_file_indication((uint16_t) (num_files-1));
				
				// Scroll to the end of the list
				lv_page_scroll_ver(page_tbl_file_scroll, -lv_obj_get_height(tbl_file_browse));
			}
			_request_image(selected_dir, selected_file);
			_update_image_panel_controls();
		}
	}
	if (scroll_middle_entry) {
		scroll_middle_entry = false;
		_scroll_table_to_index(tbl, middle_entry_row);
	}
}


void gui_panel_file_browser_files_action(int action)
{
	switch(action) {
		case GUIPN_FILE_BROWSER_FILES_ACT_PREV:
			if ((selected_file >= 0) && ((selected_dir != 0) || (selected_file != 0))) {
				if (selected_file > 0) {
					// Previous file in this directory
					_update_selected_file_indication((uint16_t) selected_file-1);
					_scroll_table(tbl_file_browse, SCROLL_DOWN);
					
					// Request the selected directory + filename image
					_request_image(selected_dir, selected_file);
					
					_update_image_panel_controls();
				} else {
					// Last file in previous directory - this is a special case as
					// we need to fetch a new file list and then select and display
					// the last entry
					//
					// Select previous directory
					_update_selected_dir_indication((uint16_t) selected_dir-1);
					_scroll_table(tbl_dir_browse, SCROLL_DOWN);
					
					// Set a flag indicating we should treat the loading of the file list specially
					load_last_file = true;
					
					// Request the file list
					_request_file_list(selected_dir);
				}
			}
			break;
			
		case GUIPN_FILE_BROWSER_FILES_ACT_NEXT:
			if ((selected_file >= 0) && ((selected_dir != (num_dirs - 1)) || (selected_file != (num_files-1)))) {
				if (selected_file < (num_files-1)) {
					// Next file in this directory
					_update_selected_file_indication((uint16_t) selected_file+1);
					_scroll_table(tbl_file_browse, SCROLL_UP);
					
					// Request the selected directory + filename image
					_request_image(selected_dir, selected_file);
					
					_update_image_panel_controls();
				} else {
					// First file in next directory - this is a special case as we need
					// to fetch a new file list and then select and display the first entry
					//
					// Select next directory
					_update_selected_dir_indication((uint16_t) selected_dir+1);
					_scroll_table(tbl_dir_browse, SCROLL_UP);
					
					// Set a flag indicating we should treat the loading of the file list specially
					load_first_file = true;
					
					// Request the file list
					_request_file_list(selected_dir);
				}
			}
			break;
			
		case GUIPN_FILE_BROWSER_FILES_ACT_DEL:
			if (selected_file >= 0) {
				gui_display_message_box(my_parent_page, "Delete selected file?", GUI_MSG_BOX_2_BTN, _cb_messagebox);
			} else if (selected_dir >= 0) {
				gui_display_message_box(my_parent_page, "Delete selected folder?", GUI_MSG_BOX_2_BTN, _cb_messagebox);
			}
			break;
	}
}



//
// Internal functions
//
static lv_obj_t* _create_table(lv_obj_t* my_page, lv_event_cb_t event_cb)
{
	lv_obj_t* tbl = lv_table_create(my_page, NULL);
	lv_page_glue_obj(tbl, true);
	lv_table_set_col_cnt(tbl, 1);
	lv_table_set_col_width(tbl, 0, GUIPN_FILE_BROWSER_FILES_TBL_W-10);
    lv_obj_set_pos(tbl, 0, 0);
    lv_obj_add_protect(tbl, LV_PROTECT_CLICK_FOCUS);
    lv_obj_set_style_local_text_font(tbl, LV_TABLE_PART_CELL1, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SMALL);
    lv_obj_set_style_local_text_font(tbl, LV_TABLE_PART_CELL2, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SMALL);
    lv_obj_set_style_local_bg_color(tbl, LV_TABLE_PART_CELL2, LV_STATE_DEFAULT, GUI_THEME_TBL_BG_COLOR);
    lv_obj_set_style_local_bg_opa(tbl, LV_TABLE_PART_CELL2, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_obj_set_style_local_border_width(tbl, LV_TABLE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_top(tbl, LV_TABLE_PART_CELL1, LV_STATE_DEFAULT, 10);
	lv_obj_set_style_local_pad_bottom(tbl, LV_TABLE_PART_CELL1, LV_STATE_DEFAULT, 10);
	lv_obj_set_style_local_pad_left(tbl, LV_TABLE_PART_CELL1, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_right(tbl, LV_TABLE_PART_CELL1, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_top(tbl, LV_TABLE_PART_CELL2, LV_STATE_DEFAULT, 10);
	lv_obj_set_style_local_pad_bottom(tbl, LV_TABLE_PART_CELL2, LV_STATE_DEFAULT, 10);
	lv_obj_set_style_local_pad_left(tbl, LV_TABLE_PART_CELL2, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_right(tbl, LV_TABLE_PART_CELL2, LV_STATE_DEFAULT, 0);
    lv_obj_set_event_cb(tbl, event_cb);
	
    return tbl;
}


static lv_obj_t* _destroy_table(lv_obj_t* page, lv_obj_t* tbl)
{
	if (page != NULL) {
		lv_page_clean(page);
	}
	if (tbl != NULL) {
		lv_obj_del(tbl);
	}
	
	return NULL;
}


static void _configure_sizes()
{
	uint16_t d;
	uint16_t table_w = GUIPN_FILE_BROWSER_FILES_TBL_W;
		
	// Configure the size of the panel
	lv_obj_set_size(my_panel, panel_w, panel_h);
	
	// Shrink tables if necessary to fit
	if (panel_w < (2*GUIPN_FILE_BROWSER_FILES_TBL_W)) {
		table_w = panel_w / 2;
	}
	
	// Center tables
	//
	// Set label and table dimensions
	d = panel_h - GUIPN_FILE_BROWSER_FILES_TBL_Y - GUIPN_FILE_BROWSER_FILES_PAD_B;
	lv_obj_set_width(lbl_tbl_dir, table_w);
	lv_obj_set_size(page_tbl_dir_scroll, table_w, d);
	lv_page_set_scrl_width(page_tbl_dir_scroll, table_w);
	lv_obj_set_width(lbl_tbl_file, table_w);
	lv_obj_set_size(page_tbl_file_scroll, table_w, d);
	lv_page_set_scrl_width(page_tbl_file_scroll, table_w);
	
	// Set label and table x offsets
	d = (panel_w - (2 * table_w) - GUIPN_FILE_BROWSER_FILES_PAD_I) / 2;
	lv_obj_set_x(lbl_tbl_dir, d);
	lv_obj_set_x(page_tbl_dir_scroll, d);
	lv_obj_set_x(lbl_tbl_file, panel_w - (d + table_w));
	lv_obj_set_x(page_tbl_file_scroll, panel_w - (d + table_w));
}


static void _initialize_screen_values()
{
	// Start out with no tables, ready to display directory list
	load_first_file = false;
	load_last_file = false;
	scroll_middle_entry = false;
	prev_card_present = -1;       // Forces correct status eval independent of card_present state
	num_dirs = 0;
	num_files = 0;
	selected_dir = -1;
	selected_file = -1;
	
	// No tables yet
	tbl_dir_browse = NULL;
	tbl_file_browse = NULL;
	
	// No task yet
	task_update = NULL;
}


static void _status_update_task(lv_task_t * task)
{
	if ((int) gui_state.card_present != prev_card_present) {
		prev_card_present = (int) gui_state.card_present;
		
		// Disable any displayed image (this updates correct message)
		gui_panel_file_browser_image_set_valid(false);
		
		selected_dir = -1;
		selected_file = -1;
		
		if (gui_state.card_present) {
			// Request list of directories to update
			_request_dir_list();
		} else {			
			// Delete any existing lists
			tbl_file_browse = _destroy_table(page_tbl_file_scroll, tbl_file_browse);
			num_files = 0;
			selected_file = -1;
			
			tbl_dir_browse = _destroy_table(page_tbl_dir_scroll, tbl_dir_browse);
			num_dirs = 0;
			selected_dir = -1;
			
			_update_image_panel_controls();
		}
	}
	
	// Slow down subsequent updates
	lv_task_set_period(task, GUIPN_FILE_BROWSER_FILES_POLL_MSEC);
}


static void _cb_tbl_dir(lv_obj_t * obj, lv_event_t event)
{
	uint16_t col;
	uint16_t row;
	
	if (event == LV_EVENT_CLICKED) {
		if (lv_table_get_pressed_cell(obj, &row, &col) == LV_RES_OK) {
			if (((int) row != prev_tbl_dir_row) && ((int) row < num_dirs)) {
				// Update the selected dir indications
				_update_selected_dir_indication(row);
				
				// Disable any displayed image
				gui_panel_file_browser_image_set_valid(false);
				
				// Request file list
				_request_file_list((int) row);
				
				_update_image_panel_controls();
			}
		}
	}
}


static void _cb_tbl_file(lv_obj_t * obj, lv_event_t event)
{
	uint16_t col;
	uint16_t row;
	
	if (event == LV_EVENT_CLICKED) {
		if (lv_table_get_pressed_cell(obj, &row, &col) == LV_RES_OK) {
			if (((int) row != prev_tbl_file_row) && ((int) row < num_files)) {
				// Update the selected file indications
				_update_selected_file_indication(row);
				
				// Request the selected directory + filename image
				_request_image(selected_dir, selected_file);
				
				_update_image_panel_controls();
			}
		}
	}
}


static void _cb_messagebox(int btn_id)
{
	if (btn_id == GUI_MSG_BOX_BTN_AFFIRM) {
		if (selected_file >= 0) {
			// Delete a single file
			if (num_files == 1) {
				// Only one file in this directory so delete the entire directory
				_delete_dir(selected_dir);
				
				// Empty the file list
				tbl_file_browse = _destroy_table(page_tbl_file_scroll, tbl_file_browse);
				num_files = 0;
				selected_file = -1;
				
				if (num_dirs == 1) {
					// Empty the directory list as we've deleted its only content
					tbl_dir_browse = _destroy_table(page_tbl_dir_scroll, tbl_dir_browse);
					num_dirs = 0;
					selected_dir = -1;
				} else {
					// Setup to navigate to where we left off when the new list is loaded
					scroll_middle_entry = true;
					if (selected_dir == (num_dirs - 1)) {
						// Deleting last dir, navigate to the 2nd to last
						middle_entry_row = selected_dir - 1;
					} else {
						// Deleting a dir before the last, navigate to the new dir in this location
						middle_entry_row = selected_dir;
					}
					
					// Request a new directory list
					_request_dir_list();
				}
			} else {
				// Multiple files in this directory so we can just delete the file
				_delete_file(selected_dir, selected_file);
				
				// Setup to navigate to where we left off when the new list is loaded
				scroll_middle_entry = true;
				if (selected_file == (num_files - 1)) {
					// Deleting last file, navigate to the 2nd to last
					middle_entry_row = selected_file - 1;
				} else {
					// Deleting a file before the last, navigate to the new file in this location
					middle_entry_row = selected_file;
				}
				
				// Request a new file list
				_request_file_list(selected_dir);
			}
		} else if (selected_dir >= 0) {
			// Delete a directory
			_delete_dir(selected_dir);
			
			// Empty the file list associated with this directory
			tbl_file_browse = _destroy_table(page_tbl_file_scroll, tbl_file_browse);
			num_files = 0;
			selected_file = -1;
			
			if (num_dirs == 1) {
				// Empty the directory list as we've deleted its only content
				tbl_dir_browse = _destroy_table(page_tbl_dir_scroll, tbl_dir_browse);
				num_dirs = 0;
				selected_dir = -1;
			} else {
				// Setup to navigate to where we left off when the new list is loaded
				scroll_middle_entry = true;
				if (selected_dir == (num_dirs - 1)) {
					// Deleting last dir, navigate to the 2nd to last
					middle_entry_row = selected_dir - 1;
				} else {
					// Deleting a dir before the last, navigate to the new dir in this location
					middle_entry_row = selected_dir;
				}
				
				// Request a new directory list
				_request_dir_list();
			}
		}
		
		// Remove any displayed image
		gui_panel_file_browser_image_set_valid(false);
		
		// Update controls
		_update_image_panel_controls();
	}
}


static void _request_dir_list()
{
	(void) cmd_send_int32(CMD_GET, CMD_FILE_CATALOG, -1);
	num_dirs = 0;
	selected_dir = -1;
	prev_tbl_dir_row = -1;
}


static void _request_file_list(int file_index)
{
	(void) cmd_send_int32(CMD_GET, CMD_FILE_CATALOG, (int32_t) file_index);
	num_files = 0;
	selected_file = -1;
	prev_tbl_file_row = -1;
}


static void _request_image(int dir_index, int file_index)
{
	(void) cmd_send_file_indicies(CMD_GET, CMD_FILE_GET_IMAGE, dir_index, file_index);
}


static void _delete_dir(int dir_index)
{
	(void) cmd_send_file_indicies(CMD_SET, CMD_FILE_DELETE, dir_index, -1);
}


static void _delete_file(int dir_index, int file_index)
{
	(void) cmd_send_file_indicies(CMD_SET, CMD_FILE_DELETE, dir_index, file_index);
}


static void _scroll_table(lv_obj_t* tbl, int dir)
{
	int16_t n;
	
	if (tbl == tbl_dir_browse) {
		if (num_dirs <= 1) {
			n = 0;
		} else {
			n = (lv_obj_get_height(tbl_dir_browse) - 29) / num_dirs;
		}
		lv_page_scroll_ver(page_tbl_dir_scroll, (dir == SCROLL_UP) ? -n : n);
	} else if (tbl == tbl_file_browse) {
		if (num_files <= 1) {
			n = 0;
		} else {
			n = (lv_obj_get_height(tbl_file_browse) - 29) / num_files;
		}
		lv_page_scroll_ver(page_tbl_file_scroll, (dir == SCROLL_UP) ? -n : n);
	}
}


static void _scroll_table_to_index(lv_obj_t* tbl, int index)
{
	int16_t n;
	
	if (tbl == tbl_dir_browse) {
		n = ((lv_obj_get_height(tbl_dir_browse) - 29) / num_dirs) * index;
		lv_page_scroll_ver(page_tbl_dir_scroll, -n);
	} else {
		n = ((lv_obj_get_height(tbl_file_browse) - 29) / num_files) * index;
		lv_page_scroll_ver(page_tbl_file_scroll, -n);
	}
}


static void _update_selected_dir_indication(uint16_t row)
{
	// De-highlight any previously selected cell
	if (prev_tbl_dir_row != -1) {
		lv_table_set_cell_type(tbl_dir_browse, (uint16_t) prev_tbl_dir_row, 0, 1);
	}
	
	// Highlight the selected cell
	lv_table_set_cell_type(tbl_dir_browse, row, 0, 2);
	prev_tbl_dir_row = row;
		
	// Note user has selected a directory
	selected_dir = row;
}


static void _update_selected_file_indication(uint16_t row)
{
	// De-highlight any previously selected cell
	if (prev_tbl_file_row != -1) {
		lv_table_set_cell_type(tbl_file_browse, (uint16_t) prev_tbl_file_row, 0, 1);
	}
	
	// Highlight the selected cell
	lv_table_set_cell_type(tbl_file_browse, row, 0, 2);
	prev_tbl_file_row = row;
	
	// Note user has selected a file
	selected_file = row;
}


static void _update_image_panel_controls()
{
	// Previous button is active if we have selected a file to view and we are
	// not displaying the first file of the first directory
	if ((selected_file < 0) || ((selected_dir == 0) && (selected_file == 0))) {
		gui_panel_file_browser_image_set_ctrl_en(GUIPN_FILE_BROWSER_IMAGE_PREV, false);
	} else {
		gui_panel_file_browser_image_set_ctrl_en(GUIPN_FILE_BROWSER_IMAGE_PREV, true);
	}
	
	// Next button is active if we have selected a file to view and we are not displaying
	// the last file of the last directory
	if ((selected_file < 0) || ((selected_dir == (num_dirs - 1)) && (selected_file == (num_files-1)))) {
		gui_panel_file_browser_image_set_ctrl_en(GUIPN_FILE_BROWSER_IMAGE_NEXT, false);
	} else {
		gui_panel_file_browser_image_set_ctrl_en(GUIPN_FILE_BROWSER_IMAGE_NEXT, true);
	}
	
	// Delete button is active if there is at least a selected directory
	gui_panel_file_browser_image_set_ctrl_en(GUIPN_FILE_BROWSER_IMAGE_DEL, (selected_dir >= 0));
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
