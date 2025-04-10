/*
 * Websocket command handlers for use by the web interface.
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
#include "cmd_list.h"
#include "cmd_utilities.h"
#include "gui_cmd_handlers.h"
#include "gui_main.h"
#include "web_cmd_utilities.h"
#include <emscripten/emscripten.h>
#include <stdio.h>
#include <stdlib.h>




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


//
// Local variables
//
static const char* TAG = "web_cmd_utilities";

// TX buffer over-sized to hold various SET packets
static uint8_t tx_buffer[1024];
static uint32_t tx_len = 0;

// Our web socket
EMSCRIPTEN_WEBSOCKET_T web_socket;


//
// Forward declarations for internal functions
//
static void _cmd_handler_set_shutdown(cmd_data_t data_type, uint32_t len, uint8_t* data);



//
// API
//
bool web_cmd_init()
{
	// Initialize the command system
	if (!cmd_init_remote(web_cmd_send_handler)) {
		return false;
	}
	
	// Register command handlers supported on our end (get, set, rsp)
	(void) cmd_register_cmd_id(CMD_AMBIENT_CORRECT, NULL, NULL, cmd_handler_rsp_ambient_correct);
	(void) cmd_register_cmd_id(CMD_BATT_LEVEL, NULL, NULL, cmd_handler_rsp_batt_info);
	(void) cmd_register_cmd_id(CMD_BRIGHTNESS, NULL, NULL, cmd_handler_rsp_brightness);
	(void) cmd_register_cmd_id(CMD_CRIT_BATT, NULL, cmd_handler_set_critical_batt, NULL);
	(void) cmd_register_cmd_id(CMD_CTRL_ACTIVITY, NULL, NULL, cmd_handler_rsp_ctrl_activity);
	(void) cmd_register_cmd_id(CMD_CARD_PRESENT, NULL, NULL, cmd_handler_rsp_card_present);
	(void) cmd_register_cmd_id(CMD_EMISSIVITY, NULL, NULL, cmd_handler_rsp_emissivity);
	(void) cmd_register_cmd_id(CMD_FILE_CATALOG, NULL, NULL, cmd_handler_rsp_file_catalog);
	(void) cmd_register_cmd_id(CMD_FILE_GET_IMAGE, NULL, NULL, cmd_handler_rsp_file_image);
	(void) cmd_register_cmd_id(CMD_GAIN, NULL, NULL, cmd_handler_rsp_gain);
	(void) cmd_register_cmd_id(CMD_IMAGE, NULL, cmd_handler_set_image, NULL);
	(void) cmd_register_cmd_id(CMD_MIN_MAX_EN, NULL, NULL, cmd_handler_rsp_min_max_en);
	(void) cmd_register_cmd_id(CMD_MSG_ON, NULL, cmd_handler_set_msg_on, NULL);
	(void) cmd_register_cmd_id(CMD_MSG_OFF, NULL, cmd_handler_set_msg_off, NULL);
	(void) cmd_register_cmd_id(CMD_PALETTE, NULL, NULL, cmd_handler_rsp_palette);
	(void) cmd_register_cmd_id(CMD_REGION_EN, NULL, NULL, cmd_handler_rsp_region_enable);
	(void) cmd_register_cmd_id(CMD_SAVE_OVL_EN, NULL, NULL, cmd_handler_rsp_save_ovl_en);
	(void) cmd_register_cmd_id(CMD_SHUTTER_INFO, NULL, NULL, cmd_handler_rsp_shutter);
	(void) cmd_register_cmd_id(CMD_SHUTDOWN, NULL, _cmd_handler_set_shutdown, NULL);
	(void) cmd_register_cmd_id(CMD_SPOT_EN, NULL, NULL, cmd_handler_rsp_spot_enable);
	(void) cmd_register_cmd_id(CMD_SYS_INFO, NULL, NULL, cmd_handler_rsp_sys_info);
	(void) cmd_register_cmd_id(CMD_TIME, NULL, NULL, cmd_handler_rsp_time);
	(void) cmd_register_cmd_id(CMD_TIMELAPSE_STATUS, NULL, cmd_handler_set_timelapse_status, NULL);
	(void) cmd_register_cmd_id(CMD_UNITS, NULL, NULL, cmd_handler_rsp_units);
	(void) cmd_register_cmd_id(CMD_WIFI_INFO, NULL, NULL, cmd_handler_rsp_wifi);
	
	return true;
}


void web_cmd_register_socket(EMSCRIPTEN_WEBSOCKET_T socket)
{
	web_socket = socket;
}


bool web_cmd_process_socket_rx_data(uint32_t len, uint8_t* data)
{
	cmd_t cmd_type;
	cmd_id_t cmd_id;
	cmd_data_t data_type;
	uint32_t dlen;
	
	// Make sure received data contains at least the minimum cmd arguments
	if (len < MIN_WS_PKT_LEN) {
		printf("%s Illegal websocket packet length %u\n", TAG, len);
		return false;
	}
	
	// Make sure the received data length matches what the cmd says its length is
	dlen = ntohl(*((uint32_t*) &data[WS_PKT_LEN_OFFSET]));
	if (len != dlen) {
		printf("%s websocket packet len %u does not match expected %u\n", TAG, len, dlen);
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
bool web_cmd_send_handler(cmd_t cmd_type, cmd_id_t cmd_id, cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if (web_socket == 0) {
		return false;
	}
	
	int buf_num = (cmd_type == CMD_RSP) ? 1 : 0;
	uint32_t* tx32P = (uint32_t*) tx_buffer;
	uint8_t* tx8P = &tx_buffer[0] + WS_PKT_DATA_OFFSET;
	EMSCRIPTEN_RESULT res;
	
	// Calculate length of websocket packet
	tx_len = MIN_WS_PKT_LEN + len;
	
	// Add the fields, in order, to the websocket packet in network byte order
	*tx32P++ = htonl(tx_len);
	*tx32P++ = htonl((uint32_t) cmd_type);
	*tx32P++ = htonl((uint32_t) cmd_id);
	*tx32P++ = htonl((uint32_t) data_type);
	
	// Add data if it exists
	while (len--) {
		*tx8P++ = *data++;
	}
	
	res = emscripten_websocket_send_binary(web_socket, (void*) tx_buffer, tx_len);
	if (res != EMSCRIPTEN_RESULT_SUCCESS) {
		printf("%s websocket send failed with %d\n", TAG, res);
		return false;
	}
	
	return true;
}


//
// Internal functions
//

// Web-specific handling of shutdown command
static void _cmd_handler_set_shutdown(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_NONE) && (len == 0)) {
		// Let the GUI controller know so it can do what it needs to do on shutdown
		gui_main_shutdown();
	}
}
