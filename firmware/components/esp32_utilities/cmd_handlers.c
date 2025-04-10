/*
 * Command handlers updating or retrieving application state
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
#include <arpa/inet.h>
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cmd_handlers.h"
#include "cmd_utilities.h"
#include "falcon_cmd.h"
#include "file_task.h"
#include "out_state_utilities.h"
#include "palettes.h"
#include "ps_utilities.h"
#include "sys_info.h"
#include "sys_utilities.h"
#include "time_utilities.h"
#include "tiny1c.h"
#include "t1c_task.h"
#include <string.h>

#ifdef CONFIG_BUILD_ICAM_MINI
	#include "ctrl_task.h"
	#include "web_task.h"
#else
	#include "gcore_task.h"
	#include "gui_task.h"
#endif


//
// Constants
//

// These must match code below and in gui response handler and sender
#define CMD_AMBIENT_CORRECT_LEN 18
#define CMD_SHUTTER_INFO_LEN    13
#define CMD_TIME_LEN            36
#define CMD_TIMELAPSE_LEN       10
#define CMD_WIFI_INFO_LEN       (3 + 2*(PS_SSID_MAX_LEN+1) + 2*(PS_PW_MAX_LEN+1) + 3*4)



//
// Variables
//
static const char* TAG = "cmd_handlers";

// Notification state for our controlling task
static bool stream_enable_flag = false;
static bool notify_take_picture = false;

// Statically allocated big data structures used by functions below to save stack space
static uint8_t send_buf[CMD_WIFI_INFO_LEN];     // Sized for the largest packet type we send
static net_config_t orig_net_config;
static net_config_t new_net_config;
static t1c_config_t t1c_config;
static struct tm te;


//
// Forward declarations for internal functions
//
static bool net_config_structs_eq(net_config_t* s1, net_config_t* s2);



//
// API
//
void cmd_handler_get_ambient_correct(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	// Pack the byte array - the response handler must unpack in the same order
	send_buf[0] = (uint8_t) out_state.use_auto_ambient;
	send_buf[1] = (uint8_t) out_state.refl_equals_ambient;
	*(uint32_t*)&send_buf[2] = htonl((uint32_t) out_state.atmospheric_temp);
	*(uint32_t*)&send_buf[6] = htonl(out_state.distance);
	*(uint32_t*)&send_buf[10] = htonl(out_state.humidity);
	*(uint32_t*)&send_buf[14] = htonl((uint32_t) out_state.reflected_temp);
	
	if (!cmd_send_binary(CMD_RSP, CMD_AMBIENT_CORRECT, CMD_AMBIENT_CORRECT_LEN, send_buf)) {
		ESP_LOGE(TAG, "Couldn't send ambient correction data");
	}
}


void cmd_handler_get_backlight(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_BACKLIGHT, (int32_t) out_state.lcd_brightness)) {
		ESP_LOGE(TAG, "Couldn't send lcd_brightness");
	}
}


void cmd_handler_get_batt_level(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	int mv;
	
#ifdef CONFIG_BUILD_ICAM_MINI
	mv = ctrl_get_batt_percent();
#else
	mv = gcore_get_batt_percent();
#endif

	if (!cmd_send_int32(CMD_RSP, CMD_BATT_LEVEL, (int32_t) mv)) {
		ESP_LOGE(TAG, "Couldn't send batt_level");
	}
}


void cmd_handler_get_brightness(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_BRIGHTNESS, (int32_t) out_state.brightness)) {
		ESP_LOGE(TAG, "Couldn't send brightness");
	}
}


void cmd_handler_get_card_present(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_CARD_PRESENT, (int32_t) file_card_available())) {
		ESP_LOGE(TAG, "Couldn't send card present");
	}
}


void cmd_handler_get_emissivity(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_EMISSIVITY, (int32_t) out_state.emissivity)) {
		ESP_LOGE(TAG, "Couldn't send emissivity");
	}
}


void cmd_handler_get_file_catalog(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	int n;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		// Get the catalog number (-1 is folder catalog, 0.. are file catalogs for the indexed folder)
		n = (int) ntohl(*((uint32_t*) &data[0]));
		
		file_set_catalog_index(n);
		xTaskNotify(task_handle_file, FILE_NOTIFY_GUI_GET_CATALOG_MASK, eSetBits);
	}
}


void cmd_handler_get_file_image(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	int d, f;
	
	if (data_type == CMD_DATA_INT32) {
		if (cmd_decode_file_indicies(len, data, &d, &f)) {
			// Set the file info and request file_task to read and decompress the file
			file_set_image_fileinfo(d, f);
			xTaskNotify(task_handle_file, FILE_NOTIFY_GUI_GET_IMAGE_MASK, eSetBits);
		}
	}
}


void cmd_handler_get_gain(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_GAIN, (int32_t) out_state.high_gain)) {
		ESP_LOGE(TAG, "Couldn't send high_gain");
	}
}


void cmd_handler_get_min_max_enable(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_MIN_MAX_EN, (int32_t) out_state.min_max_mrk_enable)) {
		ESP_LOGE(TAG, "Couldn't send min_max_mrk_enable");
	}
}


void cmd_handler_get_palette(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_PALETTE, (int32_t) out_state.gui_palette_index)) {
		ESP_LOGE(TAG, "Couldn't send gui_palette_index");
	}
}


void cmd_handler_get_region_enable(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_REGION_EN, (int32_t) out_state.region_enable)) {
		ESP_LOGE(TAG, "Couldn't send region enable");
	}
}


void cmd_handler_get_save_ovl_en(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_SAVE_OVL_EN, (int32_t) out_state.save_ovl_en)) {
		ESP_LOGE(TAG, "Couldn't send save overlay enable");
	}
}


void cmd_handler_get_shutter(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	// Pack the byte array - the response handler must unpack in the same order
	send_buf[0] = (uint8_t) out_state.auto_ffc_en;
	*(uint32_t*)&send_buf[1] = htonl(out_state.ffc_temp_threshold_x10);
	*(uint32_t*)&send_buf[5] = htonl(out_state.min_ffc_interval);
	*(uint32_t*)&send_buf[9] = htonl(out_state.max_ffc_interval);
	
	if (!cmd_send_binary(CMD_RSP, CMD_SHUTTER_INFO, CMD_SHUTTER_INFO_LEN, send_buf)) {
		ESP_LOGE(TAG, "Couldn't send shutter data");
	}
}


void cmd_handler_get_spot_enable(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_SPOT_EN, (int32_t) out_state.spotmeter_enable)) {
		ESP_LOGE(TAG, "Couldn't send spotmeter_enable");
	}
}


void cmd_handler_get_sys_info(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_string(CMD_RSP, CMD_SYS_INFO, sys_info_get_string())) {
		ESP_LOGE(TAG, "Couldn't send sys_info");
	}
}


void cmd_handler_get_time(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	// Get the current time
	time_get(&te);
	
	// Pack the byte array - the response handler must unpack in the same order
	*(uint32_t*)&send_buf[0] = htonl((uint32_t) te.tm_sec);
	*(uint32_t*)&send_buf[4] = htonl((uint32_t) te.tm_min);
	*(uint32_t*)&send_buf[8] = htonl((uint32_t) te.tm_hour);
	*(uint32_t*)&send_buf[12] = htonl((uint32_t) te.tm_mday);
	*(uint32_t*)&send_buf[16] = htonl((uint32_t) te.tm_mon);
	*(uint32_t*)&send_buf[20] = htonl((uint32_t) te.tm_year);
	*(uint32_t*)&send_buf[24] = htonl((uint32_t) te.tm_wday);
	*(uint32_t*)&send_buf[28] = htonl((uint32_t) te.tm_yday);
	*(uint32_t*)&send_buf[32] = htonl((uint32_t) te.tm_isdst);
	
	if (!cmd_send_binary(CMD_RSP, CMD_TIME, CMD_TIME_LEN, send_buf)) {
		ESP_LOGE(TAG, "Couldn't send time");
	}
}


void cmd_handler_get_units(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (!cmd_send_int32(CMD_RSP, CMD_UNITS, (int32_t) out_state.temp_unit_C)) {
		ESP_LOGE(TAG, "Couldn't send temp_unit_C");
	}
}


void cmd_handler_get_wifi(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	int i, n = 0;
	
	// Get the current configuration
	ps_get_config(PS_CONFIG_TYPE_NET, &orig_net_config);
	
	// Pack the byte array - the response handler must unpack in the same order
	send_buf[n++] = (uint8_t) orig_net_config.mdns_en;
	send_buf[n++] = (uint8_t) orig_net_config.sta_mode;
	send_buf[n++] = (uint8_t) orig_net_config.sta_static_ip;
	
	for (i=0; i<PS_SSID_MAX_LEN+1; i++) send_buf[n++] = orig_net_config.ap_ssid[i];
	for (i=0; i<PS_SSID_MAX_LEN+1; i++) send_buf[n++] = orig_net_config.sta_ssid[i];
	for (i=0; i<PS_PW_MAX_LEN+1; i++) send_buf[n++] = orig_net_config.ap_pw[i];
	for (i=0; i<PS_PW_MAX_LEN+1; i++) send_buf[n++] = orig_net_config.sta_pw[i];
	
	for (i=0; i<4; i++) send_buf[n++] = orig_net_config.ap_ip_addr[i];
	for (i=0; i<4; i++) send_buf[n++] = orig_net_config.sta_ip_addr[i];
	for (i=0; i<4; i++) send_buf[n++] = orig_net_config.sta_netmask[i];
	
	if (!cmd_send_binary(CMD_RSP, CMD_WIFI_INFO, n, send_buf)) {
		ESP_LOGE(TAG, "Couldn't send wifi info");
	}
}


void cmd_handler_set_ambient_correct(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_BINARY) && (len == CMD_AMBIENT_CORRECT_LEN)) {
		// Unpack the byte array in the same order the set command packed it
		out_state.use_auto_ambient = (bool) data[0];
		out_state.refl_equals_ambient = (bool) data[1];
		out_state.atmospheric_temp = (int32_t) ntohl(*((uint32_t*) &data[2]));
		out_state.distance = ntohl(*((uint32_t*) &data[6]));
		out_state.humidity = ntohl(*((uint32_t*) &data[10]));
		out_state.reflected_temp = (int32_t) ntohl(*((uint32_t*) &data[14]));
		
	    out_state_save();
	    
	    // Update Tiny1C (after PS updated so it can use updated config)
	    xTaskNotify(task_handle_t1c, T1C_NOTIFY_ENV_UPD_MASK, eSetBits);
	}
}


void cmd_handler_set_backlight(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		out_state.lcd_brightness = ntohl(*((uint32_t*) &data[0]));
		
#ifndef CONFIG_BUILD_ICAM_MINI
		// Update gCore to change the backlight level
		xTaskNotify(task_handle_gcore, GCORE_NOTIFY_BRGHT_UPD_MASK, eSetBits);
#endif
	}
}


void cmd_handler_set_brightness(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		out_state.brightness = ntohl(*((uint32_t*) &data[0]));
			
		// Update Tiny1C
		if (!t1c_set_param_image(IMAGE_PROP_LEVEL_BRIGHTNESS, brightness_to_param_value(out_state.brightness))) {
			ESP_LOGE(TAG, "Failed to set brightness");
		}
		
		out_state_save();
	}
}


void cmd_handler_set_ctrl_activity(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_BINARY) && (len == 8)) {
		enum cmd_ctrl_act_param cmd = ntohl(*((uint32_t*) &data[0]));
		int32_t aux = (int32_t) ntohl(*((uint32_t*) &data[4]));
		
		switch (cmd) {
			case CMD_CTRL_ACT_RESTORE:
				// First reset persistent storage
				(void) ps_reinit_all();
				
				// Then notify t1c_task to reset the Tiny1C (it will notify the
				// control task to shutdown when done)
				xTaskNotify(task_handle_t1c, T1C_NOTIFY_RESTORE_DEFAULT_MASK, eSetBits);
				break;
				
			case CMD_CTRL_ACT_TINY1C_CAL_1:
				// Notify t1c_task to perform a 1 point calibration
				t1c_set_blackbody_temp((uint16_t) aux);
				xTaskNotify(task_handle_t1c, T1C_NOTIFY_CAL_1_MASK, eSetBits);
				break;
				
			case CMD_CTRL_ACT_TINY1C_CAL_2L:
				// Notify t1c_task to perform the first half of a 2 point calibration
				t1c_set_blackbody_temp((uint16_t) aux);
				xTaskNotify(task_handle_t1c, T1C_NOTIFY_CAL_2L_MASK, eSetBits);
				break;
				
			case CMD_CTRL_ACT_TINY1C_CAL_2H:
				// Notify t1c_task to perform the second half of a 2 point calibration
				t1c_set_blackbody_temp((uint16_t) aux);
				xTaskNotify(task_handle_t1c, T1C_NOTIFY_CAL_2H_MASK, eSetBits);
				break;
				
			case CMD_CTRL_ACT_SD_FORMAT:
				// Notify the file_task to format the card.  It'll let the output task know success/fail
				xTaskNotify(task_handle_file, FILE_NOTIFY_GUI_FORMAT_MASK, eSetBits);
				break;
		}
	}
}


void cmd_handler_set_emissivity(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		out_state.emissivity = ntohl(*((uint32_t*) &data[0]));
			
		// Update Tiny1C
		if (!t1c_set_param_tpd(TPD_PROP_EMS, emissivity_to_param_value(out_state.emissivity))) {
			ESP_LOGE(TAG, "Failed to set emissivity");
		}
		
		out_state_save();
	}
}


void cmd_handler_set_ffc(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (data_type == CMD_DATA_NONE) {
		xTaskNotify(task_handle_t1c, T1C_NOTIFY_FFC_MASK, eSetBits);
	}
}


void cmd_handler_set_file_delete(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	int d, f;
	
	if (data_type == CMD_DATA_INT32) {
		if (cmd_decode_file_indicies(len, data, &d, &f)) {
			// Setup delete info
			file_set_delete_file(d, f);
			
			if (f == -1) {
				// Notify file_task to delete directory
				xTaskNotify(task_handle_file, FILE_NOTIFY_GUI_DEL_DIR_MASK, eSetBits);
			} else {
				// Notify file_task to delete file
				xTaskNotify(task_handle_file, FILE_NOTIFY_GUI_DEL_FILE_MASK, eSetBits);
			}
		}
	}
}


void cmd_handler_set_gain(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		out_state.high_gain = (t != 0);
		out_state_save();
		
		// Update t1c_task
		if (!t1c_set_param_tpd(TPD_PROP_GAIN_SEL, out_state.high_gain ? 1 : 0)) {
			ESP_LOGE(TAG, "Failed to set gain mode");
		}
	}
}


void cmd_handler_set_min_max_enable(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		out_state.min_max_mrk_enable = (t != 0);
		out_state_save();
		
		// Update t1c_task
		t1c_set_minmax_marker_enable(out_state.min_max_mrk_enable);
	}
}


void cmd_handler_set_orientation(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		out_state.is_portrait = (t != 0);
	}
}


void cmd_handler_set_palette(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		set_palette(t);
		out_state.gui_palette_index = t;
		out_state_save();
	}
}


void cmd_handler_set_poweroff(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (data_type == CMD_DATA_NONE) {
#ifdef CONFIG_BUILD_ICAM_MINI
	xTaskNotify(task_handle_ctrl, CTRL_NOTIFY_SHUTDOWN, eSetBits);
#else
	xTaskNotify(task_handle_gcore, GCORE_NOTIFY_SHUTOFF_MASK, eSetBits);
#endif	
	}
}


void cmd_handler_set_save_backlight(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		out_state.lcd_brightness = ntohl(*((uint32_t*) &data[0]));
		
#ifndef CONFIG_BUILD_ICAM_MINI
		// Update gCore to change the backlight level
		xTaskNotify(task_handle_gcore, GCORE_NOTIFY_BRGHT_UPD_MASK, eSetBits);
#endif

		out_state_save();
	}
}


void cmd_handler_set_save_ovl_en(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		out_state.save_ovl_en = (t != 0) ? true : false;
		out_state_save();
	}
}


void cmd_handler_set_save_palette(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	// Don't update NVS as this may occur frequently and a future call to save 
	// the display palette will catch this
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		set_save_palette(t);
		out_state.sav_palette_index = t;
	}
}


void cmd_handler_set_region_enable(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		out_state.region_enable = (t != 0);
		out_state_save();
		
		// Update t1c_task
		t1c_set_region_enable(out_state.region_enable);
	}
}


void cmd_handler_set_region_location(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint16_t x1, y1, x2, y2;
	
	if (cmd_decode_marker_location(len, data, &x1, &y1, &x2, &y2)) {
		t1c_set_region_location(x1, y1, x2, y2);
	}
}


void cmd_handler_set_shutter(cmd_data_t data_type, uint32_t len, uint8_t* data)
{	
	if ((data_type == CMD_DATA_BINARY) && (len == CMD_SHUTTER_INFO_LEN)) {
		// Unpack the byte array in the same order the set command packed it
		out_state.auto_ffc_en = (bool) data[0];
		out_state.ffc_temp_threshold_x10 = ntohl(*((uint32_t*) &data[1]));
		out_state.min_ffc_interval = ntohl(*((uint32_t*) &data[5]));
		out_state.max_ffc_interval = ntohl(*((uint32_t*) &data[9]));
		
		// Update Tiny1C before updating PS to detect differences
		(void) ps_get_config(PS_CONFIG_TYPE_T1C, &t1c_config);
		
		if (out_state.auto_ffc_en != t1c_config.auto_ffc_en) {
			if (!t1c_set_param_shutter(SHUTTER_PROP_SWITCH, out_state.auto_ffc_en ? 1 : 0)) {
				ESP_LOGE(TAG, "Failed to set Auto Shutter enable");
			}
		}
		if (out_state.ffc_temp_threshold_x10 != t1c_config.ffc_temp_threshold_x10) {
			uint16_t t = (uint16_t) (out_state.ffc_temp_threshold_x10 * 36 / 10);
			if (!t1c_set_param_shutter(SHUTTER_PROP_TEMP_THRESHOLD_B, t)) {
				ESP_LOGE(TAG, "Failed to set Auto Shutter temp threshold");
			}
		}
		if (out_state.min_ffc_interval != t1c_config.min_ffc_interval) {
			if (!t1c_set_param_shutter(SHUTTER_PROP_MIN_INTERVAL, (uint16_t) out_state.min_ffc_interval)) {
				ESP_LOGE(TAG, "Failed to set Auto Shutter min interval");
			}
			if (!t1c_set_param_shutter(SHUTTER_PROP_ANY_INTERVAL, (uint16_t) out_state.min_ffc_interval)) {
				ESP_LOGE(TAG, "Failed to set Manual Shutter min interval");
			}
		}
		if (out_state.max_ffc_interval != t1c_config.max_ffc_interval) {
			if (!t1c_set_param_shutter(SHUTTER_PROP_MAX_INTERVAL, (uint16_t) out_state.max_ffc_interval)) {
				ESP_LOGE(TAG, "Failed to set Auto Shutter max interval");
			}
		}
		
	    out_state_save();
	}
}


void cmd_handler_set_spot_enable(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		out_state.spotmeter_enable = (t != 0);
		out_state_save();
		
		// Update t1c_task
		t1c_set_spot_enable(out_state.spotmeter_enable);
	}
}


void cmd_handler_set_spot_location(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint16_t x1, y1, x2, y2;
	
	if (cmd_decode_marker_location(len, data, &x1, &y1, &x2, &y2)) {
		t1c_set_spot_location(x1, y1);
	}
}


void cmd_handler_set_stream_enable(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		stream_enable_flag = (t != 0);
	}
}


void cmd_handler_set_take_picture(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (data_type == CMD_DATA_NONE) {
		notify_take_picture = true;
	}
}


void cmd_handler_set_time(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_BINARY) && (len == CMD_TIME_LEN)) {
		te.tm_sec = (int) ntohl(*((uint32_t*) &data[0]));
		te.tm_min = (int) ntohl(*((uint32_t*) &data[4]));
		te.tm_hour = (int) ntohl(*((uint32_t*) &data[8]));
		te.tm_mday = (int) ntohl(*((uint32_t*) &data[12]));
		te.tm_mon = (int) ntohl(*((uint32_t*) &data[16]));
		te.tm_year = (int) ntohl(*((uint32_t*) &data[20]));
		te.tm_wday = (int) ntohl(*((uint32_t*) &data[24]));
		te.tm_yday = (int) ntohl(*((uint32_t*) &data[28]));
		te.tm_isdst = (int) ntohl(*((uint32_t*) &data[32]));
		
		time_set(&te);
	}
}


void cmd_handler_set_timelapse_cfg(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	bool en;
	bool notify;
	uint32_t interval;
	uint32_t num;
	
	if ((data_type == CMD_DATA_BINARY) && (len == CMD_TIMELAPSE_LEN)) {
		en = ntohl(*((uint32_t*) &data[0])) != 0;
		notify = ntohl(*((uint32_t*) &data[1])) != 0;
		interval = ntohl(*((uint32_t*) &data[2]));
		num = ntohl(*((uint32_t*) &data[6]));
		
		// Set the parameters
		file_set_timelapse_info(en, notify, interval, num);
	}
}


void cmd_handler_set_units(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		out_state.temp_unit_C = (t != 0);
		out_state_save();
	}
}


void cmd_handler_set_wifi(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	int i, n = 0;
	
	if ((data_type == CMD_DATA_BINARY) && (len == CMD_WIFI_INFO_LEN)) {
		// Get the current configuration
		ps_get_config(PS_CONFIG_TYPE_NET, &orig_net_config);
		
		// Unpack in same order as loaded
		new_net_config.mdns_en = (bool) data[n++];
		new_net_config.sta_mode = (bool) data[n++];
		new_net_config.sta_static_ip = (bool) data[n++];
		
		for (i=0; i<PS_SSID_MAX_LEN+1; i++) new_net_config.ap_ssid[i] = data[n++];
		for (i=0; i<PS_SSID_MAX_LEN+1; i++) new_net_config.sta_ssid[i] = data[n++];
		for (i=0; i<PS_PW_MAX_LEN+1; i++) new_net_config.ap_pw[i] = data[n++];
		for (i=0; i<PS_PW_MAX_LEN+1; i++) new_net_config.sta_pw[i] = data[n++];
		
		for (i=0; i<4; i++) new_net_config.ap_ip_addr[i] = data[n++];
		for (i=0; i<4; i++) new_net_config.sta_ip_addr[i] = data[n++];
		for (i=0; i<4; i++) new_net_config.sta_netmask[i] = data[n++];
		
		if (!net_config_structs_eq(&orig_net_config, &new_net_config)) {
			// Update PS if changed
			ps_set_config(PS_CONFIG_TYPE_NET, &new_net_config);
			
#ifdef CONFIG_BUILD_ICAM_MINI
			// Notify ctrl_task to restart the network
			xTaskNotify(task_handle_ctrl, CTRL_NOTIFY_RESTART_NETWORK, eSetBits);
#endif
		}
	}
}


bool cmd_handler_stream_enabled()
{
	return stream_enable_flag;
}


bool cmd_handler_take_picture_notification()
{
	if (notify_take_picture) {
		notify_take_picture = false;
		return true;
	}
	
	return false;
}



//
// Internal functions
//
static bool net_config_structs_eq(net_config_t* s1, net_config_t* s2)
{
	if (s1->mdns_en != s2->mdns_en) return false;
	if (s1->sta_mode != s2->sta_mode) return false;
	if (s1->sta_static_ip != s2->sta_static_ip) return false;
	
	if (strcmp(s1->ap_ssid, s2->ap_ssid) != 0) return false;
	if (strcmp(s1->sta_ssid, s2->sta_ssid) != 0) return false;
	if (strcmp(s1->ap_pw, s2->ap_pw) != 0) return false;
	if (strcmp(s1->sta_pw, s2->sta_pw) != 0) return false;
	
	for (int i=0; i<4; i++) {
		if (s1->ap_ip_addr[i] != s2->ap_ip_addr[i]) return false;
		if (s1->sta_ip_addr[i] != s2->sta_ip_addr[i]) return false;
		if (s1->sta_netmask[i] != s2->sta_netmask[i]) return false;
	}
	
	return true;
}
