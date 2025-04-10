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
#ifndef CMD_UTILITIES_H
#define CMD_UTILITIES_H

#include "cmd_list.h"
#include <stdbool.h>
#include <stdint.h>



//
// Typedefs
//

// Command packet type
typedef enum {
	CMD_GET = 1,
	CMD_SET,
	CMD_RSP
} cmd_t;

// Command packet data types
typedef enum {
	CMD_DATA_NONE = 1,
	CMD_DATA_INT32,
	CMD_DATA_STRING,
	CMD_DATA_BINARY
} cmd_data_t;

// Application-specific handler for one of the packet types
typedef void (*cmd_handler)(cmd_data_t data_type, uint32_t len, uint8_t* data);

// Application-specific handler called when a packet is sent (transport specific)
typedef bool (*cmd_send_handler)(cmd_t cmd_type, cmd_id_t cmd_id, cmd_data_t data_type, uint32_t len, uint8_t* data);



//
// API
//
bool cmd_init_local();
bool cmd_init_remote(cmd_send_handler sender);
bool cmd_process_received_cmd(cmd_t cmd_type, cmd_id_t cmd_id, cmd_data_t data_type, uint32_t len, uint8_t* data);
bool cmd_register_cmd_id(cmd_id_t cmd_id, cmd_handler get_handler, cmd_handler set_handler, cmd_handler rsp_handler);

// Core packet send API
bool cmd_send(cmd_t cmd_type, cmd_id_t cmd_id);
bool cmd_send_int32(cmd_t cmd_type, cmd_id_t cmd_id, int32_t val);
bool cmd_send_string(cmd_t cmd_type, cmd_id_t cmd_id, char* val);
bool cmd_send_binary(cmd_t cmd_type, cmd_id_t cmd_id, uint32_t len, uint8_t* val);

// Utility API
bool cmd_send_marker_location(cmd_t cmd_type, cmd_id_t cmd_id, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
bool cmd_decode_marker_location(uint32_t len, uint8_t* data, uint16_t* x1, uint16_t* y1, uint16_t* x2, uint16_t* y2);

bool cmd_send_file_indicies(cmd_t cmd_type, cmd_id_t cmd_id, int dir_index, int file_index);
bool cmd_decode_file_indicies(uint32_t len, uint8_t* data, int* dir_index, int* file_index);

#endif /* CMD_UTILITIES_H */
