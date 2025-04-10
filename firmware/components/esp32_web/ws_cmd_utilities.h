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
#ifndef WS_CMD_UTILITIES_H
#define WS_CMD_UTILITIES_H

#include "cmd_utilities.h"
#include <stdbool.h>
#include <stdint.h>



//
// API
//
bool ws_gui_cmd_init();

// web socket interface
bool ws_cmd_get_tx_data(uint32_t* len, uint8_t** data); // Valid data when returning true
uint8_t* ws_cmd_get_rx_data_buffer();
bool ws_cmd_process_socket_rx_data(uint32_t len, uint8_t* data);

// cmd_utilities send handler
bool ws_cmd_send_handler(cmd_t cmd_type, cmd_id_t cmd_id, cmd_data_t data_type, uint32_t len, uint8_t* data);

// Custom send utilities
bool ws_cmd_send_t1c_image(t1c_buffer_t* t1cP);
bool ws_cmd_send_file_image(uint32_t* rgb888);


#endif /* WS_CMD_UTILITIES_H */
