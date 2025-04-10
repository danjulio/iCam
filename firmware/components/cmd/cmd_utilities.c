/*
 * Websocket command utilities.  Routines to pack and unpack data sent over the
 * websocket.  Designed to be built by both the ESP32 IDF and emscripten build
 * tools for use on both sides of the interface.
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
#ifdef ESP_PLATFORM
	#include "esp_system.h"
	#include "esp_log.h"
	#include "esp_heap_caps.h"
#else
	#include <stdlib.h>
#endif
#include "cmd_utilities.h"
#include <stdio.h>
#include <string.h>


//
// Private typedefs
//
// Handlers for each packet id (one handler per packet type)
typedef struct {
	cmd_handler get_handler;
	cmd_handler set_handler;
	cmd_handler rsp_handler;
} cmd_handler_t;




//
// Internal variables
//
static const char* TAG = "cmd_utilities";

static bool is_local = false;
static cmd_send_handler send_handler;
static cmd_handler_t* cmd_id_list;



//
// API
//
bool cmd_init_local()
{
	// Allocate space for the command list and zero it so each handler is NULL by default
#ifdef ESP_PLATFORM
	cmd_id_list = (cmd_handler_t*) heap_caps_calloc(CMD_TOTAL_COUNT, sizeof(cmd_handler_t), MALLOC_CAP_32BIT);
	if (cmd_id_list == NULL) {
		ESP_LOGE(TAG, "Failed to allocate memory for the command list");
		return false;
	}
#else
	cmd_id_list = (cmd_handler_t*) calloc(CMD_TOTAL_COUNT, sizeof(cmd_handler_t));
	if (cmd_id_list == NULL) {
		printf("%s Failed to allocate memory for the command list\n", TAG);
		return false;
	}
#endif
	
	is_local = true;
	
	return true;
}


bool cmd_init_remote(cmd_send_handler sender)
{
	if (sender == NULL) {
#ifdef ESP_PLATFORM
		ESP_LOGE(TAG, "No send handler specified");
#else
		printf("%s No send handler specified", TAG);
#endif
		return false;
	}
	
	// Allocate space for the command list and zero it so each handler is NULL by default
#ifdef ESP_PLATFORM
	cmd_id_list = (cmd_handler_t*) heap_caps_calloc(CMD_TOTAL_COUNT, sizeof(cmd_handler_t), MALLOC_CAP_32BIT);
	if (cmd_id_list == NULL) {
		ESP_LOGE(TAG, "Failed to allocate memory for the command list");
		return false;
	}
#else
	cmd_id_list = (cmd_handler_t*) calloc(CMD_TOTAL_COUNT, sizeof(cmd_handler_t));
	if (cmd_id_list == NULL) {
		printf("%s Failed to allocate memory for the command list\n", TAG);
		return false;
	}
#endif
	
	is_local = false;
	send_handler = sender;
	
	return true;
}


bool cmd_process_received_cmd(cmd_t cmd_type, cmd_id_t cmd_id, cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	bool null_handler = false;
	
	if (cmd_id > CMD_TOTAL_COUNT) {
#ifdef ESP_PLATFORM
		ESP_LOGE(TAG, "No handler for received cmd %ul", cmd_id);
#else
		printf("%s No handler for received cmd %ul\n", TAG, cmd_id);
#endif
		return false;
	}
	
	switch (cmd_type) {
		case CMD_GET:
			if (cmd_id_list[cmd_id].get_handler != NULL) {
				cmd_id_list[cmd_id].get_handler(data_type, len, data);
			} else {
				null_handler = true;
			}
			break;
		case CMD_SET:
			if (cmd_id_list[cmd_id].set_handler != NULL) {
				cmd_id_list[cmd_id].set_handler(data_type, len, data);
			} else {
				null_handler = true;
			}
			break;
		case CMD_RSP:
			if (cmd_id_list[cmd_id].rsp_handler != NULL) {
				cmd_id_list[cmd_id].rsp_handler(data_type, len, data);
			} else {
				null_handler = true;
			}
			break;
	}
	
	if (null_handler) {
#ifdef ESP_PLATFORM
		ESP_LOGE(TAG, "No handler for received type: %d, id: %d, data_type: %d, len: %d", (int) cmd_type, (int) cmd_id, (int) data_type, (int) len);
#else
		printf("%s No handler for received type: %d, id: %d, data_type: %d, len: %d\n", TAG, (int) cmd_type, (int) cmd_id, (int) data_type, (int) len);
#endif
		return false;
	}
	
	return true;
}


bool cmd_register_cmd_id(cmd_id_t cmd_id, cmd_handler get_handler, cmd_handler set_handler, cmd_handler rsp_handler)
{
	if (cmd_id > CMD_TOTAL_COUNT) {
#ifdef ESP_PLATFORM
		ESP_LOGE(TAG, "Attempt to register illegal command %ul", cmd_id);
#else
		printf("%s Attempt to register illegal command %ul\n", TAG, cmd_id);
#endif
		return false;
	}
	
	cmd_id_list[cmd_id].get_handler = get_handler;
	cmd_id_list[cmd_id].set_handler = set_handler;
	cmd_id_list[cmd_id].rsp_handler = rsp_handler;
	
	return true;
}


bool cmd_send(cmd_t cmd_type, cmd_id_t cmd_id)
{
	if (is_local) {
		return cmd_process_received_cmd(cmd_type, cmd_id, CMD_DATA_NONE, 0, NULL);
	} else {
		return send_handler(cmd_type, cmd_id, CMD_DATA_NONE, 0, NULL);
	}
}


bool cmd_send_int32(cmd_t cmd_type, cmd_id_t cmd_id, int32_t val)
{
	uint32_t t32;
	
	t32 = htonl((uint32_t) val);
	
	if (is_local) {
		return cmd_process_received_cmd(cmd_type, cmd_id, CMD_DATA_INT32, 4, (uint8_t*) &t32);
	} else {
		return send_handler(cmd_type, cmd_id, CMD_DATA_INT32, 4, (uint8_t*) &t32);
	}
}


bool cmd_send_string(cmd_t cmd_type, cmd_id_t cmd_id, char* val)
{
	if (is_local) {
		return cmd_process_received_cmd(cmd_type, cmd_id, CMD_DATA_STRING, strlen(val)+1, (uint8_t*) val);
	} else {
		return send_handler(cmd_type, cmd_id, CMD_DATA_STRING, strlen(val)+1, (uint8_t*) val);
	}
}


bool cmd_send_binary(cmd_t cmd_type, cmd_id_t cmd_id, uint32_t len, uint8_t* val)
{
	if (is_local) {
		return cmd_process_received_cmd(cmd_type, cmd_id, CMD_DATA_BINARY, len, val);
	} else {
		return send_handler(cmd_type, cmd_id, CMD_DATA_BINARY, len, val);
	}
}


bool cmd_send_marker_location(cmd_t cmd_type, cmd_id_t cmd_id, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint8_t array[8];
	
	// First dword contains {x1, y1}
	*((uint32_t*) &array[0]) = htonl((x1 << 16) | y1);
	
	// Second dword contains {x2, y2}
	*((uint32_t*) &array[4]) = htonl((x2 << 16) | y2);
	
	if (is_local) {
		return cmd_process_received_cmd(cmd_type, cmd_id, CMD_DATA_BINARY, 8, array);
	} else {
		return send_handler(cmd_type, cmd_id, CMD_DATA_BINARY, 8, array);
	}
}


bool cmd_decode_marker_location(uint32_t len, uint8_t* data, uint16_t* x1, uint16_t* y1, uint16_t* x2, uint16_t* y2)
{
	uint32_t t;
	
	if (len != 8) return false;
	
	// First dword contains {x1, y1}
	t = ntohl(*((uint32_t*) &data[0]));
	*x1 = t >> 16;
	*y1 = t & 0x0000FFFF;
	
	// Second dword contains {x2, y2}
	t = ntohl(*((uint32_t*) &data[4]));
	*x2 = t >> 16;
	*y2 = t & 0x0000FFFF;
	
	return true;
}


bool cmd_send_file_indicies(cmd_t cmd_type, cmd_id_t cmd_id, int dir_index, int file_index)
{
	uint32_t t;
	
	t = (((uint16_t) dir_index) << 16) | (((uint16_t) file_index));
	return cmd_send_int32(cmd_type, cmd_id, (int32_t) t);
}


bool cmd_decode_file_indicies(uint32_t len, uint8_t* data, int* dir_index, int* file_index)
{
	uint32_t t;
	
	if (len != 4) return false;
	
	t = ntohl(*((uint32_t*) &data[0]));
	*dir_index = (int) (t >> 16);
	*file_index = (int) ((int16_t)(t & 0xFFFF));
	
	return true;
}
