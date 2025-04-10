/*
 * Web Task - implement simple web server to send compressed single webpage app
 * to connected browser and then manage communications with the page over a websocket.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmd_handlers.h"
#include "cmd_list.h"
#include "cmd_utilities.h"
#include "ctrl_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "file_task.h"
#include "file_utilities.h"
#include "out_state_utilities.h"
#include "palettes.h"
#include "sys_utilities.h"
#include "tiny1c.h"
#include "ws_cmd_utilities.h"
#include "web_task.h"
#include "wifi_utilities.h"


//
// WEB Task constants
//

// Maximum number of connections
#define max_sockets 3



//
// WEB Task typedefs
//

// Command packet data types
typedef enum {
	SEND_CMD_FW_UPD_EN,
	SEND_CMD_FW_UPD_END,
	SEND_CMD_CRIT_BATT,
	SEND_CMD_SHUTDOWN,
	SEND_CMD_FILE_MSG_ON,
	SEND_CMD_FILE_MSG_OFF,
	SEND_CMD_FILE_CATALOG,
	SEND_CMD_FILE_IMAGE,
	SEND_CMD_TIMELAPSE_ON,
	SEND_CMD_TIMELAPSE_OFF,
	SEND_CMD_CTRL_ACT_SUCCEEDED,
	SEND_CMD_CTRL_ACT_FAILED
} send_cmd_type_t;


//
// WEB Task variables
//
static const char* TAG = "web_task";

// State
static bool client_connected = false;

// Notifications (clear after use)
static bool notify_take_picture = false;
static bool notify_network_disconnect = false;
static bool notify_fw_upd_en = false;
static bool notify_fw_upd_end = false;
static bool notify_crit_batt = false;
static bool notify_shutdown = false;
static bool notify_disp_file_message = false;
static bool notify_clear_file_message = false;
static bool notify_image_1 = false;
static bool notify_image_2 = false;
static bool notify_catalog_response = false;
static bool notify_file_image_response = false;
static bool notify_timelapse_on = false;
static bool notify_timelapse_off = false;
static bool notify_ctrl_act_succeeded = false;
static bool notify_ctrl_act_failed = false;

// served web page and favicon
extern const uint8_t index_html_start[] asm("_binary_index_html_gz_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_gz_end");

extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");



//
// WEB Task Forward Declarations for internal functions
//
static void _web_handle_notifications();
static httpd_handle_t _web_start_webserver(void);
static esp_err_t _web_stop_webserver(httpd_handle_t server);
static void _web_connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void _web_disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t _web_req_handler(httpd_req_t *req);
static esp_err_t _web_favicon_handler(httpd_req_t *req);
static esp_err_t _web_ws_handler(httpd_req_t *req);
static void _web_send_cmd(httpd_handle_t handle, int sock, send_cmd_type_t cmd_type);
static void _web_send_image(httpd_handle_t handle, int sock, int render_buf_index);
static void _web_send_get_file_catalog_response();
static void _web_send_get_file_image_response();



//
// Web handler configuration
//
static const httpd_uri_t uri_get = {
        .uri        = "/",
        .method     = HTTP_GET,
        .handler    = _web_req_handler,
        .user_ctx   = NULL,
        .is_websocket = false
};

static const httpd_uri_t uri_get_favicon = {
        .uri        = "/favicon.ico",
        .method     = HTTP_GET,
        .handler    = _web_favicon_handler,
        .user_ctx   = NULL,
        .is_websocket = false
};

static const httpd_uri_t uri_ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = _web_ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};



//
// WEB Task API
//
void web_task()
{
	esp_err_t ret;
	size_t clients;
	int client_fds[max_sockets];
	int sock;
	static httpd_handle_t server = NULL;
	
	ESP_LOGI(TAG, "Start task");
	
	// Configure the save palette
	set_save_palette(out_state.gui_palette_index);
	
	// Initialize cmd interface
	if (!ws_gui_cmd_init()) {
		ESP_LOGE(TAG, "Could not initialize command interface");
		ctrl_set_fault_type(CTRL_FAULT_WEB_SERVER);
		vTaskDelete(NULL);
	}
	
	// Wait until we are connected to start the web server
	while (!wifi_is_connected()) {
		vTaskDelay(pdMS_TO_TICKS(100));
	}
	
	// Register even handlers to stop the server when Wifi is disconnected and
	// start it again upon connection
	(void) esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_web_connect_handler, &server);
	(void) esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &_web_disconnect_handler, &server);
	
	// Start the server for the first time
	if ((server = _web_start_webserver()) ==  NULL) {
		ESP_LOGE(TAG, "Could not start web server");
		ctrl_set_fault_type(CTRL_FAULT_WEB_SERVER);
		vTaskDelete(NULL);
	}

	
	while (1) {
		_web_handle_notifications();

		// Give the scheduler some time between images
		if (!notify_image_1 && !notify_image_2) {
			vTaskDelay(pdMS_TO_TICKS(10));
		}
		
		// Look for notifications that we've been asked to take a picture from
		//   - local button press via ctrl_task
		//   - command from GUI button press
		if (notify_take_picture || cmd_handler_take_picture_notification()) {
			// Trigger an image save and wait to be given the filename or message to display
			xTaskNotify(task_handle_file, FILE_NOTIFY_SAVE_JPG_MASK, eSetBits);
		}
		
		if (server != NULL) {
			// Look for things to send to a connected client over the websocket
			clients = max_sockets;
			if ((ret = httpd_get_client_list(server, &clients, client_fds)) == ESP_OK) {
				client_connected = (clients != 0);
				for (int i=0; i<clients; i++) {
					sock = client_fds[i];
					if (httpd_ws_get_fd_info(server, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
						// Look for things to send
						if (notify_network_disconnect) {
							ret = httpd_sess_trigger_close(server, sock);
							if (ret != ESP_OK) {
								ESP_LOGE(TAG, "Couldn't close connection (%d)", ret);
							}
						}
	
						if (notify_fw_upd_en) {
							_web_send_cmd(server, sock, SEND_CMD_FW_UPD_EN);
						}
						
						if (notify_fw_upd_end) {
							_web_send_cmd(server, sock, SEND_CMD_FW_UPD_END);
						}
						
						if (notify_crit_batt) {
							_web_send_cmd(server, sock, SEND_CMD_CRIT_BATT);
						}
						
						if (notify_shutdown) {
							_web_send_cmd(server, sock, SEND_CMD_SHUTDOWN);
						}
						
						if (notify_disp_file_message) {
							_web_send_cmd(server, sock, SEND_CMD_FILE_MSG_ON);
						}
						
						if (notify_clear_file_message) {
							_web_send_cmd(server, sock, SEND_CMD_FILE_MSG_OFF);
						}
						
						if (notify_ctrl_act_succeeded) {
							_web_send_cmd(server, sock, SEND_CMD_CTRL_ACT_SUCCEEDED);
						}
						
						if (notify_ctrl_act_failed) {
							_web_send_cmd(server, sock, SEND_CMD_CTRL_ACT_FAILED);
						}
						
						if (notify_catalog_response) {
							_web_send_cmd(server, sock, SEND_CMD_FILE_CATALOG);
						}
						
						if (notify_file_image_response) {
							_web_send_cmd(server, sock, SEND_CMD_FILE_IMAGE);
						}
						
						if (notify_timelapse_on) {
							_web_send_cmd(server, sock, SEND_CMD_TIMELAPSE_ON);
						}
						
						if (notify_timelapse_off) {
							_web_send_cmd(server, sock, SEND_CMD_TIMELAPSE_OFF);
						}
						
						if (notify_image_1 && cmd_handler_stream_enabled()) {
							_web_send_image(server, sock, 0);
						}
						
						if (notify_image_2 && cmd_handler_stream_enabled()) {
							_web_send_image(server, sock, 1);
						}
					}
				}
			} else {
				ESP_LOGE(TAG, "httpd_get_client_list failed (%d)", ret);
			}
		}
		
		// Clear notifications every time through loop to handle case where nothing
		// is connected and we ignore them
		notify_take_picture = false;
		notify_network_disconnect = false;
		notify_fw_upd_en = false;
		notify_fw_upd_end = false;
		notify_crit_batt = false;
		notify_shutdown = false;
		notify_disp_file_message = false;
		notify_clear_file_message = false;
		notify_ctrl_act_succeeded = false;
		notify_ctrl_act_failed = false;
		notify_catalog_response = false;
		notify_file_image_response = false;
		notify_timelapse_on = false;
		notify_timelapse_off = false;
		notify_image_1 = false;
		notify_image_2 = false;
	}
}


bool web_has_client()
{
	return client_connected;
}



//
// WEB Task Internal functions
//
static void _web_handle_notifications()
{
	uint32_t notification_value;
	
	notification_value = 0;
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, WEB_NOTIFY_T1C_FRAME_MASK_1)) {
			notify_image_1 = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_T1C_FRAME_MASK_2)) {
			notify_image_2 = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_TAKE_PICTURE_MASK)) {
			notify_take_picture = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_NETWORK_DISC_MASK)) {
			notify_network_disconnect = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_FW_UPD_EN_MASK)) {
			notify_fw_upd_en = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_FW_UPD_END_MASK)) {
			notify_fw_upd_end = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_CRIT_BATT_DET_MASK)) {
			notify_crit_batt = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_SHUTDOWN_MASK)) {
			notify_shutdown = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_FILE_MSG_ON_MASK)) {
			notify_disp_file_message = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_FILE_MSG_OFF_MASK)) {
			notify_clear_file_message = true;
		}
				
		if (Notification(notification_value, WEB_NOTIFY_FILE_CATALOG_READY_MASK)) {
			notify_catalog_response = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_FILE_IMAGE_READY_MASK)) {
			notify_file_image_response = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_FILE_TIMELAPSE_ON_MASK)) {
			notify_timelapse_on = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_FILE_TIMELAPSE_OFF_MASK)) {
			notify_timelapse_off = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_CTRL_ACT_SUCCEEDED_MASK)) {
			notify_ctrl_act_succeeded = true;
		}
		
		if (Notification(notification_value, WEB_NOTIFY_CTRL_ACT_FAILED_MASK)) {
			notify_ctrl_act_failed = true;
		}

	}
}


static httpd_handle_t _web_start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Setup our specific config items
    config.max_open_sockets = max_sockets;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_get_favicon);
        httpd_register_uri_handler(server, &uri_ws);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}


static esp_err_t _web_stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}


static void _web_connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = _web_start_webserver();
        if (*server == NULL) {
			ESP_LOGE(TAG, "Could not restart web server");
			ctrl_set_fault_type(CTRL_FAULT_WEB_SERVER);
			vTaskDelete(NULL);
        }
    }
}


static void _web_disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (_web_stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}


static esp_err_t _web_req_handler(httpd_req_t *req)
{
	uint32_t len = index_html_end - index_html_start;
	
	ESP_LOGI(TAG, "Sending index.html");
	
	if (httpd_resp_set_hdr(req, "Content-Encoding", "gzip") != ESP_OK) {
		ESP_LOGE(TAG, "set_hdr failed");
		return false;
	}
	
	return httpd_resp_send(req, (const char*) index_html_start, (ssize_t) len);
}


static esp_err_t _web_favicon_handler(httpd_req_t *req)
{
	uint32_t len = favicon_ico_end - favicon_ico_start;
	
	ESP_LOGI(TAG, "Sending favicon");
	
	(void) httpd_resp_set_type(req, "image/x-icon");
	
	return httpd_resp_send(req, (const char*) favicon_ico_start, (ssize_t) len);
}


static esp_err_t _web_ws_handler(httpd_req_t *req)
{
	esp_err_t ret;
	httpd_ws_frame_t ws_pkt;
	
	// Handle opening the websocket
	if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, socket opened");
        return ESP_OK;
    }
    
    // Look for incoming packets to process
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    
    if (ws_pkt.len) {
    	// Get and process the websocket packet
    	ws_pkt.payload = ws_cmd_get_rx_data_buffer();
    	ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            return ret;
        }
        
        // May push response data into the tx buffer
        (void) ws_cmd_process_socket_rx_data(ws_pkt.len, ws_pkt.payload);
        
        // Check for response data (from a GET)
        while (ws_cmd_get_tx_data((uint32_t*) &ws_pkt.len, &ws_pkt.payload)) {
        	// Send the response
        	ws_pkt.type = HTTPD_WS_TYPE_BINARY;
        	ws_pkt.final = true;
			ws_pkt.fragmented = false;
			ret = httpd_ws_send_frame(req, &ws_pkt);
		    if (ret != ESP_OK) {
		        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
		    }
        }
    }
    
    return ESP_OK;
}


static void _web_send_cmd(httpd_handle_t handle, int sock, send_cmd_type_t cmd_type)
{
	esp_err_t ret;
	httpd_ws_frame_t ws_pkt;
	
	if (handle == NULL) return;
	
	// Create the specific command to send
	switch (cmd_type) {
		case SEND_CMD_FW_UPD_EN:
			(void) cmd_send(CMD_SET, CMD_FW_UPD_EN);
			break;
		case SEND_CMD_FW_UPD_END:
			(void) cmd_send(CMD_SET, CMD_FW_UPD_END);
			break;
		case SEND_CMD_CRIT_BATT:
			(void) cmd_send(CMD_SET, CMD_CRIT_BATT);
			break;
		case SEND_CMD_SHUTDOWN:
			(void) cmd_send(CMD_SET, CMD_SHUTDOWN);
			break;
		case SEND_CMD_FILE_MSG_ON:
			(void) cmd_send_string(CMD_SET, CMD_MSG_ON, file_get_file_save_status_string());
			break;
		case SEND_CMD_FILE_MSG_OFF:
			(void) cmd_send(CMD_SET, CMD_MSG_OFF);
			break;
		case SEND_CMD_FILE_CATALOG:
			_web_send_get_file_catalog_response();
			break;
		case SEND_CMD_FILE_IMAGE:
			_web_send_get_file_image_response();
			break;
		case SEND_CMD_TIMELAPSE_ON:
			(void) cmd_send_int32(CMD_SET, CMD_TIMELAPSE_STATUS, 1);
			break;
		case SEND_CMD_TIMELAPSE_OFF:
			(void) cmd_send_int32(CMD_SET, CMD_TIMELAPSE_STATUS, 0);
			break;
		case SEND_CMD_CTRL_ACT_SUCCEEDED:
			(void) cmd_send_int32(CMD_RSP, CMD_CTRL_ACTIVITY, 1);
			break;
		case SEND_CMD_CTRL_ACT_FAILED:
			(void) cmd_send_int32(CMD_RSP, CMD_CTRL_ACTIVITY, 0);
			break;
	}
	
	// Synchronously send the packet
	while (ws_cmd_get_tx_data((uint32_t*) &ws_pkt.len, &ws_pkt.payload)) {
		ws_pkt.type = HTTPD_WS_TYPE_BINARY;
		ws_pkt.final = true;
		ws_pkt.fragmented = false;
		ret = httpd_ws_send_data(handle, sock, &ws_pkt);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "httpd_ws_send_data failed - %d", ret);
		}
	}
}


static void _web_send_image(httpd_handle_t handle, int sock, int render_buf_index)
{
	esp_err_t ret;
	t1c_buffer_t* t1cP = (render_buf_index == 0) ? &out_t1c_buffer[0] : &out_t1c_buffer[1];
	httpd_ws_frame_t ws_pkt;
	
	if (handle == NULL) return;
	
	(void) ws_cmd_send_t1c_image(t1cP);
	
	// Synchronously send the packet
	if (ws_cmd_get_tx_data((uint32_t*) &ws_pkt.len, &ws_pkt.payload)) {
		ws_pkt.type = HTTPD_WS_TYPE_BINARY;
		ws_pkt.final = true;
		ws_pkt.fragmented = false;
		ret = httpd_ws_send_data(handle, sock, &ws_pkt);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "httpd_ws_send_data image failed - %d", ret);
		}
	}
}


// web_task specific routine to send the catalog to a remote response handler
static void _web_send_get_file_catalog_response()
{
	char* catalog_list;
	int catalog_type;
	int num_entries;
	int16_t short_type;
	uint32_t t;
	
	// Get the catalog information from file_task
	catalog_list = file_get_catalog(&num_entries, &catalog_type);
	
	// First, send an integer with the type and number of entries packed in it
	//   Low 16 bits: type
	//   High 16 bits: number of entries
	short_type = (int16_t) catalog_type;
	t = ((uint16_t) short_type) | (((uint16_t) num_entries) << 16);
	(void) cmd_send_int32(CMD_RSP, CMD_FILE_CATALOG, (int32_t) t);
	
	// Then send the string
	(void) cmd_send_string(CMD_RSP, CMD_FILE_CATALOG, catalog_list);
}


// web_task specific routine to send an image to a remote response handler
static void _web_send_get_file_image_response()
{
	(void) ws_cmd_send_file_image(rgb_file_image);
}

#endif /* CONFIG_BUILD_ICAM_MINI */
