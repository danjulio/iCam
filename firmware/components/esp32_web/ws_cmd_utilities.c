/*
 * Websocket command handlers for use by the web server.
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
#ifdef CONFIG_BUILD_ICAM_MINI

#include <arpa/inet.h>
#include "cmd_handlers.h"
#include "cmd_list.h"
#include "cmd_utilities.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/semphr.h"
#include "sys_utilities.h"
#include "tiny1c.h"
#include "ws_cmd_utilities.h"




//
// Local constants
//

// Encoded websocket packet (network byte order)
//   uint32_t  length     (complete packet length)
//   uint32_t  cmd_type
//   uint32_t  cmd_id
//   uint32_t  data_type
//   uint8_t[] data
#define WS_PKT_LEN_OFFSET   0
#define WS_PKT_CTYPE_OFFSET 4
#define WS_PKT_ID_OFFSET    8
#define WS_PKT_DTYPE_OFFSET 12
#define WS_PKT_DATA_OFFSET  16

// Minimum websocket packet size (no data)
#define MIN_WS_PKT_LEN      16

// Maximum websocket packet size (sized for the largest item: RGB888 image from jpeg)
#define MAX_WS_PKT_LEN      (MIN_WS_PKT_LEN + 3*T1C_WIDTH*T1C_HEIGHT)

// Each TX buffer (sent to gui through websocket) is sized to hold one packet
#define WS_TX_BUFFER_LEN    (MAX_WS_PKT_LEN)

// RX buffer (holds packet received from gui) is sized to hold one packet.
// It's designed to handed to websocket receive code by calling task and
// then processed.
#define WS_RX_BUFFER_LEN    (MIN_WS_PKT_LEN + 8192)

// Maximum number of stored TX packets
#define WS_MAX_TX_PKTS      4



//
// Typedefs
//
typedef struct {
	uint8_t* buf;
	uint32_t len;
} tx_buffer_t;


//
// Local variables
//
static const char* TAG = "ws_cmd_utilities";

// RX buffer for processing a single received web socket packet at a time in a callback
// function called by the underlying web server task.  Its contents should be processed
// immediately (and any response pushed into the TX buffer)
static uint8_t* rx_buffer;

// The TX buffer consists of multiple individual buffers each holding one packet.
// They are treated as a circular buffer and protected with a mutex so the can be
// loaded from both the underlying web server task and our web task.  A bit wasteful
// of space but we're putting them in PSRAM.
static tx_buffer_t tx_buffer[WS_MAX_TX_PKTS];
static int tx_buffer_push_index = 0;
static int tx_buffer_pop_index = 0;
static int tx_buffer_num_entries = 0;
static SemaphoreHandle_t tx_mutex;



//
// Forward declarations for internal functions
//
static uint32_t _serialize_t1c_buffer(t1c_buffer_t* t1cP, uint8_t* data);
static uint8_t* _add_i16(int16_t data, uint8_t* buf);
static uint8_t* _add_u16(uint16_t data, uint8_t* buf);



//
// API
//
bool ws_gui_cmd_init()
{
	// Allocate rx and tx buffers
	for (int i=0; i<WS_MAX_TX_PKTS; i++) {
		tx_buffer[i].buf =  (uint8_t*) heap_caps_malloc(WS_TX_BUFFER_LEN, MALLOC_CAP_SPIRAM);
		if (tx_buffer[i].buf == NULL) {
			ESP_LOGE(TAG, "create tx_buffer %d failed", i);
			return false;
		}
	}
	tx_mutex = xSemaphoreCreateMutex();
	
	rx_buffer = (uint8_t*) heap_caps_malloc(WS_RX_BUFFER_LEN, MALLOC_CAP_SPIRAM);
	if (rx_buffer == NULL) {
		ESP_LOGE(TAG, "malloc rx_buffer failed");
		return false;
	}
	
	// Initialize the command system
	if (!cmd_init_remote(ws_cmd_send_handler)) {
		return false;
	}
	
	// Register command handlers supported on our end (get, set, rsp)
	(void) cmd_register_cmd_id(CMD_AMBIENT_CORRECT, cmd_handler_get_ambient_correct, cmd_handler_set_ambient_correct, NULL);
	(void) cmd_register_cmd_id(CMD_BATT_LEVEL, cmd_handler_get_batt_level, NULL, NULL);
	(void) cmd_register_cmd_id(CMD_BRIGHTNESS, cmd_handler_get_brightness, cmd_handler_set_brightness, NULL);
	(void) cmd_register_cmd_id(CMD_CTRL_ACTIVITY, NULL, cmd_handler_set_ctrl_activity, NULL);
	(void) cmd_register_cmd_id(CMD_CARD_PRESENT, cmd_handler_get_card_present, NULL, NULL);
	(void) cmd_register_cmd_id(CMD_EMISSIVITY, cmd_handler_get_emissivity, cmd_handler_set_emissivity, NULL);
	(void) cmd_register_cmd_id(CMD_FILE_CATALOG, cmd_handler_get_file_catalog, NULL, NULL);
	(void) cmd_register_cmd_id(CMD_FILE_DELETE, NULL, cmd_handler_set_file_delete, NULL);
	(void) cmd_register_cmd_id(CMD_FILE_GET_IMAGE, cmd_handler_get_file_image, NULL, NULL);
	(void) cmd_register_cmd_id(CMD_FFC, NULL, cmd_handler_set_ffc, NULL);
	(void) cmd_register_cmd_id(CMD_GAIN, cmd_handler_get_gain, cmd_handler_set_gain, NULL);
	(void) cmd_register_cmd_id(CMD_MIN_MAX_EN, cmd_handler_get_min_max_enable, cmd_handler_set_min_max_enable, NULL);
	(void) cmd_register_cmd_id(CMD_ORIENTATION, NULL, cmd_handler_set_orientation, NULL);
	(void) cmd_register_cmd_id(CMD_PALETTE, cmd_handler_get_palette, cmd_handler_set_palette, NULL);
	(void) cmd_register_cmd_id(CMD_POWEROFF, NULL, cmd_handler_set_poweroff, NULL);
	(void) cmd_register_cmd_id(CMD_REGION_EN, cmd_handler_get_region_enable, cmd_handler_set_region_enable, NULL);
	(void) cmd_register_cmd_id(CMD_REGION_LOC, NULL, cmd_handler_set_region_location, NULL);
	(void) cmd_register_cmd_id(CMD_SHUTTER_INFO, cmd_handler_get_shutter, cmd_handler_set_shutter, NULL);
	(void) cmd_register_cmd_id(CMD_SAVE_OVL_EN, cmd_handler_get_save_ovl_en, cmd_handler_set_save_ovl_en, NULL);
	(void) cmd_register_cmd_id(CMD_SAVE_PALETTE, NULL, cmd_handler_set_save_palette, NULL);
	(void) cmd_register_cmd_id(CMD_SPOT_EN, cmd_handler_get_spot_enable, cmd_handler_set_spot_enable, NULL);
	(void) cmd_register_cmd_id(CMD_SPOT_LOC, NULL, cmd_handler_set_spot_location, NULL);
	(void) cmd_register_cmd_id(CMD_STREAM_EN, NULL, cmd_handler_set_stream_enable, NULL);
	(void) cmd_register_cmd_id(CMD_SYS_INFO, cmd_handler_get_sys_info, NULL, NULL);
	(void) cmd_register_cmd_id(CMD_TAKE_PICTURE, NULL, cmd_handler_set_take_picture, NULL);
	(void) cmd_register_cmd_id(CMD_TIME, cmd_handler_get_time, cmd_handler_set_time, NULL);
	(void) cmd_register_cmd_id(CMD_TIMELAPSE_CFG, NULL, cmd_handler_set_timelapse_cfg, NULL);
	(void) cmd_register_cmd_id(CMD_UNITS, cmd_handler_get_units, cmd_handler_set_units, NULL);
	(void) cmd_register_cmd_id(CMD_WIFI_INFO, cmd_handler_get_wifi, cmd_handler_set_wifi, NULL);
	
	return true;
}


bool ws_cmd_get_tx_data(uint32_t* len, uint8_t** data)
{
	bool valid;
	
	xSemaphoreTake(tx_mutex, portMAX_DELAY);
	valid = (tx_buffer_num_entries > 0);
	if (valid) {
		*data = tx_buffer[tx_buffer_pop_index].buf;
		*len = tx_buffer[tx_buffer_pop_index].len;
		if (++tx_buffer_pop_index >= WS_MAX_TX_PKTS) tx_buffer_pop_index = 0;
		tx_buffer_num_entries -= 1;
	}
	xSemaphoreGive(tx_mutex);
	
	return valid;
}


uint8_t* ws_cmd_get_rx_data_buffer()
{
	return rx_buffer;
}


bool ws_cmd_process_socket_rx_data(uint32_t len, uint8_t* data)
{
	cmd_t cmd_type;
	cmd_id_t cmd_id;
	cmd_data_t data_type;
	uint32_t dlen;
	
	// Make sure received data contains at least the minimum cmd arguments
	if (len < MIN_WS_PKT_LEN) {
		ESP_LOGE(TAG, "Illegal websocket packet length %lu", len);
		return false;
	}
	
	// Make sure the received data length matches what the cmd says its length is
	dlen = ntohl(*((uint32_t*) &data[WS_PKT_LEN_OFFSET]));
	if (len != dlen) {
		ESP_LOGE(TAG, "websocket packet len %lu does not match expected %lu", len, dlen);
		return false;
	}
	
	// Convert raw packet data in network order to cmd arguments
	cmd_type = (cmd_t) ntohl(*((uint32_t*) &data[WS_PKT_CTYPE_OFFSET]));
	cmd_id = (cmd_id_t) ntohl(*((uint32_t*) &data[WS_PKT_ID_OFFSET]));
	data_type = (cmd_data_t) ntohl(*((uint32_t*) &data[WS_PKT_DTYPE_OFFSET]));
	dlen = len - WS_PKT_DATA_OFFSET;
	return cmd_process_received_cmd(cmd_type, cmd_id, data_type, dlen, data + WS_PKT_DATA_OFFSET);
}


// Encode responses from the command response handlers into our tx_buffer
bool ws_cmd_send_handler(cmd_t cmd_type, cmd_id_t cmd_id, cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	uint32_t* tx32P;
	uint8_t* tx8P;
	
	if (tx_buffer_num_entries == WS_MAX_TX_PKTS) {
		ESP_LOGE(TAG, "TX Buffer full for ws_cmd_send_handler(%d, %d, %d, ...)", (int) cmd_type, (int) cmd_id, (int) data_type);
		return false;
	}
	
	// Atomically get and fill a buffer
	xSemaphoreTake(tx_mutex, portMAX_DELAY);
	tx32P = (uint32_t*) tx_buffer[tx_buffer_push_index].buf;
	tx8P = tx_buffer[tx_buffer_push_index].buf + WS_PKT_DATA_OFFSET;
	tx_buffer[tx_buffer_push_index].len = WS_PKT_DATA_OFFSET + len;
	if (++tx_buffer_push_index == WS_MAX_TX_PKTS) tx_buffer_push_index = 0;
	tx_buffer_num_entries += 1;
	
	// Add the fields, in order, to the websocket packet in network byte order
	*tx32P++ = htonl(WS_PKT_DATA_OFFSET + len);
	*tx32P++ = htonl((uint32_t) cmd_type);
	*tx32P++ = htonl((uint32_t) cmd_id);
	*tx32P   = htonl((uint32_t) data_type);
	
	// Add data if it exists
	while (len--) {
		*tx8P++ = *data++;
	}
	
	xSemaphoreGive(tx_mutex);
	
	return true;
}


// Directly encode a t1c_buffer_t buffer as a command response into our tx_buffer
bool ws_cmd_send_t1c_image(t1c_buffer_t* t1cP)
{
	uint32_t* tx32P;
	uint8_t* tx8P;
	uint32_t dlen;
	int push_index;
	
	if (tx_buffer_num_entries == WS_MAX_TX_PKTS) {
		ESP_LOGE(TAG, "TX Buffer full for ws_cmd_send_t1c_image");
		return false;
	}
	
	// Atomically get and fill a buffer
	xSemaphoreTake(tx_mutex, portMAX_DELAY);
	tx32P = (uint32_t*) tx_buffer[tx_buffer_push_index].buf;
	tx8P = tx_buffer[tx_buffer_push_index].buf + WS_PKT_DATA_OFFSET;
	push_index = tx_buffer_push_index;
	if (++tx_buffer_push_index == WS_MAX_TX_PKTS) tx_buffer_push_index = 0;
	tx_buffer_num_entries += 1;
	
	// Start by adding the t1c data to the data area
	dlen = _serialize_t1c_buffer(t1cP, tx8P);
	
	// Set the length of the websocket packet
	tx_buffer[push_index].len = WS_PKT_DATA_OFFSET + dlen;
	
	// Finally add the websocket packet fields in network byte order
	*tx32P++ = htonl(WS_PKT_DATA_OFFSET + dlen);
	*tx32P++ = htonl((uint32_t) CMD_SET);
	*tx32P++ = htonl((uint32_t) CMD_IMAGE);
	*tx32P   = htonl((uint32_t) CMD_DATA_BINARY);
	
	xSemaphoreGive(tx_mutex);
	
	return true;
}


bool ws_cmd_send_file_image(uint32_t* rgb888)
{
	uint32_t* tx32P;
	uint8_t* tx8P;
	uint32_t dlen;
	int push_index;
	
	if (tx_buffer_num_entries == WS_MAX_TX_PKTS) {
		ESP_LOGE(TAG, "TX Buffer full for ws_cmd_send_file_image");
		return false;
	}
	
	// Atomically get and fill a buffer
	xSemaphoreTake(tx_mutex, portMAX_DELAY);
	tx32P = (uint32_t*) tx_buffer[tx_buffer_push_index].buf;
	tx8P = tx_buffer[tx_buffer_push_index].buf + WS_PKT_DATA_OFFSET;
	push_index = tx_buffer_push_index;
	if (++tx_buffer_push_index == WS_MAX_TX_PKTS) tx_buffer_push_index = 0;
	tx_buffer_num_entries += 1;
	
	// Start by adding the pixel data to the data area
	dlen = 3*T1C_WIDTH*T1C_HEIGHT;
	memcpy(tx8P, (uint8_t*) rgb888, (size_t) dlen);
	
	// Set the length of the websocket packet
	tx_buffer[push_index].len = WS_PKT_DATA_OFFSET + dlen;
	
	// Finally add the websocket packet fields in network byte order
	*tx32P++ = htonl(WS_PKT_DATA_OFFSET + dlen);
	*tx32P++ = htonl((uint32_t) CMD_RSP);
	*tx32P++ = htonl((uint32_t) CMD_FILE_GET_IMAGE);
	*tx32P   = htonl((uint32_t) CMD_DATA_BINARY);
	
	xSemaphoreGive(tx_mutex);
	
	return true;
}



//
// Internal functions
//

// Serialize a t1c_buffer_t into a network order byte array and return the length.
// This is full-on custom code which must be reversed in the gui's rsp handler.  It
// has to change if the contents of t1c_buffer_t change.
static uint32_t _serialize_t1c_buffer(t1c_buffer_t* t1cP, uint8_t* data)
{
	uint8_t* dP = data;
	uint16_t* imgP = t1cP->img_data;
	uint32_t diff;
	uint32_t t32;
	
	// Lock access
	xSemaphoreTake(t1cP->mutex, portMAX_DELAY);
	
	// Boolean flags as bytes
	*dP++ = (uint8_t) t1cP->high_gain;
	*dP++ = (uint8_t) t1cP->vid_frozen;
	*dP++ = (uint8_t) t1cP->spot_valid;
	*dP++ = (uint8_t) t1cP->minmax_valid;
	*dP++ = (uint8_t) t1cP->region_valid;
	*dP++ = (uint8_t) t1cP->amb_temp_valid;
	*dP++ = (uint8_t) t1cP->amb_hum_valid;
	*dP++ = (uint8_t) t1cP->distance_valid;
	
	// Various data values
	dP = _add_i16(t1cP->amb_temp, dP);
	dP = _add_u16(t1cP->amb_hum, dP);
	dP = _add_u16(t1cP->distance, dP);
	dP = _add_u16(t1cP->spot_temp, dP);
	dP = _add_u16(t1cP->spot_point.x, dP);
	dP = _add_u16(t1cP->spot_point.y, dP);
	dP = _add_u16(t1cP->max_min_temp_info.min_temp, dP);
	dP = _add_u16(t1cP->max_min_temp_info.min_temp_point.x, dP);
	dP = _add_u16(t1cP->max_min_temp_info.min_temp_point.y, dP);
	dP = _add_u16(t1cP->max_min_temp_info.max_temp, dP);
	dP = _add_u16(t1cP->max_min_temp_info.max_temp_point.x, dP);
	dP = _add_u16(t1cP->max_min_temp_info.max_temp_point.y, dP);
	dP = _add_u16(t1cP->region_points.start_point.x, dP);
	dP = _add_u16(t1cP->region_points.start_point.y, dP);
	dP = _add_u16(t1cP->region_points.end_point.x, dP);
	dP = _add_u16(t1cP->region_points.end_point.y, dP);
	dP = _add_u16(t1cP->region_temp_info.temp_info_value.ave_temp, dP);
	dP = _add_u16(t1cP->region_temp_info.temp_info_value.min_temp, dP);
	dP = _add_u16(t1cP->region_temp_info.min_temp_point.x, dP);
	dP = _add_u16(t1cP->region_temp_info.min_temp_point.y, dP);
	dP = _add_u16(t1cP->region_temp_info.temp_info_value.max_temp, dP);
	dP = _add_u16(t1cP->region_temp_info.max_temp_point.x, dP);
	dP = _add_u16(t1cP->region_temp_info.max_temp_point.y, dP);
	
	// Add the raw image data, scaling it to 8-bits
	diff = t1cP->y16_max - t1cP->y16_min;
	if (diff == 0) diff = 1;
	while (imgP < (t1cP->img_data + T1C_WIDTH*T1C_HEIGHT)) {
		t32 = ((uint32_t)(*imgP++ - t1cP->y16_min) * 255) / diff;
		*dP++ = ((t32 > 255) ? 255 : (uint8_t) t32);
	}
	
	// Unlock
	xSemaphoreGive(t1cP->mutex);
	
	return (dP - data);
}


static uint8_t* _add_i16(int16_t data, uint8_t* buf)
{
	// Network order - big endian
	*buf++ = data >> 8;
	*buf++ = data & 0xFF;
	
	return buf;
}


static uint8_t* _add_u16(uint16_t data, uint8_t* buf)
{
	// Network order - big endian
	*buf++ = data >> 8;
	*buf++ = data & 0xFF;
	
	return buf;
}

#endif /* CONFIG_BUILD_ICAM_MINI */
