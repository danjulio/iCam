/*
 * Application entry-point for iCamMini.  Initializes camera and starts the tasks
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
#ifdef CONFIG_BUILD_ICAM_MINI

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ctrl_task.h"
#include "env_task.h"
#include "file_task.h"
#include "mon_task.h"
#include "t1c_task.h"
#include "video_task.h"
#include "web_task.h"
#include "system_config.h"
#include "sys_utilities.h"


//
// Variables
//
static const char* TAG = "main";



//
// API
//
void icam_mini_app_main(void)
{
	int output_type;
	
	ESP_LOGI(TAG, "iCamCntrl on iCamMini starting");
    
    // Start the control task to light the red light immediately
    // and to determine what type of video we will be generating
    xTaskCreatePinnedToCore(&ctrl_task, "ctrl_task", 2176, NULL, 1, &task_handle_ctrl, 0);
    
    // Allow task to start and determine operating mode
    vTaskDelay(pdMS_TO_TICKS(50));
    output_type = ctrl_get_output_mode();
    
    // Initialize system-level ESP32 internal peripherals
    if (!system_esp_io_init()) {
    	ESP_LOGE(TAG, "ESP32 init failed");
    	ctrl_set_fault_type(CTRL_FAULT_ESP32_INIT);
    	while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
    }
    
    // Initialize system-level subsystems
    if (!system_peripheral_init(output_type == CTRL_OUTPUT_WIFI)) {
    	ESP_LOGE(TAG, "Peripheral init failed");
    	ctrl_set_fault_type(CTRL_FAULT_PERIPH_INIT);
    	while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
    }
    
    // Pre-allocate shared big buffers
    if (!system_buffer_init(output_type == CTRL_OUTPUT_VID)) {
    	ESP_LOGE(TAG, "Memory allocate failed");
    	ctrl_set_fault_type(CTRL_FAULT_MEM_INIT);
    	while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
    }
    
    // Start tasks
    //  Core 0 : PRO
    //  Core 1 : APP
    if (output_type == CTRL_OUTPUT_VID) {
    	xTaskCreatePinnedToCore(&vid_task, "vid_task",  4096, NULL, 3, &task_handle_vid,  0);
    } else {
    	xTaskCreatePinnedToCore(&web_task, "web_task",  4096, NULL, 2, &task_handle_web,  0);
    }
    xTaskCreatePinnedToCore(&env_task,     "env_task",  3072, NULL, 1, &task_handle_env,  0);
    xTaskCreatePinnedToCore(&file_task,    "file_task", 8192, NULL, 2, &task_handle_file, 1);
    xTaskCreatePinnedToCore(&t1c_task,     "t1c_task",  4096, NULL, 2, &task_handle_t1c,  1);

#ifdef INCLUDE_SYS_MON
	xTaskCreatePinnedToCore(&mon_task,     "mon_task",  2048, NULL, 1, &task_handle_mon,  0);
#endif
	    
    // Notify control task that we've successfully started up
    xTaskNotify(task_handle_ctrl, CTRL_NOTIFY_STARTUP_DONE, eSetBits);
}

#endif /* CONFIG_BUILD_ICAM_MINI */
