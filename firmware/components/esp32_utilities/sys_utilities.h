/*
 * System related utilities
 *
 * Contains functions to initialize the system, other utility functions, a set
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
#ifndef SYS_UTILITIES_H
#define SYS_UTILITIES_H

#include "esp_system.h"
#include "falcon_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "system_config.h"
#include "tiny1c.h"
#include <stdbool.h>
#include <stdint.h>



//
// System Utilities macros
//
#define Notification(var, mask) ((var & mask) == mask)



//
// Task handle externs for use by tasks to communicate with each other
//
#ifdef CONFIG_BUILD_ICAM_MINI
	extern TaskHandle_t task_handle_ctrl;
	extern TaskHandle_t task_handle_env;
	extern TaskHandle_t task_handle_file;
	extern TaskHandle_t task_handle_t1c;
	extern TaskHandle_t task_handle_vid;
	extern TaskHandle_t task_handle_web;
#else
	extern TaskHandle_t task_handle_env;
	extern TaskHandle_t task_handle_file;
	extern TaskHandle_t task_handle_gcore;
	extern TaskHandle_t task_handle_gui;
	extern TaskHandle_t task_handle_t1c;
#endif

#ifdef INCLUDE_SYS_MON
extern TaskHandle_t task_handle_mon;
#endif



//
// Global buffer pointers for allocated memory
//

// Shared memory data structures
extern uint16_t* t1c_y16_buffer;         // Holds image read from camera module

extern t1c_buffer_t out_t1c_buffer[2];     // Ping-pong buffer loaded by t1c_task for the output task
extern t1c_buffer_t file_t1c_buffer;       // Buffer loaded by t1c_task for the file task
extern t1c_param_metadata_t file_t1c_meta; // Loaded by t1c_task for the file task

#ifdef CONFIG_BUILD_ICAM_MINI
extern uint8_t* rend_fbP[2];             // Ping-pong rendering buffers for vid_task
#endif

extern uint32_t* rgb_save_image;         // Buffer to render a 24-bit color image into for compression to jpeg

#ifdef CONFIG_BUILD_ICAM_MINI
extern uint32_t* rgb_file_image;         // Buffer to render a 24-bit color image to from jpeg decompression
#else
extern uint16_t* rgb_file_image;         // Buffer to render a 16-bit color image to from jpeg decompression
#endif

// Pointer to filesystem information structure (catalog)
extern void* file_info_bufferP;


//
// System Utilities API
//
bool system_esp_io_init();
bool system_peripheral_init(bool init_wifi);
bool system_buffer_init(bool init_vid_buffers);
 
#endif /* SYS_UTILITIES_H */