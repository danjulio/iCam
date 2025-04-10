/*
 * System related utilities
 *
 * Contains functions to initialize the system, other utility functions and a set
 * of globally available handles for the various tasks (to use for task notifications).
 *
 * Copyright 2020-2024 Dan Julio
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
 *
 */
#include "ctrl_task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i2cs.h"
#include "file_utilities.h"
#include "ps_utilities.h"
#include "out_state_utilities.h"
#include "sys_utilities.h"
#include "time_utilities.h"
#include "wifi_utilities.h"
#include "system_config.h"
#include "tiny1c.h"
#include <string.h>

#ifdef CONFIG_BUILD_ICAM_MINI
	#include "vid_render.h"
#else
	#include "i2cg.h"
#endif


//
// System Utilities internal constants
//



//
// System Utilities variables
//
static const char* TAG = "sys";


//
// Task handle externs for use by tasks to communicate with each other
//
#ifdef CONFIG_BUILD_ICAM_MINI
	TaskHandle_t task_handle_ctrl;
	TaskHandle_t task_handle_env;
	TaskHandle_t task_handle_file;
	TaskHandle_t task_handle_t1c;
	TaskHandle_t task_handle_vid;
	TaskHandle_t task_handle_web;
#else
	TaskHandle_t task_handle_env;
	TaskHandle_t task_handle_file;
	TaskHandle_t task_handle_gcore;
	TaskHandle_t task_handle_gui;
	TaskHandle_t task_handle_t1c;
#endif

#ifdef INCLUDE_SYS_MON
TaskHandle_t task_handle_mon;
#endif



//
// Global buffer pointers for memory allocated in the external SPIRAM
//

// Shared memory data structures
uint16_t* t1c_y16_buffer;         // Holds image read from camera module

t1c_buffer_t out_t1c_buffer[2];     // Ping-pong buffer loaded by t1c_task for the output task
t1c_buffer_t file_t1c_buffer;       // Buffer loaded by t1c_task for the file task
t1c_param_metadata_t file_t1c_meta; // Loaded by t1c_task for the file task

#ifdef CONFIG_BUILD_ICAM_MINI
uint8_t* rend_fbP[2];             // Ping-pong rendering buffers for vid_task
#endif

uint32_t* rgb_save_image;         // Buffer to render a 24-bit color image into for compression to jpeg

#ifdef CONFIG_BUILD_ICAM_MINI
uint32_t* rgb_file_image;         // Buffer to render a 24-bit color image to from jpeg decompression
#else
uint16_t* rgb_file_image;         // Buffer to render a 16-bit color image to from jpeg decompression
#endif

// Pointer to filesystem information structure (catalog)
void* file_info_bufferP;


//
// System Utilities API
//

/**
 * Initialize the ESP32 GPIO and internal peripherals
 */
bool system_esp_io_init()
{
	esp_err_t ret;
	
	ESP_LOGI(TAG, "ESP32 Peripheral Initialization");	
	
	// Attempt to initialize the I2C Master(s)
	ret = i2c_sensor_init(BRD_I2C_SENSOR_SCL_IO, BRD_I2C_SENSOR_SDA_IO);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Sensor I2C Master initialization failed - %d", ret);
		return false;
	}
	
#ifndef CONFIG_BUILD_ICAM_MINI
	ret = i2c_gcore_init(BRD_I2C_GCORE_SCL_IO, BRD_I2C_GCORE_SDA_IO);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "gCore I2C Master initialization failed - %d", ret);
		return false;
	}
#endif
	
#ifdef BRD_DIAG_IO
	// Initialize the diagnostic output pin
	gpio_reset_pin(BRD_DIAG_IO);
	gpio_set_direction(BRD_DIAG_IO, GPIO_MODE_OUTPUT);
	gpio_set_level(BRD_DIAG_IO, 0);
#endif

	return true;
}


/**
 * Initialize the board-level peripheral subsystems
 */
bool system_peripheral_init(bool init_wifi)
{
	ESP_LOGI(TAG, "System Peripheral Initialization");
	
	time_init();
	
	if (!ps_init()) {
		ESP_LOGE(TAG, "Persistent Storage initialization failed");
		return false;
	}
	
	// Setup the initial GUI state (controller side)
	out_state_init();
	
	if (!file_init_driver()) {
		ESP_LOGE(TAG, "SD Card driver initialization failed");
		return false;
	}
	
	if (init_wifi) {
		if (!wifi_init()) {
			ESP_LOGE(TAG, "WiFi initialization failed");
			return false;
		}
	}
	
	// Reset the Tiny1C (and, optionally, and other sensors)
	gpio_set_direction(BRD_SENSOR_RSTN_IO, GPIO_MODE_OUTPUT);
	gpio_set_level(BRD_SENSOR_RSTN_IO, 0);
	vTaskDelay(pdMS_TO_TICKS(10));
	gpio_set_level(BRD_SENSOR_RSTN_IO, 1);
	gpio_set_direction(BRD_SENSOR_RSTN_IO, GPIO_MODE_INPUT);
	
	return true;
}


/**
 * Allocate shared buffers for use by tasks for image data in the external RAM
 */
bool system_buffer_init(bool init_vid_buffers)
{
	ESP_LOGI(TAG, "Buffer Allocation");
	
	// Allocate the image read buffer
	t1c_y16_buffer = (uint16_t*) heap_caps_malloc(T1C_WIDTH*T1C_HEIGHT*2, MALLOC_CAP_SPIRAM);
	if (t1c_y16_buffer == NULL) {
		ESP_LOGE(TAG, "malloc image buffer failed");
		return false;
	}
	
	// Allocate the ping/pong t1c->output task Tiny1C buffers in the external RAM
	for (int i=0; i<2; i++) {
		memset(&out_t1c_buffer[i], 0, sizeof(t1c_buffer_t));
		out_t1c_buffer[i].img_data = (uint16_t*) heap_caps_malloc(T1C_WIDTH*T1C_HEIGHT*2, MALLOC_CAP_SPIRAM);
		if (out_t1c_buffer[i].img_data == NULL) {
			ESP_LOGE(TAG, "malloc VID shared image buffer %d failed", i);
			return false;
		}
		out_t1c_buffer[i].mutex = xSemaphoreCreateMutex();
	}
	
	// Allocate the t1c->file task buffers
	memset(&file_t1c_buffer, 0, sizeof(t1c_buffer_t));
	file_t1c_buffer.img_data = (uint16_t*) heap_caps_malloc(T1C_WIDTH*T1C_HEIGHT*2, MALLOC_CAP_SPIRAM);
	if (file_t1c_buffer.img_data == NULL) {
		ESP_LOGE(TAG, "malloc file save image buffer failed");
		return false;
	}
	file_t1c_buffer.mutex = xSemaphoreCreateMutex();
	
	// Allocate the rending frame buffer for raw 24-bit RGB images for conversion to jpeg
	rgb_save_image = (uint32_t*) heap_caps_malloc(T1C_WIDTH*T1C_HEIGHT*4, MALLOC_CAP_SPIRAM);
	if (rgb_save_image == NULL) {
		ESP_LOGE(TAG, "malloc save RGB buffer failed");
		return false;
	}
	
#ifdef CONFIG_BUILD_ICAM_MINI
	if (init_vid_buffers) {
		// Create the ping-pong video rendering buffers
		rend_fbP[0] = heap_caps_malloc(IMG_BUF_WIDTH*IMG_BUF_HEIGHT, MALLOC_CAP_SPIRAM);
		if (rend_fbP[0] == NULL) {
			ESP_LOGE(TAG, "create vid rendering buffer 0 failed");
			return false;
		}
		rend_fbP[1] = heap_caps_malloc(IMG_BUF_WIDTH*IMG_BUF_HEIGHT, MALLOC_CAP_SPIRAM);
		if (rend_fbP[1] == NULL) {
			ESP_LOGE(TAG, "create vid rendering buffer 1 failed");
			return false;
		}
	}
#endif
	
	// Allocate the filesystem information buffer in external RAM
	file_info_bufferP = heap_caps_malloc(FILE_INFO_BUFFER_LEN, MALLOC_CAP_SPIRAM);
	if (file_info_bufferP == NULL) {
		ESP_LOGE(TAG, "malloc filesystem information buffer failed");
		return false;
	}
	
#ifdef CONFIG_BUILD_ICAM_MINI
	if (!init_vid_buffers) {
		// 24-bit RGB from jpeg decoder
		rgb_file_image = heap_caps_malloc(T1C_WIDTH*T1C_HEIGHT*3, MALLOC_CAP_SPIRAM);
		if (rgb_file_image == NULL) {
			ESP_LOGE(TAG, "create file image failed");
		}
	}
#else
	// 16-bit RGB from jpeg decoder
	rgb_file_image = heap_caps_malloc(T1C_WIDTH*T1C_HEIGHT*2, MALLOC_CAP_SPIRAM);
	if (rgb_file_image == NULL) {
		ESP_LOGE(TAG, "create file image failed");
	}
#endif
	
	return true;
}
