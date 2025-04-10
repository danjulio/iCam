/*
 * Command handlers updating GUI state in the web browser (iCamMini) or on gCore (iCam).
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
#include "gui_cmd_handlers.h"
#include "gui_panel_file_browser_files.h"
#include "gui_panel_file_browser_image.h"
#include "gui_panel_image_controls.h"
#include "gui_panel_image_main.h"
#include "gui_render.h"
#include "gui_state.h"
#include "gui_sub_page_info.h"
#include "gui_sub_page_time.h"
#include "gui_utilities.h"
#include "palettes.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef ESP_PLATFORM
#include "falcon_cmd.h"
#include "tiny1c.h"
#endif


//
// Constants
//

// These must match code below and in cmd handlers and sender
#define CMD_AMBIENT_CORRECT_LEN 18
#define CMD_SHUTTER_INFO_LEN    13
#define CMD_TIME_LEN            36
#define CMD_WIFI_INFO_LEN       (3 + 2*(GUI_SSID_MAX_LEN+1) + 2*(GUI_PW_MAX_LEN+1) + 3*4)



//
// Forward declarations for internal functions
//
#ifndef ESP_PLATFORM
static uint8_t* _get_i16(int16_t* data, uint8_t* buf);
static uint8_t* _get_u16(uint16_t* data, uint8_t* buf);
#endif



//
// API
//
void cmd_handler_set_critical_batt(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_NONE) && (len == 0)) {
		// Display a permanent critical battery imminent shutdown message long enough
		// to cover the shutdown period but expiring if a iCamMini shuts down and
		// then the user Reconnects instead of refreshing the browser.
		gui_panel_image_set_message("Low battery shutdown imminent", 31000);
	}
}


void cmd_handler_set_image(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	// web:
	//  have to decode this into individual units and update GUI colorizing 8-bit data
	// local:
	//  can recast to t1c_buffer_t and use that to decode this into individual units
	//  and update GUI scaling and colorizing 8-bit data
#ifdef ESP_PLATFORM
	t1c_buffer_t* t1cP;
	
	if ((data_type == CMD_DATA_BINARY) && (len == 4)) {
		// Unpack the t1c_buffer_t pointer
		t1cP = (t1c_buffer_t*) ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
		
		// Get boolean flags
		gui_panel_image_buf.high_gain = t1cP->high_gain;
		gui_panel_image_buf.vid_frozen = t1cP->vid_frozen;
		gui_panel_image_buf.spot_valid = t1cP->spot_valid;
		gui_panel_image_buf.minmax_valid = t1cP->minmax_valid;
		gui_panel_image_buf.region_valid = t1cP->region_valid;
		gui_panel_image_buf.amb_temp_valid = t1cP->amb_temp_valid;
		gui_panel_image_buf.amb_hum_valid = t1cP->amb_hum_valid;
		gui_panel_image_buf.distance_valid = t1cP->distance_valid;
		
		// Get 16-bit values
		gui_panel_image_buf.amb_temp = t1cP->amb_temp;
		gui_panel_image_buf.amb_hum = t1cP->amb_hum;
		gui_panel_image_buf.distance = t1cP->distance;
		gui_panel_image_buf.spot_temp = t1cP->spot_temp;
		gui_panel_image_buf.spot_x = t1cP->spot_point.x;
		gui_panel_image_buf.spot_y = t1cP->spot_point.y;
		gui_panel_image_buf.min_temp = t1cP->max_min_temp_info.min_temp;
		gui_panel_image_buf.min_x = t1cP->max_min_temp_info.min_temp_point.x;
		gui_panel_image_buf.min_y = t1cP->max_min_temp_info.min_temp_point.y;
		gui_panel_image_buf.max_temp = t1cP->max_min_temp_info.max_temp;
		gui_panel_image_buf.max_x = t1cP->max_min_temp_info.max_temp_point.x;
		gui_panel_image_buf.max_y = t1cP->max_min_temp_info.max_temp_point.y;
		gui_panel_image_buf.region_x1 = t1cP->region_points.start_point.x;
		gui_panel_image_buf.region_y1 = t1cP->region_points.start_point.y;
		gui_panel_image_buf.region_x2 = t1cP->region_points.end_point.x;
		gui_panel_image_buf.region_y2 = t1cP->region_points.end_point.y;
		gui_panel_image_buf.region_avg_temp = t1cP->region_temp_info.temp_info_value.ave_temp;
		gui_panel_image_buf.region_min_temp = t1cP->region_temp_info.temp_info_value.min_temp;
		gui_panel_image_buf.region_min_x = t1cP->region_temp_info.min_temp_point.x;
		gui_panel_image_buf.region_min_y = t1cP->region_temp_info.min_temp_point.y;
		gui_panel_image_buf.region_max_temp = t1cP->region_temp_info.temp_info_value.max_temp;
		gui_panel_image_buf.region_max_x = t1cP->region_temp_info.max_temp_point.x;
		gui_panel_image_buf.region_max_y = t1cP->region_temp_info.max_temp_point.y;
		
		// Scale Tiny1c raw data to 8-bits
		gui_panel_image_buf.y8_data = gui_render_get_y8_data(t1cP->img_data, t1cP->y16_min, t1cP->y16_max);
		
		// Let the image display know we've got an image to display
		gui_panel_image_render_image();
	}
#else
	uint8_t* dP = data;
	
	// Unpack in the same order as encoded in ws_cmd_utilties.c
	if ((data_type == CMD_DATA_BINARY) && (len == (54 + GUI_RAW_IMG_W*GUI_RAW_IMG_H))) {
		//  Get boolean flags (each held in a byte)
		gui_panel_image_buf.high_gain = *dP++;
		gui_panel_image_buf.vid_frozen = *dP++;
		gui_panel_image_buf.spot_valid = *dP++;
		gui_panel_image_buf.minmax_valid = *dP++;
		gui_panel_image_buf.region_valid = *dP++;
		gui_panel_image_buf.amb_temp_valid = *dP++;
		gui_panel_image_buf.amb_hum_valid = *dP++;
		gui_panel_image_buf.distance_valid = *dP++;
		
		// Unpack the 16-bit values
		dP = _get_i16(&gui_panel_image_buf.amb_temp, dP);
		dP = _get_u16(&gui_panel_image_buf.amb_hum, dP);
		dP = _get_u16(&gui_panel_image_buf.distance, dP);
		dP = _get_u16(&gui_panel_image_buf.spot_temp, dP);
		dP = _get_u16(&gui_panel_image_buf.spot_x, dP);
		dP = _get_u16(&gui_panel_image_buf.spot_y, dP);
		dP = _get_u16(&gui_panel_image_buf.min_temp, dP);
		dP = _get_u16(&gui_panel_image_buf.min_x, dP);
		dP = _get_u16(&gui_panel_image_buf.min_y, dP);
		dP = _get_u16(&gui_panel_image_buf.max_temp, dP);
		dP = _get_u16(&gui_panel_image_buf.max_x, dP);
		dP = _get_u16(&gui_panel_image_buf.max_y, dP);
		dP = _get_u16(&gui_panel_image_buf.region_x1, dP);
		dP = _get_u16(&gui_panel_image_buf.region_y1, dP);
		dP = _get_u16(&gui_panel_image_buf.region_x2, dP);
		dP = _get_u16(&gui_panel_image_buf.region_y2, dP);
		dP = _get_u16(&gui_panel_image_buf.region_avg_temp, dP);
		dP = _get_u16(&gui_panel_image_buf.region_min_temp, dP);
		dP = _get_u16(&gui_panel_image_buf.region_min_x, dP);
		dP = _get_u16(&gui_panel_image_buf.region_min_y, dP);
		dP = _get_u16(&gui_panel_image_buf.region_max_temp, dP);
		dP = _get_u16(&gui_panel_image_buf.region_max_x, dP);
		dP = _get_u16(&gui_panel_image_buf.region_max_y, dP);
		
		// Copy the pre-scaled 8-bit data to our buffer
		gui_panel_image_buf.y8_data = gui_render_get_y8_data((uint16_t*) dP, 0, 0);
		
		// Let the image display know we've got an image to display
		gui_panel_image_render_image();
	}
#endif
}


void cmd_handler_set_msg_on(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (data_type == CMD_DATA_STRING) {
		gui_panel_image_set_message((char*) data, 0);
	}
}


void cmd_handler_set_msg_off(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_NONE) && (len == 0)) {
		gui_panel_image_set_message("", 0);
	}
}


void cmd_handler_set_timelapse_status(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	bool new_running;
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		new_running = (t != 0);
		
		// Notify the user of changes
		if (gui_state.timelapse_running != new_running) {
			if (new_running) {
				gui_panel_image_set_message("Timelapse start", 700);
			} else {
				gui_panel_image_set_message("Timelapse stop", 700);
			}
		}
		
		// Update state and image panels that display indication of timelapse running
		gui_state.timelapse_running = new_running;
		if (!new_running) {
			// Clear enable flag at the end of a timelapse series
			gui_state.timelapse_enable = false;
		}
		gui_panel_image_set_timelapse(gui_state.timelapse_running);
		gui_panel_image_controls_set_timelapse(gui_state.timelapse_running);
	}
}


void cmd_handler_rsp_ambient_correct(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_BINARY) && (len == CMD_AMBIENT_CORRECT_LEN)) {
		// Unpack the byte array in the same order the get command packed it
		gui_state.use_auto_ambient = (bool) data[0];
		gui_state.refl_equals_ambient = (bool) data[1];
		gui_state.atmospheric_temp = (int32_t) ntohl(*((uint32_t*) &data[2]));
		gui_state.distance = ntohl(*((uint32_t*) &data[6]));
		gui_state.humidity = ntohl(*((uint32_t*) &data[10]));
		gui_state.reflected_temp = (int32_t) ntohl(*((uint32_t*) &data[14]));
		gui_state_note_item_inited(GUI_STATE_INIT_AMBIENT_C);
	}
}


void cmd_handler_rsp_backlight(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		gui_state.lcd_brightness = ntohl(*((uint32_t*) &data[0]));
		gui_state_note_item_inited(GUI_STATE_INIT_BACKLIGHT);
	}
}


void cmd_handler_rsp_batt_info(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		gui_panel_image_set_batt_percent((int) ntohl(*((uint32_t*) &data[0])));
	}
}


void cmd_handler_rsp_brightness(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		gui_state.brightness = ntohl(*((uint32_t*) &data[0]));
		gui_state_note_item_inited(GUI_STATE_INIT_BRIGHTNESS);
	}
}


void cmd_handler_rsp_card_present(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		gui_state.card_present = (bool) ntohl(*((uint32_t*) &data[0]));
		gui_state_note_item_inited(GUI_STATE_INIT_CARD_PRES);
	}
}


void cmd_handler_rsp_ctrl_activity(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		gui_update_activity_popup((bool) ntohl(*((uint32_t*) &data[0])));
	}
}


void cmd_handler_rsp_emissivity(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		gui_state.emissivity = ntohl(*((uint32_t*) &data[0]));
		gui_state_note_item_inited(GUI_STATE_INIT_EMISSIVITY);
	}
}


void cmd_handler_rsp_file_catalog(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	static bool part_1_valid = false;
	bool part_2_valid = false;
	char* catalog_list;
	static int cmd_type;
	static int num_entries;
	uint32_t t;
	
	// This response handler is a bit special.  It actually can receive two different
	// types.  First it should see an int with the cmd_type and num_entries packed in
	// it then it should see a string with the catalog_list.  The packing must match
	// the sending code.  For all code running on the esp platform, the string is passed
	// as a pointer.  For code running on the web platform the string comes in the command.
	//
	// First get the numeric values
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		
		// Low 16 bits: type
		// High 16 bits: number of entries
		cmd_type = (int) ((int16_t) (t & 0xFFFF));
		num_entries = (int) (t >> 16);
		
		part_1_valid = true;
	}
	
	// Then, in a platform-specific way, get the string
#ifdef ESP_PLATFORM
	if ((data_type == CMD_DATA_BINARY) && (len == 4)) {
		// Unpack the string pointer
		catalog_list = (char*) ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
		part_2_valid = true;
	}
#else
	if (data_type == CMD_DATA_STRING) {
		catalog_list = (char*) data;
		part_2_valid = true;
	}
#endif

	if (part_1_valid && part_2_valid) {
		// Update the list in the file browser list
		gui_panel_file_browser_files_set_catalog(cmd_type, num_entries, catalog_list);
		
		// Setup for next response
		part_1_valid = false;
		part_2_valid = false;
	}
}


void cmd_handler_rsp_file_image(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
#ifdef ESP_PLATFORM
	if ((data_type == CMD_DATA_NONE)) {
		gui_panel_file_browser_image_set_valid(true);
		gui_panel_file_browser_image_set_image();
	}
#else
	if ((data_type == CMD_DATA_BINARY) && (len == (3*GUI_RAW_IMG_W*GUI_RAW_IMG_H))) {
		gui_panel_file_browser_image_set_valid(true);
		gui_panel_file_browser_image_set_image(len, data);
	}
#endif
}


void cmd_handler_rsp_gain(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		gui_state.high_gain = (t != 0);
		gui_state_note_item_inited(GUI_STATE_INIT_GAIN);
	}
}
	

void cmd_handler_rsp_min_max_en(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		gui_state.min_max_mrk_enable = (t != 0);
		gui_state_note_item_inited(GUI_STATE_INIT_MIN_MAX);
	}
}


void cmd_handler_rsp_palette(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		gui_state.palette_index = ntohl(*((uint32_t*) &data[0]));
		set_palette(gui_state.palette_index);
		gui_state_note_item_inited(GUI_STATE_INIT_PALETTE);
	}
}


void cmd_handler_rsp_region_enable(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		gui_state.region_enable = (t != 0);
		gui_state_note_item_inited(GUI_STATE_INIT_REGION);
	}
}


void cmd_handler_rsp_save_ovl_en(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		gui_state.save_ovl_en = (t != 0);
		gui_state_note_item_inited(GUI_STATE_INIT_SAVE_OVL);
	}
}


void cmd_handler_rsp_shutter(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_BINARY) && (len == CMD_SHUTTER_INFO_LEN)) {
		// Unpack the byte array in the same order the get command packed it
		gui_state.auto_ffc_en = (bool) data[0];
		gui_state.ffc_temp_threshold_x10 = ntohl(*((uint32_t*) &data[1]));
		gui_state.min_ffc_interval = ntohl(*((uint32_t*) &data[5]));
		gui_state.max_ffc_interval = ntohl(*((uint32_t*) &data[9]));
		gui_state_note_item_inited(GUI_STATE_INIT_SHUTTER);
	}
}


void cmd_handler_rsp_spot_enable(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		gui_state.spotmeter_enable = (t != 0);
		gui_state_note_item_inited(GUI_STATE_INIT_SPOT);
	}
}


void cmd_handler_rsp_sys_info(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (data_type == CMD_DATA_STRING) {
		gui_sub_page_info_set_string((char*) data);
	}
}


void cmd_handler_rsp_time(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	struct tm te;
	
	if ((data_type == CMD_DATA_BINARY) && (len == CMD_TIME_LEN)) {
		// Unpack the byte array in the same order the get command packed it
		te.tm_sec = (int) ntohl(*((uint32_t*) &data[0]));
		te.tm_min = (int) ntohl(*((uint32_t*) &data[4]));
		te.tm_hour = (int) ntohl(*((uint32_t*) &data[8]));
		te.tm_mday = (int) ntohl(*((uint32_t*) &data[12]));
		te.tm_mon = (int) ntohl(*((uint32_t*) &data[16]));
		te.tm_year = (int) ntohl(*((uint32_t*) &data[20]));
		te.tm_wday = (int) ntohl(*((uint32_t*) &data[24]));
		te.tm_yday = (int) ntohl(*((uint32_t*) &data[28]));
		te.tm_isdst = (int) ntohl(*((uint32_t*) &data[32]));
		
		gui_sub_page_time_set_time(&te);
	}
}


void cmd_handler_rsp_units(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t t;
	
	if ((data_type == CMD_DATA_INT32) && (len == 4)) {
		t = ntohl(*((uint32_t*) &data[0]));
		gui_state.temp_unit_C = (t != 0);
		gui_state_note_item_inited(GUI_STATE_INIT_UNIT);
	}
}


void cmd_handler_rsp_wifi(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	int i, n = 0;
	
	if ((data_type == CMD_DATA_BINARY) && (len == CMD_WIFI_INFO_LEN)) {
		// Unpack the byte array in the same order the get command packed it
		gui_state.mdns_en = (bool) data[n++];
		gui_state.sta_mode = (bool) data[n++];
		gui_state.sta_static_ip = (bool) data[n++];
		
		for (i=0; i<GUI_SSID_MAX_LEN+1; i++) gui_state.ap_ssid[i] = data[n++];
		for (i=0; i<GUI_SSID_MAX_LEN+1; i++) gui_state.sta_ssid[i] = data[n++];
		for (i=0; i<GUI_PW_MAX_LEN+1; i++) gui_state.ap_pw[i] = data[n++];
		for (i=0; i<GUI_PW_MAX_LEN+1; i++) gui_state.sta_pw[i] = data[n++];
		
		for (i=0; i<4; i++) gui_state.ap_ip_addr[i] = data[n++];
		for (i=0; i<4; i++) gui_state.sta_ip_addr[i] = data[n++];
		for (i=0; i<4; i++) gui_state.sta_netmask[i] = data[n++];
		
		gui_state_note_item_inited(GUI_STATE_INIT_WIFI);
	}
}



//
// Internal functions
//
#ifndef ESP_PLATFORM
static uint8_t* _get_i16(int16_t* data, uint8_t* buf)
{
	// Network order - big endian
	*data = (*buf++) << 8;
	*data |= *buf++;
	
	return buf;
}

static uint8_t* _get_u16(uint16_t* data, uint8_t* buf)
{
	// Network order - big endian
	*data = (*buf++) << 8;
	*data |= *buf++;
	
	return buf;
}
#endif

#endif /* !CONFIG_BUILD_ICAM_MINI */
