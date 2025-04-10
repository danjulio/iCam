/*
 * Video Task
 *
 * Initialize video library and render Tiny1C data info a frame buffer
 * for PAL or NTSC video output.  Manage text-based user-interface.
 *
 * Copyright 2023-2024 Dan Julio
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
#include "esp_system.h"
#ifdef CONFIG_BUILD_ICAM_MINI

#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ctrl_task.h"
#include "file_task.h"
#include "out_state_utilities.h"
#include "palettes.h"
#include "system_config.h"
#include "sys_utilities.h"
#include "t1c_task.h"
#include "tiny1c.h"
#include "video_task.h"
#include "vid_render.h"
#include "video.h"



//
// VID Task constants
//
#define HEADER_PIXEL(data,pixel) {\
pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
data += 4; \
}

// Undefine to toggle diagnostic output high while rendering
//#define INCLUDE_VID_DIAG_OUTPUT

// Parameter display mode
#define PARM_DISP_NONE          0
#define PARM_DISP_FILE          1
#define PARM_DISP_PARM          2
#define PARM_DISP_TIMELAPSE_MSG 3

// Maximum parameter string length (must fit on display)
#define PARM_DISP_MAX_LEN       40


//
// VID Task Parameter setting related
//  (same order as parm_entries[] below)
//
#define PARM_INDEX_VID_PALETTE  0
#define PARM_INDEX_SPOT         1
#define PARM_INDEX_MIN_MAX      2
#define PARM_INDEX_FFC          3
#define PARM_INDEX_BRIGHTNESS   4
#define PARM_INDEX_EMISSIVITY   5
#define PARM_INDEX_GAIN         6
#define PARM_INDEX_ENV_CORRECT  7
#define PARM_INDEX_SAV_OVERLAY  8
#define PARM_INDEX_SAV_PALETTE  9
#define PARM_INDEX_TL_EN        10
#define PARM_INDEX_TL_INTERVAL  11
#define PARM_INDEX_TL_IMAGES    12
#define PARM_INDEX_TL_NOTIFY    13
#define PARM_INDEX_UNITS        14
#define PARM_INDEX_VID_MODE     15

#define NUM_PARMS               16

// Timeout from non-default parameter selection
//  Must be longer than button long-press
#define PARM_ENTRY_TIMEOUT_MSEC (CTRL_BTN_LONG_PRESS_MSEC + 7000)

// Parameter management struct
typedef struct {
	const int num_parms;
	const char* parm_name;
	const int* parm_values;
} parm_entry_t;



//
// VID Task variables
//
static const char* TAG = "vid_task";

// Parameters
//
// On/Off value and string used by multiple parameters
static const int parm_on_off_value[] = {0, 1};
static const char* parm_on_off_name[] = {"Off", "On"};

// Video Palette related
#define NUM_VP_PARM_VALS 2
static const char* parm_vp_name[] = {"White Hot", "Black Hot"};
static const parm_entry_t parm_vp_entry = {NUM_VP_PARM_VALS, "Vid Palette: ", parm_on_off_value};

// Save Palette related (note this must match palette.h - yes, I know, this is ugly.  A result of wanting the 'const')
#define NUM_SP_PARM_VALS 10
static const int parm_sp_value[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const char* parm_sp_name[] = {"White Hot", "Black Hot", "Artic", "Fusion", "Iron Black", "Rainbow", "Double Rainbow", "Sepia", "Banded", "IsoTherm"};
static const parm_entry_t parm_sp_entry = {NUM_SP_PARM_VALS, "Save Palette: ", parm_sp_value};

// Spot meter related
#define NUM_SM_PARM_VALS 2
static const parm_entry_t parm_sm_entry = {NUM_SM_PARM_VALS, "Spot: ", parm_on_off_value};

// Min/Max marker enable related
#define NUM_MM_PARM_VALS 2
static const parm_entry_t parm_mm_entry = {NUM_MM_PARM_VALS, "Min/Max Markers: ", parm_on_off_value};

// Brightness Parameter related
#define NUM_B_PARM_VALS 10
static const int parm_b_value[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static const char* parm_b_name[] = {"10", "20", "30", "40", "50", "60", "70", "80", "90", "100"};
static const parm_entry_t parm_b_entry = {NUM_B_PARM_VALS, "Brightness: ", parm_b_value};

// Emissivity Parameter related
#define NUM_E_PARM_VALS 24
static const int parm_e_value[] = {5, 10, 20, 30, 40, 50, 60, 70, 80, 82, 84, 86, 88,
                                   90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const char* parm_e_name[] = {"5", "10", "20", "30", "40", "50", "60", "70", "80", "82", "84", "86", "88",
                                    "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "100"};
static const parm_entry_t parm_e_entry = {NUM_E_PARM_VALS, "Emissivity: ", parm_e_value};

// Gain Parameter related
#define NUM_G_PARM_VALS 2
static const char* parm_g_name[] = {"Low", "High"};
static const parm_entry_t parm_g_entry = {NUM_G_PARM_VALS, "Gain: ", parm_on_off_value};

// FFC related
#define NUM_FFC_PARM_VALS 1
static const int parm_ffc_value[] = {0};
static const char* parm_ffc_name[] = {"Trig"};
static const parm_entry_t parm_ffc_entry = {NUM_FFC_PARM_VALS, "FFC: ", parm_ffc_value};

// Environmental Correction related
#define NUM_EC_PARM_VALS 2
static const parm_entry_t parm_ec_entry = {NUM_EC_PARM_VALS, "Env Correct: ", parm_on_off_value};

// Save Overlay related
#define NUM_SO_PARM_VALS 2
static const parm_entry_t parm_so_entry = {NUM_SO_PARM_VALS, "Save Overlay: ", parm_on_off_value};

// Timelapse related
#define NUM_TL1_PARM_VALS 2
static const parm_entry_t parm_tl1_entry = {NUM_TL1_PARM_VALS, "Timelapse: ", parm_on_off_value};

#define NUM_TL2_PARM_VALS 12
static const int parm_tl2_value[] = {2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600};
static const char* parm_tl2_name[] = {"2s", "5s", "10s", "15s", "30s", "1m", "2m", "5m", "10m", "15m", "30m", "1hr"};
static const parm_entry_t parm_tl2_entry = {NUM_TL2_PARM_VALS, "Timelapse Interval: ", parm_tl2_value};

#define NUM_TL3_PARM_VALS 10
static const int parm_tl3_value[] = {10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000};
static const char* parm_tl3_name[] = {"10", "25", "50", "100", "250", "500", "1000", "2500", "5000", "10000"};
static const parm_entry_t parm_tl3_entry = {NUM_TL3_PARM_VALS, "Timelapse Images: ", parm_tl3_value};

#define NUM_TL4_PARM_VALS 2
static const parm_entry_t parm_tl4_entry = {NUM_TL4_PARM_VALS, "Timelapse Notify: ", parm_on_off_value};

// Temperature Display Units related
//   0 : Imperial - F
//   1 : Metric - C
#define NUM_U_PARM_VALS 2
static const char* parm_u_name[] = {"Imperial", "Metric"};
static const parm_entry_t parm_u_entry = {NUM_U_PARM_VALS, "Units: ", parm_on_off_value};

// Video Mode related
#define NUM_VM_PARM_VALS 2
static const char* parm_vm_name[] = {"NTSC", "PAL"};
static const parm_entry_t parm_vm_entry = {NUM_VM_PARM_VALS, "Video: ", parm_on_off_value};

// Parameter management array
static const parm_entry_t* parm_entries[] = {
	&parm_vp_entry,
	&parm_sm_entry,
	&parm_mm_entry,
	&parm_ffc_entry,
	&parm_b_entry,
	&parm_e_entry,
	&parm_g_entry,
	&parm_ec_entry,
	&parm_so_entry,
	&parm_sp_entry,
	&parm_tl1_entry,
	&parm_tl2_entry,
	&parm_tl3_entry,
	&parm_tl4_entry,
	&parm_u_entry,
	&parm_vm_entry
};


// Notifications (clear after use)
static bool notify_image_1 = false;
static bool notify_image_2 = false;
static bool notify_b1_short_press = false;
static bool notify_b2_short_press = false;
static bool notify_b2_long_press = false;
static bool notify_disp_file_message = false;
static bool notify_clear_file_message = false;
static bool notify_disp_tl_on_message = false;
static bool notify_disp_tl_off_message = false;

// Battery state
static bool notify_crit_batt = false;
static int batt_percent = 0;

// Timelapse state
static bool timelapse_running = false;
static bool timelapse_enable = false;
static bool timelapse_notify = false;
static bool timelapse_display = false;
static uint32_t timelapse_interval_sec = parm_tl2_value[0];
static uint32_t timelapse_num_img = parm_tl3_value[0];
static int64_t timelapse_toggle_usec;
	
// Video Driver Frame buffer pointer
static uint8_t* drv_fbP;

// Parameter selection and modification
static char parm_string[PARM_DISP_MAX_LEN+1];
static int parm_disp_state = PARM_DISP_NONE;
static int cur_parm_index;
static int cur_parm_value_index;



//
// VID Task Forward Declarations for internal functions
//
static bool _vid_init_output();
static void _vid_handle_notifications();
static void _vid_eval_batt_update();
static void _vid_eval_parm_update();
static void _vid_get_parm_value_index();
static void _vid_update_val_from_parm();
static void _vid_set_parm_string();
static void _vid_render_testpattern();
static void _vid_render_palette();
static void _vid_render_image(int render_buf_index);
static void _vid_display_image(int render_buf_index);
static int _vid_get_parm_index(int cur_val, const int* values, int num_values);



//
// VID Task API
//
void vid_task()
{
	ESP_LOGI(TAG, "Start task");
	
	// Configure the save palette
	set_save_palette(out_state.sav_palette_index);
	
	// We are always a landscape display
	out_state.is_portrait = false;
	
	// Allocate our frame buffer
	drv_fbP = (uint8_t*) heap_caps_calloc(IMG_BUF_WIDTH*IMG_BUF_HEIGHT, sizeof(uint8_t), MALLOC_CAP_8BIT);
	if (drv_fbP == NULL) {
		ESP_LOGE(TAG, "Could not allocate frame buffer");
		ctrl_set_fault_type(CTRL_FAULT_VIDEO);
		vTaskDelete(NULL);
	}
	
	// Initialize the output stream
	if (!_vid_init_output()) {
		vTaskDelete(NULL);
	}
	
	// Render the test image
	_vid_render_testpattern();
	
	// Delay before starting video display
	vTaskDelay(pdMS_TO_TICKS(VID_SPLASH_DISPLAY_MSEC));
	
	// Draw the current palette
	_vid_render_palette();
	
	while (1) {
		_vid_handle_notifications();
		_vid_eval_batt_update();
		_vid_eval_parm_update();
		
		if (notify_image_1) {
			notify_image_1 = false;
			_vid_render_image(1);
		}
		
		if (notify_image_2) {
			notify_image_2 = false;
			_vid_render_image(0);
		}
		
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}



//
// VID Task Internal functions
//
static bool _vid_init_output()
{
	if (out_state.output_mode_PAL) {
		if (!video_init(IMG_BUF_WIDTH, IMG_BUF_HEIGHT, drv_fbP, VIDEO_MODE_PAL)) {
			ESP_LOGE(TAG, "PAL video init failed");
			ctrl_set_fault_type(CTRL_FAULT_VIDEO);
			return false;
		}
		ESP_LOGI(TAG, "Video Mode: PAL");
	} else {
		if (!video_init(IMG_BUF_WIDTH, IMG_BUF_HEIGHT, drv_fbP, VIDEO_MODE_NTSC)) {
			ESP_LOGE(TAG, "NTSC video init failed");
			ctrl_set_fault_type(CTRL_FAULT_VIDEO);
			return false;
		}
		ESP_LOGI(TAG, "Video Mode: NTSC");
	}
	
	return true;
}


static void _vid_handle_notifications()
{
	uint32_t notification_value;
	
	notification_value = 0;
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, VID_NOTIFY_T1C_FRAME_MASK_1)) {
			notify_image_1 = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_T1C_FRAME_MASK_2)) {
			notify_image_2 = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_B1_SHORT_PRESS_MASK)) {
			notify_b1_short_press = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_B2_SHORT_PRESS_MASK)) {
			notify_b2_short_press = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_B2_LONG_PRESS_MASK)) {
			notify_b2_long_press = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_CRIT_BATT_DET_MASK)) {
			notify_crit_batt = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_SHUTDOWN_MASK)) {
			// Disable video output as we're being switched off
			video_stop();
		}
		
		if (Notification(notification_value, VID_NOTIFY_FILE_MSG_ON_MASK)) {
			notify_disp_file_message = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_FILE_MSG_OFF_MASK)) {
			notify_clear_file_message = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_FILE_TIMELAPSE_ON_MASK)) {
			// Trigger display of a start message
			notify_disp_tl_on_message = true;
			
			// Setup to start toggling the battery display with timelapse indication
			timelapse_display = true;
			timelapse_running = true;
			timelapse_toggle_usec = esp_timer_get_time() + (VID_TIMELAPSE_TOGGLE_MSEC * 1000);
		}
		
		if (Notification(notification_value, VID_NOTIFY_FILE_TIMELAPSE_OFF_MASK)) {
			// Trigger display of a stop message
			notify_disp_tl_off_message = true;
			

			// End toggling and ensure battery display visible;
			timelapse_display = false;
			timelapse_running = false;
			
			// Disable timelapse (user must re-enable it again)
			timelapse_enable = false;
		}
	}
}


static void _vid_eval_batt_update()
{
	int64_t cur_usec;
	static int64_t prev_usec = 0;
	
	cur_usec = esp_timer_get_time();
	if (cur_usec > (prev_usec + VID_BATT_STATE_UPDATE_MSEC*1000)) {
		batt_percent = ctrl_get_batt_percent();
		prev_usec = cur_usec;
	}
	
	if (timelapse_running) {
		if (cur_usec > timelapse_toggle_usec) {
			timelapse_toggle_usec += VID_TIMELAPSE_TOGGLE_MSEC * 1000;
			timelapse_display = !timelapse_display;
		}
	}
}


static void _vid_eval_parm_update()
{
	int64_t cur_time;
	static int64_t prev_time = 0;
	
	switch (parm_disp_state) {
		case PARM_DISP_NONE:
			if (notify_b2_long_press) {
				// Enter parameter set mode
				parm_disp_state = PARM_DISP_PARM;
				cur_parm_index = 0;
				_vid_get_parm_value_index();
				_vid_set_parm_string();
				prev_time = esp_timer_get_time();
			} else if (notify_b1_short_press) {
				// Request a picture (or start timelapse)
				xTaskNotify(task_handle_file, FILE_NOTIFY_SAVE_JPG_MASK, eSetBits);
			} else if (notify_disp_file_message) {
				// Setup to display a message from file_task
				parm_disp_state = PARM_DISP_FILE;
				strncpy(parm_string, file_get_file_save_status_string(), PARM_DISP_MAX_LEN);
				parm_string[PARM_DISP_MAX_LEN] = 0;
			} else if (notify_disp_tl_on_message) {
				parm_disp_state = PARM_DISP_TIMELAPSE_MSG;
				strcpy(parm_string, "Timelapse Start");
				prev_time = esp_timer_get_time();
			} else if (notify_disp_tl_on_message) {
				parm_disp_state = PARM_DISP_TIMELAPSE_MSG;
				strcpy(parm_string, "Timelapse Stop");
				prev_time = esp_timer_get_time();
			}
			break;
			
		case PARM_DISP_FILE:
			if (notify_clear_file_message) {
				// Done displaying message
				parm_disp_state = PARM_DISP_NONE;
			}
			break;
		
		case PARM_DISP_PARM:
			if (notify_b1_short_press) {
				// Next parameter value
				if (++cur_parm_value_index >= parm_entries[cur_parm_index]->num_parms) cur_parm_value_index = 0;
				_vid_update_val_from_parm();
				_vid_set_parm_string();
				prev_time = esp_timer_get_time();
			}
			
			else if (notify_b2_short_press) {
				// Next parameter
				if (++cur_parm_index >= NUM_PARMS) cur_parm_index = 0;
				_vid_get_parm_value_index();
				_vid_set_parm_string();
				prev_time = esp_timer_get_time();
			}
			
			else if (notify_b2_long_press) {
				// Exit parameter set mode
				parm_disp_state = PARM_DISP_NONE;
				file_set_timelapse_info(timelapse_enable, timelapse_notify, timelapse_interval_sec, timelapse_num_img);
				out_state_save();
			}
			
			else {
				// Look for parameter entry timeout
				cur_time = esp_timer_get_time();
				if ((cur_time - prev_time) >= (PARM_ENTRY_TIMEOUT_MSEC * 1000)) {
					parm_disp_state = PARM_DISP_NONE;
					file_set_timelapse_info(timelapse_enable, timelapse_notify, timelapse_interval_sec, timelapse_num_img);
					out_state_save();
				}
			}
			break;
		
		case PARM_DISP_TIMELAPSE_MSG:
			// Look for message timeout
			cur_time = esp_timer_get_time();
			if ((cur_time - prev_time) >= (VID_TIMELAPSE_MSG_MSEC * 1000)) {
				// Done displaying message
				parm_disp_state = PARM_DISP_NONE;
			}
			break;
	}
	
	// Clear out notifications each time we evaluate
	notify_b1_short_press = false;
	notify_b2_short_press = false;
	notify_b2_long_press = false;
	notify_disp_file_message = false;
	notify_clear_file_message = false;
	notify_disp_tl_on_message = false;
	notify_disp_tl_off_message = false;
}


static void _vid_get_parm_value_index() {
	switch (cur_parm_index) {
		case PARM_INDEX_VID_PALETTE:
			cur_parm_value_index = out_state.vid_palette_index;
			break;
		case PARM_INDEX_SPOT:
			cur_parm_value_index = out_state.spotmeter_enable ? 1 : 0;
			break;
		case PARM_INDEX_MIN_MAX:
			cur_parm_value_index = out_state.min_max_mrk_enable ? 1 : 0;
			break;
		case PARM_INDEX_FFC:
			cur_parm_value_index = 0;
			break;
		case PARM_INDEX_BRIGHTNESS:
			cur_parm_value_index = _vid_get_parm_index(out_state.brightness, parm_b_value, NUM_B_PARM_VALS);
			break;
		case PARM_INDEX_EMISSIVITY:
			cur_parm_value_index = _vid_get_parm_index(out_state.emissivity, parm_e_value, NUM_E_PARM_VALS);
			break;
		case PARM_INDEX_GAIN:
			cur_parm_value_index = out_state.high_gain ? 1 : 0;
			break;
		case PARM_INDEX_ENV_CORRECT:
			cur_parm_value_index = out_state.use_auto_ambient ? 1 : 0;
			break;
		case PARM_INDEX_SAV_OVERLAY:
			cur_parm_value_index = out_state.save_ovl_en ? 1 : 0;
			break;
		case PARM_INDEX_SAV_PALETTE:
			cur_parm_value_index = out_state.sav_palette_index;
			break;
		case PARM_INDEX_TL_EN:
			cur_parm_value_index = timelapse_enable ? 1 : 0;
			break;
		case PARM_INDEX_TL_INTERVAL:
			cur_parm_value_index = _vid_get_parm_index(timelapse_interval_sec, parm_tl2_value, NUM_TL2_PARM_VALS);
			break;
		case PARM_INDEX_TL_IMAGES:
			cur_parm_value_index = _vid_get_parm_index(timelapse_num_img, parm_tl3_value, NUM_TL3_PARM_VALS);
			break;
		case PARM_INDEX_TL_NOTIFY:
			cur_parm_value_index = timelapse_notify ? 1 : 0;
			break;
		case PARM_INDEX_UNITS:
			cur_parm_value_index = out_state.temp_unit_C ? 1 : 0;
			break;
		case PARM_INDEX_VID_MODE:
			cur_parm_value_index = out_state.output_mode_PAL ? 1 : 0;
			break;
	}
}


static void _vid_update_val_from_parm() {
	switch (cur_parm_index) {
		case PARM_INDEX_VID_PALETTE:
			out_state.vid_palette_index = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			_vid_render_palette();
			break;
		case PARM_INDEX_SPOT:
			out_state.spotmeter_enable = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			
			// Update t1c_task
			t1c_set_spot_enable(out_state.spotmeter_enable);
			
			if (!out_state.spotmeter_enable) {
				// Re-render CMAP area to get rid of the spot marker since we won't be rendering its area
				// anymore and wouldn't be able to erase the last one
				_vid_render_palette();
			}
			break;
		case PARM_INDEX_MIN_MAX:
			out_state.min_max_mrk_enable = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			
			// Update t1c_task
			t1c_set_minmax_marker_enable(out_state.min_max_mrk_enable);
			break;
		case PARM_INDEX_FFC:
			// Trigger a FFC
			xTaskNotify(task_handle_t1c, T1C_NOTIFY_FFC_MASK, eSetBits);
			break;
		case PARM_INDEX_BRIGHTNESS:
			out_state.brightness = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			
			// Update Tiny1C
			if (!t1c_set_param_image(IMAGE_PROP_LEVEL_BRIGHTNESS, brightness_to_param_value(out_state.brightness))) {
				ESP_LOGE(TAG, "Failed to set brightness");
			}
			break;
		case PARM_INDEX_EMISSIVITY:
			out_state.emissivity = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			
			// Update Tiny1C
			if (!t1c_set_param_tpd(TPD_PROP_EMS, emissivity_to_param_value(out_state.emissivity))) {
				ESP_LOGE(TAG, "Failed to set emissivity");
			}
			break;
		case PARM_INDEX_GAIN:
			out_state.high_gain = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			
			// Update Tiny1C
			if (!t1c_set_param_tpd(TPD_PROP_GAIN_SEL, out_state.high_gain ? 1 : 0)) {
				ESP_LOGE(TAG, "Failed to set gain");
			}
			break;
		case PARM_INDEX_ENV_CORRECT:
			out_state.use_auto_ambient = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			
			// Update Tiny1C
	    	xTaskNotify(task_handle_t1c, T1C_NOTIFY_ENV_UPD_MASK, eSetBits);
			break;
		case PARM_INDEX_SAV_OVERLAY:
			out_state.save_ovl_en = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			break;
		case PARM_INDEX_SAV_PALETTE:
			out_state.sav_palette_index = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			set_save_palette(out_state.sav_palette_index);
			break;
		case PARM_INDEX_TL_EN:
			timelapse_enable = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			break;
		case PARM_INDEX_TL_INTERVAL:
			timelapse_interval_sec = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			break;
		case PARM_INDEX_TL_IMAGES:
			timelapse_num_img = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			break;
		case PARM_INDEX_TL_NOTIFY:
			timelapse_notify = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			break;
		case PARM_INDEX_UNITS:
			out_state.temp_unit_C = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			break;
		case PARM_INDEX_VID_MODE:
			out_state.output_mode_PAL = parm_entries[cur_parm_index]->parm_values[cur_parm_value_index];
			
			// Update video output
			video_stop();
			if (!_vid_init_output()) {
				ctrl_set_fault_type(CTRL_FAULT_VIDEO);
			}
			break;
	}
}


static void _vid_set_parm_string() {
	const char* parm_value_str;
	
	switch (cur_parm_index) {
		case PARM_INDEX_VID_PALETTE:
			parm_value_str = parm_vp_name[cur_parm_value_index];
			break;
		case PARM_INDEX_SPOT:
			parm_value_str = parm_on_off_name[cur_parm_value_index];
			break;
		case PARM_INDEX_MIN_MAX:
			parm_value_str = parm_on_off_name[cur_parm_value_index];
			break;
		case PARM_INDEX_FFC:
			parm_value_str = parm_ffc_name[cur_parm_value_index];
			break;
		case PARM_INDEX_BRIGHTNESS:
			parm_value_str = parm_b_name[cur_parm_value_index];
			break;
		case PARM_INDEX_EMISSIVITY:
			parm_value_str = parm_e_name[cur_parm_value_index];
			break;
		case PARM_INDEX_GAIN:
			parm_value_str = parm_g_name[cur_parm_value_index];
			break;
		case PARM_INDEX_ENV_CORRECT:
			parm_value_str = parm_on_off_name[cur_parm_value_index];
			break;
		case PARM_INDEX_SAV_OVERLAY:
			parm_value_str = parm_on_off_name[cur_parm_value_index];
			break;
		case PARM_INDEX_SAV_PALETTE:
			parm_value_str = parm_sp_name[cur_parm_value_index];
			break;
		case PARM_INDEX_TL_EN:
			parm_value_str = parm_on_off_name[cur_parm_value_index];
			break;
		case PARM_INDEX_TL_INTERVAL:
			parm_value_str = parm_tl2_name[cur_parm_value_index];
			break;
		case PARM_INDEX_TL_IMAGES:
			parm_value_str = parm_tl3_name[cur_parm_value_index];
			break;
		case PARM_INDEX_TL_NOTIFY:
			parm_value_str = parm_on_off_name[cur_parm_value_index];
			break;
		case PARM_INDEX_UNITS:
			parm_value_str = parm_u_name[cur_parm_value_index];
			break;
		case PARM_INDEX_VID_MODE:
			parm_value_str = parm_vm_name[cur_parm_value_index];
			break;
		default:
			parm_value_str = 0;
	}
	
	sprintf(parm_string, "%s %s", parm_entries[cur_parm_index]->parm_name, parm_value_str);
}


static void _vid_render_testpattern()
{
	uint8_t* rendP = rend_fbP[0];
	
	vid_render_test_pattern(rendP);
	_vid_display_image(0);
}


static void _vid_render_palette()
{
	// Render the static part of the current palette into both buffers
	vid_render_palette(rend_fbP[0], &out_state);
	vid_render_palette(rend_fbP[1], &out_state);
}


static void _vid_render_image(int render_buf_index)
{
	t1c_buffer_t* t1cP = (render_buf_index == 0) ? &out_t1c_buffer[0] : &out_t1c_buffer[1];
	uint8_t* rendP = rend_fbP[render_buf_index];
	static bool halt_updates = false;
	
#ifdef INCLUDE_VID_DIAG_OUTPUT
	gpio_set_level(BRD_DIAG_IO, 1);
#endif
	
	if (t1cP->vid_frozen) {
		// Just render the video frozen marker over whatever image we're currently displaying
		if (!halt_updates) {
			xSemaphoreTake(t1cP->mutex, portMAX_DELAY);
			vid_render_freeze_marker(rendP);
			xSemaphoreGive(t1cP->mutex);
			_vid_display_image(render_buf_index);
		}
		halt_updates = true;
	} else {	
		halt_updates = false;
		
		xSemaphoreTake(t1cP->mutex, portMAX_DELAY);
		
		// Render the image into the frame buffer
		vid_render_t1c_data(t1cP, rendP, &out_state);
		
		// Render the battery or timelapse status
		if (timelapse_running && timelapse_display) {
			vid_render_timelapse_status(rendP);
		} else {
			vid_render_battery_info(rendP, batt_percent, notify_crit_batt);
		}
		
		// Render the current scene min/max temperature values
		vid_render_min_max_temps(t1cP, rendP, &out_state);
	
		// Render the min/max markers if enabled
		if (out_state.min_max_mrk_enable && t1cP->minmax_valid) {
			vid_render_min_max_markers(t1cP, rendP, &out_state);
		}
		
		// Render the region marker if enabled
		if (out_state.region_enable && t1cP->region_valid) {
			vid_render_region_marker(t1cP, rendP, &out_state);
			vid_render_region_temps(t1cP, rendP, &out_state);
		}
	
		// Render the spot meter if enabled
		if (out_state.spotmeter_enable && t1cP->spot_valid) {
			vid_render_spotmeter(t1cP, rendP, &out_state);
			vid_render_palette_marker(t1cP, rendP, &out_state);
		}
		
		// Render environmental conditions if they exist
		vid_render_env_info(t1cP, rendP, &out_state);
	
		if (parm_disp_state != PARM_DISP_NONE) {
			vid_render_parm_string(parm_string, rendP);
		}
		xSemaphoreGive(t1cP->mutex);
		
		_vid_display_image(render_buf_index);
	}
	
#ifdef INCLUDE_VID_DIAG_OUTPUT
	gpio_set_level(BRD_DIAG_IO, 0);
#endif
}


static void _vid_display_image(int render_buf_index)
{
	uint8_t* drvP = drv_fbP;
	uint8_t* rendP = rend_fbP[render_buf_index];

	memcpy(drvP, rendP, IMG_BUF_WIDTH*IMG_BUF_HEIGHT);
}


static int _vid_get_parm_index(int cur_val, const int* values, int num_values)
{
	// Look for the closest match
	for (int i=0; i<(num_values-1); i++) {
		if ((values[i] <= cur_val) && (values[i+1] > cur_val)) {
			return i;
		}
	}
	
	return (num_values - 1);
}

#endif /* CONFIG_BUILD_ICAM_MINI */
