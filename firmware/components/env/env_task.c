/*
 * Environmental monitoring task - Detect and initialize optional AHT20 temp/humidtity
 * and/or VS53L4CX distance sensor and report data to t1c_task for correction.
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
#include "driver_aht20_interface.h"
#include "driver_aht20.h"
#include "env_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "sys_utilities.h"
#include "t1c_task.h"
#include "vl53lx_api.h"
#include "vl53lx_platform.h"
#include "vl53lx_platform_user_data.h"
#include <math.h>



//
// Constants
//

// Uncomment to display some debug info
//#define DEBUG_UPDATES



//
// Variables
//
static const char* TAG = "env_task";

// AHT20 driver handlers
static aht20_handle_t aht20_handle = {
	aht20_interface_iic_init,
	aht20_interface_iic_deinit,
	aht20_interface_iic_read_cmd,
	aht20_interface_iic_write_cmd,
	aht20_interface_delay_ms,
	aht20_interface_debug_print,
	0
};

// VL53L4CX handle
static VL53LX_Dev_t dist_handle;

// VL53L4CX data
static VL53LX_MultiRangingData_t multi_ranging_data;

// State
static bool found_temp_humidity_sensor = false;
static bool found_dist_sensor = false;


//
// Forward declarations for internal functions
//
static void _update_temp_humidity(int16_t* temp, uint8_t* humidity, bool* valid);
static void _update_distance(uint16_t* dist_cm, bool* valid);



//
// API
//
void env_task()
{
	bool temp_humidity_valid = false;
	bool dist_valid = false;
	int temp_eval_count = 1;   // Trigger right away
	int dist_eval_count = 1;
	uint8_t humidity = 0;
	uint8_t dist_data_ready;
	int16_t temp = 0;
	int32_t delay_msec;
	int64_t prev_eval_usec;
	uint16_t dist_ID;
	uint16_t dist_cm = 0;
	VL53LX_Error dist_status;
	
	ESP_LOGI(TAG, "Start task");
	
	// Probe for AHT20 temperature/humidity sensor
	if (aht20_init(&aht20_handle) == 0) {
		found_temp_humidity_sensor = true;
		ESP_LOGI(TAG, "Found AHT20");
	} else {
		ESP_LOGI(TAG, "Could not find AHT20");
	}
	
	// Setup and probe for VL53L4CX sensor
	dist_handle.i2c_slave_address = 0x52;
	if ((dist_status = VL53LX_RdWord(&dist_handle, 0x010F, &dist_ID)) == VL53LX_ERROR_NONE) {
		if (dist_ID == 0xEBAA) {
			found_dist_sensor = true;
			ESP_LOGI(TAG, "Found VL53L4CX");
		} else {
			ESP_LOGI(TAG, "Found VL53L4CX, but DID: %X did not match expected 0xEBAA", dist_ID);
		}
	} else {
		ESP_LOGI(TAG, "Could not find VL53L4CX - %d", (int) dist_status);
	}
	
	// Initialize and start the VL53L4CX sensor
	if (found_dist_sensor) {
		if ((dist_status = VL53LX_WaitDeviceBooted(&dist_handle)) != VL53LX_ERROR_NONE) {
			ESP_LOGE(TAG, "Distance sensor boot wait failed - %d", (int) dist_status);
			found_dist_sensor = false;
		} else if ((dist_status = VL53LX_DataInit(&dist_handle)) != VL53LX_ERROR_NONE) {
			ESP_LOGE(TAG, "Distance sensor data init failed - %d", (int) dist_status);
			found_dist_sensor = false;
		} else if ((dist_status = VL53LX_SetMeasurementTimingBudgetMicroSeconds(&dist_handle, 100000)) != VL53LX_ERROR_NONE) {
			ESP_LOGE(TAG, "Distance sensor set timing budget failed - %d", (int) dist_status);
			found_dist_sensor = false;
		} else if ((dist_status = VL53LX_StartMeasurement(&dist_handle)) != VL53LX_ERROR_NONE) {
			ESP_LOGE(TAG, "Distance sensor start measurements failed - %d", (int) dist_status);
			found_dist_sensor = false;
		}
	}
	
	// This task only runs if at least one sensor is found
	if (!found_temp_humidity_sensor && !found_dist_sensor) {
		ESP_LOGI(TAG, "End task");
		vTaskDelete(NULL);
	}
	
	// Initial evaluation timestamp
	prev_eval_usec = esp_timer_get_time();
	
	while (1) {
		// Temperature/Humidity sensor evaluation
		if (found_temp_humidity_sensor) {
			if (--temp_eval_count == 0) {
				temp_eval_count = ENV_TASK_READ_T_H_MSEC / ENV_TASK_EVAL_MSEC;
				_update_temp_humidity(&temp, &humidity, &temp_humidity_valid);
				t1c_set_ambient_temp(temp, temp_humidity_valid);
				t1c_set_ambient_humidity((uint16_t) humidity, temp_humidity_valid);
				xTaskNotify(task_handle_t1c, T1C_NOTIFY_SET_T_H_MASK, eSetBits);
#ifdef DEBUG_UPDATES
				ESP_LOGI(TAG, "temp = %u, hum = %u %%", temp, humidity);
#endif
			}
		}
		
		// Distance sensor evaluation
		if (found_dist_sensor) {
			if (--dist_eval_count == 0) {
				dist_eval_count = ENV_TASK_READ_DIST_MSEC / ENV_TASK_EVAL_MSEC;
				
				// See if there is data
				dist_status = VL53LX_GetMeasurementDataReady(&dist_handle, &dist_data_ready);
				if (dist_status == VL53LX_ERROR_NONE) {
					if (dist_data_ready != 0) {
						_update_distance(&dist_cm, &dist_valid);
						t1c_set_target_distance(dist_cm, dist_valid);
						xTaskNotify(task_handle_t1c, T1C_NOTIFY_SET_DIST_MASK, eSetBits);
#ifdef DEBUG_UPDATES
						ESP_LOGI(TAG, "dist = %u", dist_cm);
#endif
						
					} else {
						ESP_LOGI(TAG, "dist not ready");
					}
				} else {
					ESP_LOGE(TAG, "VL53LX_GetMeasurementDataReady failed with %d", (int) dist_status);
				}
			}
		}
		
		// Determine how long to sleep to maintain our evaluation interval
		delay_msec = ENV_TASK_EVAL_MSEC - (int32_t)((esp_timer_get_time() - prev_eval_usec) / 1000);
		if (delay_msec < 0) delay_msec = 10;  // Minimum sleep to allow FreeRTOS to schedule
		//ESP_LOGI(TAG, "delta %d", (int32_t)((esp_timer_get_time() - prev_eval_usec) / 1000));
		vTaskDelay(pdMS_TO_TICKS(delay_msec));
		
		// Get a timestamp of when we start an evaluation
		prev_eval_usec = esp_timer_get_time();
	}
}


void env_sensor_present(bool* temp_hum, bool* dist)
{
	*temp_hum = found_temp_humidity_sensor;
	*dist = found_dist_sensor;
}



//
// Internal functions
//
static void _update_temp_humidity(int16_t* temp, uint8_t* humidity, bool* valid)
{
	float t;
	uint8_t h;
	uint32_t humidity_raw;
	uint32_t temp_raw;
	
	if (aht20_read_temperature_humidity(&aht20_handle, &temp_raw, &t, &humidity_raw, &h) == 0) {
		*temp = (int16_t) round(t);
		*humidity = h;
		*valid = true;
	} else{
		*valid = false;
	}
}


static void _update_distance(uint16_t* dist_cm, bool* valid)
{
	int no_of_object_found;
	int max_range_index = -1;
	uint32_t max_signal_mcps = 0;
	VL53LX_Error dist_status;
	
	if ((dist_status = VL53LX_GetMultiRangingData(&dist_handle, &multi_ranging_data)) != VL53LX_ERROR_NONE) {
		ESP_LOGE(TAG, "VL53LX_GetMultiRangingData failed with %d", (int) dist_status);
	} else {
		no_of_object_found = multi_ranging_data.NumberOfObjectsFound;

		for (int i = 0; i < no_of_object_found; i++) {
			if (multi_ranging_data.RangeData[i].RangeStatus == 0) {
				if (multi_ranging_data.RangeData[i].SignalRateRtnMegaCps > max_signal_mcps) {
					max_range_index = i;
					max_signal_mcps = multi_ranging_data.RangeData[i].SignalRateRtnMegaCps;
				}
			}
		}
	}
	
	if ((dist_status = VL53LX_ClearInterruptAndStartMeasurement(&dist_handle)) != VL53LX_ERROR_NONE) {
		ESP_LOGE(TAG, "VL53LX_ClearInterruptAndStartMeasurement failed with %d", (int) dist_status);
	}
	
	if (max_range_index >= 0) {
		*dist_cm = multi_ranging_data.RangeData[max_range_index].RangeMilliMeter / 10;
		*dist_cm += (multi_ranging_data.RangeData[max_range_index].RangeMilliMeter % 10) >= 5 ? 1 : 0;
		*valid = true;
	} else {
		*valid = false;
	}
}
