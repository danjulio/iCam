/*
 * GUI system set time/date sub-page
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
#include "gui_sub_page_time.h"
#include "gui_page_settings.h"
#include <sys/time.h>

#ifdef ESP_PLATFORM
	#include "gui_task.h"
#else
	#include "gui_main.h"
#endif


// This must match code below and in cmd handlers
#define CMD_TIME_LEN       36


//
// Local constants
//
#define TIMESET_I_HOUR_H  0
#define TIMESET_I_HOUR_L  1
#define TIMESET_I_MIN_H   2
#define TIMESET_I_MIN_L   3
#define TIMESET_I_SEC_H   4
#define TIMESET_I_SEC_L   5
#define TIMESET_I_YEAR_TH 6
#define TIMESET_I_YEAR_H  7
#define TIMESET_I_YEAR_TE 8
#define TIMESET_I_YEAR_U  9
#define TIMESET_I_MON_H  10
#define TIMESET_I_MON_L  11
#define TIMESET_I_DAY_H  12
#define TIMESET_I_DAY_L  13

#define TIMESET_NUM_I    14


// Macro to convert a single-digit numeric value (0-9) to an ASCII digit ('0' - '9')
#define ASC_DIGIT(n)     ('0' + n)

// Button map indicies
#define BTNM_MAP_1       0
#define BTNM_MAP_2       1
#define BTNM_MAP_3       2
#define BTNM_MAP_4       3
#define BTNM_MAP_5       4
#define BTNM_MAP_6       5
#define BTNM_MAP_7       6
#define BTNM_MAP_8       7
#define BTNM_MAP_9       8
#define BTNM_MAP_LEFT    9
#define BTNM_MAP_10      10
#define BTNM_MAP_RIGHT   11



//
// Local variables
//

// State
static bool my_page_active = false;

// LVGL objects
static lv_obj_t* my_page;
static lv_obj_t* btn_back;
static lv_obj_t* lbl_btn_back;
static lv_obj_t* lbl_title;
static lv_obj_t* page_controls;
static lv_obj_t* page_controls_scrollable;
static lv_obj_t* lbl_time_set;
static lv_obj_t* btn_set_time_keypad;

#ifndef ESP_PLATFORM
static lv_obj_t* btn_auto_set;
static lv_obj_t* lbl_btn_auto_set;
#endif

// Time set state
static struct tm original_time_value;
static struct tm timeset_value;
static int timeset_index;
static char timeset_string[32];     // "HH:MM:SS YYYY-MM-DD" + room for #FFFFFF n# recolor string

// Keypad array
static const char* btnm_map[] = {
	"1", "2", "3", "\n",
	"4", "5", "6", "\n",
	"7", "8", "9", "\n",
	LV_SYMBOL_LEFT, "0", LV_SYMBOL_RIGHT, ""
};

// Character values to prepend the set time/date string currently indexed character with
static const char recolor_array[8] = {'#', 'F', 'F', 'F', 'F', 'F', 'F', ' '};

// Days per month (not counting leap years) for validation (0-based index)
static const uint8_t days_per_month[]={31,28,31,30,31,30,31,31,30,31,30,31};



//
// Forward declarations for internal routines
//
static bool is_valid_digit_position(int i);
static void display_timeset_value();
static bool set_timeset_indexed_value(int n);
static void cb_btn_set_time_keypad(lv_obj_t * btn, lv_event_t event);
static void _cb_back_button(lv_obj_t* obj, lv_event_t event);
#ifndef ESP_PLATFORM
static void _cb_btn_autoset(lv_obj_t* obj, lv_event_t event);
#endif
void _send_time();



//
// API
//
lv_obj_t* gui_sub_page_time_init(lv_obj_t* screen)
{
	// Create the top-level container for the page
	my_page = lv_obj_create(screen, NULL);
	lv_obj_set_pos(my_page, 0, 0);
	lv_obj_set_click(my_page, false);
		
	// Add the top-level controls
	//
	// Back button (fixed location)
	btn_back = lv_btn_create(my_page, NULL);
	lv_obj_set_pos(btn_back, GUISP_TIME_BACK_X, GUISP_TIME_BACK_Y);
	lv_obj_set_size(btn_back, GUISP_TIME_BACK_W, GUISP_TIME_BACK_H);
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
	lv_obj_set_pos(lbl_title, GUISP_TIME_TITLE_X, GUISP_TIME_TITLE_Y);
	lv_label_set_static_text(lbl_title, "Set Time/Date");
		
	// Control page (dynamic size)
	page_controls = lv_page_create(my_page, NULL);
	lv_page_set_scrollable_fit2(page_controls, LV_FIT_PARENT, LV_FIT_TIGHT);
	lv_page_set_scrl_layout(page_controls, LV_LAYOUT_COLUMN_MID);
	lv_obj_set_auto_realign(page_controls, true);
	lv_obj_align_origo(page_controls, NULL, LV_ALIGN_CENTER, 0, GUISP_TIME_CONTROL_Y);
	lv_obj_set_style_local_pad_top(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_TOP_PAD);
	lv_obj_set_style_local_pad_bottom(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_BTM_PAD);
	lv_obj_set_style_local_pad_left(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_LEFT_PAD);
	lv_obj_set_style_local_pad_right(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_RIGHT_PAD);
	lv_obj_set_style_local_pad_inner(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_INNER_PAD);
	lv_obj_set_style_local_border_width(page_controls, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
		
	page_controls_scrollable = lv_page_get_scrollable(page_controls);
	lv_obj_add_protect(lv_page_get_scrollable(page_controls), LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_style_local_pad_inner(page_controls_scrollable, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 2*GUIP_SETTINGS_INNER_PAD);
	
	// Time/Date string - centered with dynamic width
	// Default color is a dimmer white so the currently selected digit can stand out
	lbl_time_set = lv_label_create(page_controls_scrollable, NULL);
	lv_label_set_recolor(lbl_time_set, true);
	lv_obj_set_style_local_text_font(lbl_time_set, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
	lv_obj_set_style_local_text_color(lbl_time_set, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(0xB0, 0xB0, 0xB0));
	lv_label_set_long_mode(lbl_time_set, LV_LABEL_LONG_CROP);
	lv_label_set_align(lbl_time_set, LV_LABEL_ALIGN_CENTER);
	
	// Time set button matrix - centered
	btn_set_time_keypad = lv_btnmatrix_create(page_controls_scrollable, NULL);
	lv_btnmatrix_set_map(btn_set_time_keypad, btnm_map);
	lv_obj_set_width(btn_set_time_keypad, GUISP_TIME_KEYPAD_W);
	lv_obj_set_height(btn_set_time_keypad, GUISP_TIME_KEYPAD_H);
	lv_obj_add_protect(btn_set_time_keypad, LV_PROTECT_CLICK_FOCUS);
	lv_btnmatrix_set_btn_ctrl_all(btn_set_time_keypad, LV_BTNMATRIX_CTRL_NO_REPEAT);
	lv_btnmatrix_set_btn_ctrl_all(btn_set_time_keypad, LV_BTNMATRIX_CTRL_CLICK_TRIG);
	lv_obj_set_event_cb(btn_set_time_keypad, cb_btn_set_time_keypad);

#ifndef ESP_PLATFORM	
	// Autoset button - centered - only displayed if functional
	btn_auto_set = lv_btn_create(page_controls_scrollable, NULL);
	lv_obj_set_size(btn_auto_set, GUISP_TIME_AUTOSET_W, GUISP_TIME_AUTOSET_H);
	lv_obj_add_protect(btn_auto_set, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_event_cb(btn_auto_set, _cb_btn_autoset);
	
	// Button Label
	lbl_btn_auto_set = lv_label_create(btn_auto_set, NULL);
	lv_label_set_align(lbl_btn_auto_set, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_btn_auto_set, "Autoset");
#endif

	// We start off disabled
	lv_obj_set_hidden(my_page, true);
	
	return my_page;
}


void gui_sub_page_time_set_active(bool is_active)
{
	if (is_active) {
		// Request system time
		(void) cmd_send(CMD_GET, CMD_TIME);
	}
	
	// Set our visibility
	my_page_active = is_active;
	lv_obj_set_hidden(my_page, !is_active);
}


void gui_sub_page_time_reset_screen_size(uint16_t page_w, uint16_t page_h)
{
	// Set page dimensions
	lv_obj_set_size(my_page, page_w, page_h);
	
	// Always start off just to the right of the calling page when disabled
	if (!my_page_active) {
		lv_obj_set_pos(my_page, page_w, 0);
	}
	
	// Set title max width
	lv_obj_set_width(lbl_title, page_w - (2 * GUISP_TIME_TITLE_X));
		
	// Set control page dimensions
	lv_obj_set_size(page_controls, page_w, page_h - GUISP_TIME_CONTROL_Y - (4*GUIP_SETTINGS_INNER_PAD));
	lv_page_set_scrl_width(page_controls, page_w);
	
	// Set Time/Date string width
	lv_obj_set_width(lbl_time_set, page_w - (2 * GUISP_TIME_SETSTR_X));
}


void gui_sub_page_time_set_time(struct tm* cur_time)
{
	// Initialize the selection index to the first digit
	timeset_index = 0;
		
	// Update the time set label
	original_time_value = *cur_time;
	timeset_value = *cur_time;
	display_timeset_value();
}



//
// Internal functions
//

/**
 * Returns true when the passed in index is pointing to a valid digit position and
 * not a separator character
 */
static bool is_valid_digit_position(int i)
{
	//       H  H  :  M  M  :  S  S     Y  Y  Y  Y  -  M  M  -  D  D
	// i     0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18
	return (!((i==2) || (i==5) || (i==8) || (i==13) || (i==16)));
}


/**
 * Update the set time/date label.  The current indexed digit is made to be full
 * white to indicate it is the one being changed.
 */
static void display_timeset_value()
{
	int timeset_string_index = 0;  // Current timeset_string insertion point
	int time_string_index = 0;     // Current position in displayed "HH:MM:SS MM/DD/YY"
	int time_digit_index = 0;      // Current time digit index (0-11) for HHMMSSMMDDYY
	int i;
	bool did_recolor;

	while (time_string_index <= 18) {

		// Insert the recolor string before the currently selected time digit
		if ((timeset_index == time_digit_index) && is_valid_digit_position(time_string_index)) {
			for (i=0; i<8; i++) {
				timeset_string[timeset_string_index++] = recolor_array[i];
			}
			did_recolor = true;
		} else {
			did_recolor = false;
		}
		
		// Insert the appropriate time character
		//
		//                          H  H  :  M  M  :  S  S     Y  Y  Y  Y  -  M  M  -  D  D
		// time_string_index        0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18
		// time_digit_index         0  1     2  3     4  5     6  7  8  9     10 11    12 13
		//
		switch (time_string_index++) {
			case 0: // Hours tens
				timeset_string[timeset_string_index++] = ASC_DIGIT(timeset_value.tm_hour / 10);
				time_digit_index++;
				break;
			case 1: // Hours units
				timeset_string[timeset_string_index++] = ASC_DIGIT(timeset_value.tm_hour % 10);
				time_digit_index++;
				break;
			case 3: // Minutes tens
				timeset_string[timeset_string_index++] = ASC_DIGIT(timeset_value.tm_min / 10);
				time_digit_index++;
				break;
			case 4: // Minutes units
				timeset_string[timeset_string_index++] = ASC_DIGIT(timeset_value.tm_min % 10);
				time_digit_index++;
				break;
			case 6: // Seconds tens
				timeset_string[timeset_string_index++] = ASC_DIGIT(timeset_value.tm_sec / 10);
				time_digit_index++;
				break;
			case 7: // Seconds units
				timeset_string[timeset_string_index++] = ASC_DIGIT(timeset_value.tm_sec % 10);
				time_digit_index++;
				break;
			case 9:  // Years thousands - starts at 1900
				timeset_string[timeset_string_index++] = ASC_DIGIT((1900 + timeset_value.tm_year) / 1000);
				time_digit_index++;
				break;
			case 10: // Years hundreds
				timeset_string[timeset_string_index++] = ASC_DIGIT(((1900 + timeset_value.tm_year) % 1000) / 100);
				time_digit_index++;
				break;
			case 11: // Year tens
				timeset_string[timeset_string_index++] = ASC_DIGIT((timeset_value.tm_year % 100) / 10);
				time_digit_index++;
				break;
			case 12: // Year units
				timeset_string[timeset_string_index++] = ASC_DIGIT(timeset_value.tm_year % 10);
				time_digit_index++;
				break;
			case 14: // Month tens
				timeset_string[timeset_string_index++] = ASC_DIGIT((timeset_value.tm_mon+1) / 10);
				time_digit_index++;
				break;
			case 15: // Month units
				timeset_string[timeset_string_index++] = ASC_DIGIT((timeset_value.tm_mon+1) % 10);
				time_digit_index++;
				break;
			case 17: // Day tens
				timeset_string[timeset_string_index++] = ASC_DIGIT(timeset_value.tm_mday / 10);
				time_digit_index++;
				break;
			case 18: // Day units
				timeset_string[timeset_string_index++] = ASC_DIGIT(timeset_value.tm_mday % 10);
				time_digit_index++;
				break;
			
			case 2: // Time section separators
			case 5:
				if (did_recolor) {
					// End recoloring before we insert this character
					timeset_string[timeset_string_index++] = '#';
					did_recolor = false;
				}
				timeset_string[timeset_string_index++] = ':';
				break;
				
			case 8: // Time / Date separator
				if (did_recolor) {
					// End recoloring before we insert this character
					timeset_string[timeset_string_index++] = '#';
					did_recolor = false;
				}
				timeset_string[timeset_string_index++] = ' ';
				break;
				
			case 13: // Date section separators
			case 16:
				if (did_recolor) {
					// End recoloring before we insert this character
					timeset_string[timeset_string_index++] = '#';
					did_recolor = false;
				}
				timeset_string[timeset_string_index++] = '-';
				break;
		}
		
		if (did_recolor) {
			// End the recoloring after the digit
			timeset_string[timeset_string_index++] = '#';
			did_recolor = false;
		}
	}
	
	// Make sure the string is terminated
	timeset_string[timeset_string_index] = 0;
	
	lv_label_set_static_text(lbl_time_set, timeset_string);
}


/**
 * Apply the button press value n to the timeset_value, making sure that only
 * valid values are allowed for each digit position (for example, you cannot set
 * an hour value > 23).  Return true if the digit position was updated, false if it
 * was not changed.
 */
static bool set_timeset_indexed_value(int n)
{
	bool changed = false;
	uint8_t u8;
	
	switch (timeset_index) {
		case TIMESET_I_HOUR_H:
			if (n < 3) {
				timeset_value.tm_hour = (n * 10) + (timeset_value.tm_hour % 10);
				changed = true;
			}
			break;
		case TIMESET_I_HOUR_L:
			if (timeset_value.tm_hour >= 20) {
				// Only allow 20 - 23
				if (n < 4) {
					timeset_value.tm_hour = ((timeset_value.tm_hour / 10) * 10) + n;
					changed = true;
				}
			} else {
				// Allow 00-09 or 10-19
				timeset_value.tm_hour = ((timeset_value.tm_hour / 10) * 10) + n;
				changed = true;
			}
			break;
		case TIMESET_I_MIN_H:
			if (n < 6) {
				timeset_value.tm_min = (n * 10) + (timeset_value.tm_min % 10);
				changed = true;
			}
			break;
		case TIMESET_I_MIN_L:
			timeset_value.tm_min = ((timeset_value.tm_min / 10) * 10) + n;
			changed = true;
			break;
		case TIMESET_I_SEC_H:
			if (n < 6) {
				timeset_value.tm_sec = (n * 10) + (timeset_value.tm_sec % 10);
				changed = true;
			}
			break;
		case TIMESET_I_SEC_L:
			timeset_value.tm_sec = ((timeset_value.tm_sec / 10) * 10) + n;
			changed = true;
			break;
		case TIMESET_I_YEAR_TH:
			// Allow 1 or 2 in thousands location
			if ((n == 1) || (n == 2)) {
				timeset_value.tm_year = ((n-1) * 100) + (timeset_value.tm_year % 100);
				changed = true;
			}
			break;
		case TIMESET_I_YEAR_H:
			// Allow only 9 if thousands is 1 and 0 if thousands is 2
			if ((n == 9) && (timeset_value.tm_year < 100)) {
				timeset_value.tm_year = timeset_value.tm_year % 100;
				changed = true;
			} else if ((n == 0) && (timeset_value.tm_year >= 100)) {
				timeset_value.tm_year = (timeset_value.tm_year % 100) + 100;
				changed = true;
			}
			break;
		case TIMESET_I_YEAR_TE:
			// Allow 0 - 9
			timeset_value.tm_year = ((timeset_value.tm_year / 100) * 100) + (n * 10) + (timeset_value.tm_year % 10);
			changed = true;
			break;
		case TIMESET_I_YEAR_U:
			// Allow 0 - 9
			timeset_value.tm_year = ((timeset_value.tm_year / 10) * 10) + n;
			changed = true;
			break;
		case TIMESET_I_MON_H:
			if (n < 2) {
				timeset_value.tm_mon = (n * 10) + ((timeset_value.tm_mon+1) % 10) - 1;
				changed = true;
			}
			break;
		case TIMESET_I_MON_L:
			if (timeset_value.tm_mon >= 9) {
				// Only allow 10-12
				if (n < 3) {
					timeset_value.tm_mon = (((timeset_value.tm_mon+1) / 10) * 10) + n - 1;
					changed = true;
				}
			} else {
				// Allow 1-9
				if (n > 0) {
					timeset_value.tm_mon = (((timeset_value.tm_mon+1) / 10) * 10) + n - 1;
					changed = true;
				}
			}
			break;
		case TIMESET_I_DAY_H:
			u8 = days_per_month[timeset_value.tm_mon];
			if (n <= (u8 / 10)) {
				// Only allow valid tens digit for this month (will be 2 or 3)
				timeset_value.tm_mday = (n * 10) + (timeset_value.tm_mday % 10);
				changed = true;
			}
			break;
		case TIMESET_I_DAY_L:
			u8 = days_per_month[timeset_value.tm_mon];
			if ((timeset_value.tm_mday / 10) == (u8 / 10)) {
				if (n <= (u8 % 10)) {
					// Only allow valid units digits when the tens digit is the highest
					timeset_value.tm_mday = ((timeset_value.tm_mday / 10) * 10) + n;
					changed = true;
				}
			} else {
				// Units values of 0-9 are valid when the tens is lower than the highest
				timeset_value.tm_mday = ((timeset_value.tm_mday / 10) * 10) + n;
				changed = true;
			}
			break;
	}

	return changed;
}


static void cb_btn_set_time_keypad(lv_obj_t * btn, lv_event_t event)
{
	int button_val = -1;
	
	if (event == LV_EVENT_VALUE_CHANGED) {

		uint16_t n = lv_btnmatrix_get_active_btn(btn);
	
		if (n == BTNM_MAP_LEFT) {
			// Decrement to the previous digit
			if (timeset_index > TIMESET_I_HOUR_H) {
				timeset_index--;
			}
			display_timeset_value();
		} else if (n == BTNM_MAP_RIGHT) {
			// Increment to the next digit
			if (timeset_index < TIMESET_I_DAY_L) {
				timeset_index++;
			}
			display_timeset_value();
		} else if (n <= BTNM_MAP_10) {
			// Number button
			if (n == BTNM_MAP_10) {
				// Handle '0' specially
				button_val = 0;
			} else {
				// All other numeric buttons are base-0
				button_val = n + 1;
			}

			// Update the indexed digit based on the button value
			if (set_timeset_indexed_value(button_val)) {
				// Increment to next digit if the digit was changed
				if (timeset_index < TIMESET_I_DAY_L) {
					timeset_index++;
				}
			}
			
			// Update the display
			display_timeset_value();
		}
	}
}


static void _cb_back_button(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		// Look for a time change from entry
		time_t orig_time = mktime(&original_time_value);
		time_t new_time = mktime(&timeset_value);
		if (new_time != orig_time) {
			// Set new time
			_send_time();
		}
		
		gui_page_settings_close_sub_page(my_page);
	}
}


#ifndef ESP_PLATFORM
static void _cb_btn_autoset(lv_obj_t* obj, lv_event_t event)
{
	struct timeval tv;
	struct tm te;
	time_t t;
	
	if (event == LV_EVENT_CLICKED) {
		// Get the time from the browser and set our time to that
		gettimeofday(&tv, NULL);
		t = tv.tv_sec + (tv.tv_usec > 500000 ? 1 : 0);
		localtime_r(&t, &timeset_value);
		_send_time();
		display_timeset_value();
		original_time_value = timeset_value;  // Reset this since we just updated time
	}
}
#endif


void _send_time()
{
	uint8_t buf[CMD_TIME_LEN];
	
	// Pack the byte array - the set handler must unpack in the same order
	*(uint32_t*)&buf[0] = htonl((uint32_t) timeset_value.tm_sec);
	*(uint32_t*)&buf[4] = htonl((uint32_t) timeset_value.tm_min);
	*(uint32_t*)&buf[8] = htonl((uint32_t) timeset_value.tm_hour);
	*(uint32_t*)&buf[12] = htonl((uint32_t) timeset_value.tm_mday);
	*(uint32_t*)&buf[16] = htonl((uint32_t) timeset_value.tm_mon);
	*(uint32_t*)&buf[20] = htonl((uint32_t) timeset_value.tm_year);
	*(uint32_t*)&buf[24] = htonl((uint32_t) timeset_value.tm_wday);
	*(uint32_t*)&buf[28] = htonl((uint32_t) timeset_value.tm_yday);
	*(uint32_t*)&buf[32] = htonl((uint32_t) timeset_value.tm_isdst);
	
	(void) cmd_send_binary(CMD_SET, CMD_TIME, CMD_TIME_LEN, buf);
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
