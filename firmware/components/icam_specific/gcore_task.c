/**
 * gcore_task.c
 *  - Battery voltage and charge state monitoring
 *  - Critical battery voltage monitoring and auto shut down (with re-power on charge start)
 *  - Power button monitoring for manual power off
 *  - Backlight control
 *
 * Copyright 2023-2024 Dan Julio
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "esp_system.h"
#ifndef CONFIG_BUILD_ICAM_MINI

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "file_task.h"
#include "gcore_task.h"
#include "gui_task.h"
#include "gcore.h"
#include "out_state_utilities.h"
#include "sys_utilities.h"
#include "system_config.h"

//
// Constants
//

// Battery monitoring count
#define GCORE_BATT_MON_STEPS (GCORE_BATT_MON_MSEC / GCORE_EVAL_MSEC)

// Power state update count
#define GCORE_UPD_STEPS (GCORE_PWR_UPDATE_MSEC / GCORE_EVAL_MSEC)



//
// Variables
//

static const char* TAG = "gcore_task";

// Power state
static int batt_mon_count = 0;
static int gui_update_count = 0;
static enum BATT_STATE_t upd_batt_state = BATT_0;
static enum CHARGE_STATE_t upd_charge_state = CHARGE_OFF;
static uint16_t batt_voltage_mv;
static uint16_t batt_current_ma;
static SemaphoreHandle_t power_state_mutex;

// Backlight state
static uint8_t backlight_percent;
static uint8_t cur_bl_val = 0;

// Critical battery shutdown timer
static int batt_crit_shutdown_timer;

// SD Card state
static bool prev_sd_present = false;

// Notification flags - set by a notification and consumed/cleared by state evaluation
static bool notify_poweroff = false;



//
// Forward declarations for internal functions
//
static void _gcoreHandleNotifications();



//
// API Routines
//
void gcore_task()
{
	batt_status_t cur_batt_status;
	bool sd_present;
	
	ESP_LOGI(TAG, "Start task");
	
	// Power state semaphore
	power_state_mutex = xSemaphoreCreateMutex();
	
	if (!power_init()) {
		ESP_LOGE(TAG, "Power monitoring init failed");
		
		// We need to process notifications to detect attempt to power off
		while (true) {
			_gcoreHandleNotifications();
			
			if (notify_poweroff) {
				// Try to power off
				power_off();
			}
			
			vTaskDelay(pdMS_TO_TICKS(GCORE_EVAL_MSEC));
		}
	}
	
	// Set the button detection threshold
	(void) gcore_set_reg8(GCORE_REG_PWR_TM, GCORE_BTN_THRESH_MSEC / 10);
	
	// Get initial screen brightness information
	backlight_percent = out_state.lcd_brightness;
	
	while (true) {
		// Get any new notifications
		_gcoreHandleNotifications();
		
		// Backlight intensity update
		if (cur_bl_val != backlight_percent) {
			power_set_brightness(backlight_percent);
			cur_bl_val = backlight_percent;
		}
		
		// Look for time to get info from gCore
		if (++batt_mon_count >= GCORE_BATT_MON_STEPS) {
			batt_mon_count = 0;
			
			// Update battery values
			power_batt_update();
			power_get_batt(&cur_batt_status);
			batt_voltage_mv = (uint16_t) (cur_batt_status.batt_voltage * 1000.0);
			batt_current_ma = cur_batt_status.load_ma;
				
			// Look for power-off button press
			if (power_button_pressed() || notify_poweroff) {
				if (notify_poweroff) {
					ESP_LOGI(TAG, "Power off requested");
				} else {
					ESP_LOGI(TAG, "Power button press detected");
				}
				
#if (CONFIG_SCREENDUMP_ENABLE == true)
				// When compiled for screendump we trigger a screendump when the
				// button is pressed (or someone requests a shutdown) instead of
				// turning off.  The device can be shutdown with a long press or
				// by reloading code w/o screen dump once the desired images are taken.
				xTaskNotify(task_handle_gui, GUI_NOTIFY_SCREENDUMP_MASK, eSetBits);
#else
				// Delay for message and power down
				vTaskDelay(pdMS_TO_TICKS(100));
				power_off();
#endif
			}
			
			// Look for critical battery condition that can turn us off
			if (batt_crit_shutdown_timer > 0) {
				if (--batt_crit_shutdown_timer == 0) {
					// Timer expired -> shutdown now
					ESP_LOGI(TAG, "Critical Battery Power off");
					vTaskDelay(pdMS_TO_TICKS(20));
					power_off();
				}
			} else if (cur_batt_status.batt_state == BATT_CRIT) {
				ESP_LOGI(TAG, "Critical battery voltage detected");
				
				// Notify output task we will be shutting down shortly
				xTaskNotify(task_handle_gui, GUI_NOTIFY_CRIT_BATT_DET_MASK, eSetBits);
				
				// Setup shutdown timer
				batt_crit_shutdown_timer = GCORE_CRIT_BATT_OFF_MSEC / GCORE_EVAL_MSEC;
			}
			
			// Look for SD card present change
			sd_present = power_get_sdcard_present();
			if (sd_present && !prev_sd_present) {
				xTaskNotify(task_handle_file, FILE_NOTIFY_CARD_PRESENT_MASK, eSetBits);
			} else if (!sd_present && prev_sd_present) {
				xTaskNotify(task_handle_file, FILE_NOTIFY_CARD_REMOVED_MASK, eSetBits);
			}
			prev_sd_present = sd_present;
		}
		
		// Look for timeout to update externally visible battery state
		if (++gui_update_count >= GCORE_UPD_STEPS) {
			gui_update_count = 0;
					
			xSemaphoreTake(power_state_mutex, portMAX_DELAY);
			upd_batt_state = cur_batt_status.batt_state;
			upd_charge_state = cur_batt_status.charge_state;
			xSemaphoreGive(power_state_mutex);
		}
		
		vTaskDelay(pdMS_TO_TICKS(GCORE_EVAL_MSEC));
	}
}


void gcore_get_power_state(enum BATT_STATE_t* bs, enum CHARGE_STATE_t* cs)
{
	xSemaphoreTake(power_state_mutex, portMAX_DELAY);
	*bs = upd_batt_state;
	*cs = upd_charge_state;
	xSemaphoreGive(power_state_mutex);
}


uint16_t gcore_get_batt_mv()
{
	return batt_voltage_mv;
}


uint16_t gcore_get_load_ma()
{
	return batt_current_ma;
}


int gcore_get_batt_percent()
{
	uint16_t local_mv = batt_voltage_mv;
	
	if (local_mv < BATT_0_THRESHOLD_MV) {
		return 0;
	} else if (local_mv < BATT_25_THRESHOLD_MV) {
		return 25;
	} else if (local_mv < BATT_50_THRESHOLD_MV) {
		return 50;
	} else if (local_mv < BATT_75_THRESHOLD_MV) {
		return 75;
	} else {
		return 100;
	}
}



//
// Internal functions
//
static void _gcoreHandleNotifications()
{
	uint32_t notification_value = 0;
	
	// Handle notifications (clear them upon reading)
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, GCORE_NOTIFY_SHUTOFF_MASK)) {
			notify_poweroff = true;
		}
		
		if (Notification(notification_value, GCORE_NOTIFY_BRGHT_UPD_MASK)) {
			backlight_percent = out_state.lcd_brightness;
		}
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */

