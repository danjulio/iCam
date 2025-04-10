/*
 * Application entry-point for iCam.  Initializes camera and starts the tasks
 * that implement its functionality.
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

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "env_task.h"
#include "file_task.h"
#include "gcore_task.h"
#include "gui_task.h"
#include "mon_task.h"
#include "t1c_task.h"
#include "system_config.h"
#include "sys_utilities.h"


//
// Variables
//
static const char* TAG = "main";



//
// API
//
void icam_app_main(void)
{
	ESP_LOGI(TAG, "iCamCntrl on iCam starting");
	
	// Initialize system-level ESP32 internal peripherals
    if (!system_esp_io_init()) {
    	ESP_LOGE(TAG, "ESP32 init failed");
    	while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
    }
    
    // Initialize system-level subsystems
    if (!system_peripheral_init(false)) {
    	ESP_LOGE(TAG, "Peripheral init failed");
    	while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
    }
    
    // Pre-allocate shared big buffers
    if (!system_buffer_init(false)) {
    	ESP_LOGE(TAG, "Memory allocate failed");
    	while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
    }
    
    // Start tasks
    //  Core 0 : PRO
    //  Core 1 : APP
    xTaskCreatePinnedToCore(&env_task,   "env_task",   3072, NULL, 1, &task_handle_env,   0);
    xTaskCreatePinnedToCore(&gcore_task, "gcore_task", 3072, NULL, 2, &task_handle_gcore, 0);
	xTaskCreatePinnedToCore(&gui_task,   "gui_task",   3072, NULL, 2, &task_handle_gui,   0);
	xTaskCreatePinnedToCore(&file_task,  "file_task",  8192, NULL, 2, &task_handle_file,  1);
    xTaskCreatePinnedToCore(&t1c_task,   "t1c_task",   4096, NULL, 2, &task_handle_t1c,   1);

#ifdef INCLUDE_SYS_MON
	xTaskCreatePinnedToCore(&mon_task,   "mon_task",   2048, NULL, 1, &task_handle_mon,   0);
#endif
}

#endif /* !CONFIG_BUILD_ICAM_MINI */

