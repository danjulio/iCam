/*
 * Control Interface Task - Hardware management task for iCamMini.  Device-specific
 * functionality for monitoring the battery, controls, SD-card presence, video/web
 * jumper and controlling the Status LED.
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
 */
#include "esp_system.h"
#ifdef CONFIG_BUILD_ICAM_MINI

#include <stdbool.h>
#include "ctrl_task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "file_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "wifi_utilities.h"
#include "video_task.h"
#include "web_task.h"



//
// Control Task Constants
//

// Button indicies
#define CTRL_BTN_PWR 0
#define CTRL_BTN_AUX 1
#define CTRL_NUM_BTN 2

// LED Colors
#define CTRL_LED_OFF 0
#define CTRL_LED_RED 1
#define CTRL_LED_YEL 2
#define CTRL_LED_GRN 3

// LED State Machine states
#define CTRL_LED_ST_SOLID      0
#define CTRL_LED_ST_BLINK_ON   1
#define CTRL_LED_ST_BLINK_OFF  2
#define CTRL_LED_ST_RST_ON     3
#define CTRL_LED_ST_RST_OFF    4
#define CTRL_LED_ST_FLT_ON     5
#define CTRL_LED_ST_FLT_OFF    6
#define CTRL_LED_ST_FLT_IDLE   7
#define CTRL_LED_ST_FW_REQ_R   8
#define CTRL_LED_ST_FW_REQ_G   9
#define CTRL_LED_ST_FW_UPD_ON  10
#define CTRL_LED_ST_FW_UPD_OFF 11
#define CTRL_LED_ST_LB_OFF1    12
#define CTRL_LED_ST_LB_ON      13
#define CTRL_LED_ST_LB_OFF2    14
#define CTRL_LED_ST_SAVE_ON    15
#define CTRL_LED_ST_SAVE_OFF   16

// Control State Machine states
#define CTRL_ST_STARTUP            0
#define CTRL_ST_FAULT              1
#define CTRL_ST_VID                2
#define CTRL_ST_NET_NOT_CONNECTED  3
#define CTRL_ST_NET_CONNECTED      4
#define CTRL_ST_CLIENT_CONNECTED   5
#define CTRL_ST_RESET_ALERT        6
#define CTRL_ST_RESET_ACTION       7
#define CTRL_ST_RESTART_ALERT      8
#define CTRL_ST_RESTART_ACTION     9
#define CTRL_ST_FW_UPD_REQUEST     10
#define CTRL_ST_FW_UPD_PROCESS     11
#define CTRL_ST_WAIT_OFF           12

// Battery averaging array size
#define CTRL_BATT_ARRAY_LEN        10

//
// GPIO to ADC1 Channel mapping
//
static const int gpio_2_adc1_ch[40] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1,  4,  5,  6,  7,  0,  1,  2,  3
};



//
// Control Task variables
//
static const char* TAG = "ctrl_task";

// Board/IO related
static bool sd_present = false;
static int ctrl_output_format;

// Batt/ADC related
static int batt_channel;
static adc_cali_handle_t cali_handle;
static adc_oneshot_unit_handle_t adc1_handle;
static int batt_read_timer;
static int batt_low_indication_timer;
static int batt_crit_shutdown_timer;
static uint16_t batt_avg_array[CTRL_BATT_ARRAY_LEN];
static uint16_t ctrl_batt_mv;

// State
static int ctrl_state;
static int ctrl_pre_activity_state;              // ctrl_state to return to from fault
static int ctrl_led_state;
static int ctrl_led_ret_state;                   // ctrl_led_state to return to from low-battery or file save blink(s)
static int ctrl_action_timer;
static int ctrl_led_timer;
static int ctrl_fault_led_count;
static int ctrl_fault_type = CTRL_FAULT_NONE;

// Incoming Notifications
static bool notify_startup_done = false;
static bool save_notification_success = false;

// Mode dependent notifications
static TaskHandle_t output_task;
static uint32_t task_crit_batt_notification;
static uint32_t task_shutdown_notification;



//
// Forward Declarations for internal functions
//
static void ctrl_task_init();
static void ctrl_debounce_pwr_button(bool* short_p, bool* long_p, bool* down);
static void ctrl_debounce_aux_button(bool* short_p, bool* long_p, bool* down);
static void ctrl_debounce_sd_input(bool* present);
static void ctrl_eval_batt();
static uint16_t ctrl_read_batt_mv();
static uint16_t ctrl_get_new_batt_avg_mv(uint16_t cur_reading);
static void ctrl_set_led(int color);
static void ctrl_eval_sd_present();
static void ctrl_eval_sm();
static void ctrl_set_state(int new_st);
static void ctrl_eval_led_sm();
static void ctrl_set_led_state(int new_st);
static void ctrl_handle_notifications();
static void ctrl_setup_output_notifications();



//
// API
//
void ctrl_task()
{
	ESP_LOGI(TAG, "Start task");
	
	ctrl_task_init();
	
	while (1) {
		vTaskDelay(pdMS_TO_TICKS(CTRL_EVAL_MSEC));
		ctrl_handle_notifications();
		ctrl_eval_led_sm();
		ctrl_eval_sm();
		
		// Evaluate notification generating tasks only after the system is up (all
		// other tasks are running)
		if (notify_startup_done) {
			ctrl_eval_batt();
			ctrl_eval_sd_present();
		}
	}
}


int ctrl_get_output_mode()
{
	return ctrl_output_format;
}


// Not protected by semaphore since it won't be accessed until after subsequent notification
void ctrl_set_fault_type(int f)
{	
	if (f == CTRL_FAULT_NONE) {
		// Asynchronously notify this task to return to the previous state
		ctrl_fault_type = f;
		xTaskNotify(task_handle_ctrl, CTRL_NOTIFY_FAULT_CLEAR, eSetBits);
	} else {
		if (ctrl_state != CTRL_ST_FAULT) {
			// Asynchronously notify this task only for the first fault
			ctrl_fault_type = f;
			xTaskNotify(task_handle_ctrl, CTRL_NOTIFY_FAULT, eSetBits);
		}	
	}
}


// Called before sending CTRL_NOTIFY_SAVE_START
void ctrl_set_save_success(bool s)
{
	save_notification_success = s;
}


uint16_t ctrl_get_batt_mv()
{
	return ctrl_batt_mv;
}


int ctrl_get_batt_percent()
{
	uint16_t local_mv = ctrl_batt_mv;
	
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


bool ctrl_get_sdcard_present()
{
	return sd_present;
}



//
// Internal functions
//
static void ctrl_task_init()
{
	esp_err_t ret;
	
	// Enable power immediately
	gpio_reset_pin(BRD_PWR_HOLD_IO);
	gpio_set_pull_mode(BRD_PWR_HOLD_IO, GPIO_PULLDOWN_ONLY);
	gpio_set_direction(BRD_PWR_HOLD_IO, GPIO_MODE_OUTPUT);
	gpio_set_level(BRD_PWR_HOLD_IO, 1);
	
	// Determine the output mode
	gpio_reset_pin(BRD_OUT_MODE_IO);
	gpio_set_direction(BRD_OUT_MODE_IO, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BRD_OUT_MODE_IO, GPIO_PULLUP_ONLY);
	if (gpio_get_level(BRD_OUT_MODE_IO) == 0) {
		ctrl_output_format = CTRL_OUTPUT_WIFI;
	} else {
		ctrl_output_format = CTRL_OUTPUT_VID;
	}
	
	// Setup the GPIO
	gpio_reset_pin(BRD_BTN1_IO);
	gpio_set_direction(BRD_BTN1_IO, GPIO_MODE_INPUT);
	
	gpio_reset_pin(BRD_BTN2_IO);
	gpio_set_direction(BRD_BTN2_IO, GPIO_MODE_INPUT);
	
	gpio_reset_pin(BRD_RED_LED_IO);
	gpio_set_direction(BRD_RED_LED_IO, GPIO_MODE_OUTPUT);
	gpio_set_drive_capability(BRD_RED_LED_IO, GPIO_DRIVE_CAP_3);
	
	gpio_reset_pin(BRD_GREEN_LED_IO);
	gpio_set_direction(BRD_GREEN_LED_IO, GPIO_MODE_OUTPUT);
	gpio_set_drive_capability(BRD_GREEN_LED_IO, GPIO_DRIVE_CAP_3);
	
	gpio_reset_pin(BRD_SD_SNS_IO);
	gpio_set_direction(BRD_SD_SNS_IO, GPIO_MODE_INPUT);
	
	// Setup and calibrate the battery monitor ADC (errors here should never occur - only
	// during development if some wrong value is specified)
	if ((batt_channel = gpio_2_adc1_ch[BRD_BATT_SNS_IO]) == -1) {
		ESP_LOGE(TAG, "Illegal GPIO used for battery sense - %d", BRD_BATT_SNS_IO);
	}
	
	// Initialize ADC1
	adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    if ((ret = adc_oneshot_new_unit(&init_config1, &adc1_handle)) != ESP_OK) {
    	ESP_LOGE(TAG, "adc_oneshot_new_unit returned %d", ret);
    }
    
    // Configure the battery monitor input
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = BATT_ADC_ATTEN,
    };
    if ((ret = adc_oneshot_config_channel(adc1_handle, batt_channel, &config)) != ESP_OK) {
    	ESP_LOGE(TAG, "adc_oneshot_config_channel returned %d", ret);
    }
    
    // Calibrate ADC1
	adc_cali_line_fitting_config_t cali_config = {
		.unit_id = ADC_UNIT_1,
		.atten = BATT_ADC_ATTEN,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
	};
	if ((ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle)) != ESP_OK) {
		ESP_LOGE(TAG, "adc_cali_create_scheme_line_fitting returned %d", ret);
	}
	
	// Initialize the battery voltage
	for (int i=0; i<CTRL_BATT_ARRAY_LEN; i++) {
		ctrl_batt_mv = ctrl_get_new_batt_avg_mv(ctrl_read_batt_mv());
	}
	
	// Setup battery evaluation
	batt_read_timer = CTRL_BATT_READ_MSEC / CTRL_EVAL_MSEC;
	batt_low_indication_timer = 0; // Disabled
	batt_crit_shutdown_timer = 0; // Disabled
	
	// Initialize state
	ctrl_set_state(CTRL_ST_STARTUP);
}


static void ctrl_debounce_pwr_button(bool* short_p, bool* long_p, bool* down)
{
	// Button press state (power button starts off pressed)
	static bool prev_btn = true;
	static bool btn_down = true;
	static bool init = true;
	static int btn_timer;
	
	// Dynamic variables
	bool cur_btn;
	bool btn_released = false;
	
	// Outputs will be set as necessary
	*short_p = false;
	*long_p = false;
	
	// Get current button value (active high)
	cur_btn = gpio_get_level(BRD_BTN1_IO) == 1;
	
	// Evaluate button logic
	if (cur_btn && prev_btn && !btn_down) {
		// Button just pressed
		btn_down = true;
		btn_timer = CTRL_BTN_LONG_PRESS_MSEC / CTRL_EVAL_MSEC;
	}
	if (!cur_btn && !prev_btn && btn_down) {
		btn_down = false;
		if (init) {
			init = false;
		} else {
			btn_released = true;
		}
	}
	prev_btn = cur_btn;
	
	// Evaluate timer for long press detection
	if (btn_down && !init) {
		if (btn_timer != 0) {
			if (--btn_timer == 0) {
				// Long press detected
				*long_p = true;
			}
		}
	}
	
	if (btn_released && (btn_timer != 0)) {
		// Short press detected
		*short_p = true;
	}
	
	*down = btn_down;
}


static void ctrl_debounce_aux_button(bool* short_p, bool* long_p, bool* down)
{
	// Button press state (starts off not-pressed)
	static bool prev_btn = false;
	static bool btn_down = false;
	static int btn_timer;
	
	// Dynamic variables
	bool cur_btn;
	bool btn_released = false;
	
	// Outputs will be set as necessary
	*short_p = false;
	*long_p = false;
	
	// Get current button value (active low)
	cur_btn = gpio_get_level(BRD_BTN2_IO) == 0;
	
	// Evaluate button logic
	if (cur_btn && prev_btn && !btn_down) {
		// Button just pressed
		btn_down = true;
		btn_timer = CTRL_BTN_LONG_PRESS_MSEC / CTRL_EVAL_MSEC;
	}
	if (!cur_btn && !prev_btn && btn_down) {
		btn_down = false;
		btn_released = true;
	}
	prev_btn = cur_btn;
	
	// Evaluate timer for long press detection
	if (btn_down) {
		if (btn_timer != 0) {
			if (--btn_timer == 0) {
				// Long press detected
				*long_p = true;
			}
		}
	}
	
	if (btn_released && (btn_timer != 0)) {
		// Short press detected
		*short_p = true;
	}
	
	*down = btn_down;
}


static void ctrl_debounce_sd_input(bool* present)
{
	static bool prev_sd_sense = false;
	bool cur_sd_sense;
	
	cur_sd_sense = (gpio_get_level(BRD_SD_SNS_IO) == 0);
	*present = cur_sd_sense && prev_sd_sense;
	prev_sd_sense = cur_sd_sense;
}


static uint16_t ctrl_read_batt_mv()
{
	esp_err_t ret;
	int raw_adc;
	int cal_mv;
	
	if ((ret = adc_oneshot_read(adc1_handle, batt_channel, &raw_adc)) != ESP_OK) {
		ESP_LOGE(TAG, "adc_oneshot_read failed - %d", ret);
		raw_adc = 0;
	}
	
	if ((ret = adc_cali_raw_to_voltage(cali_handle, raw_adc, &cal_mv)) != ESP_OK) {
		ESP_LOGE(TAG, "adc_cali_raw_to_voltage failed - %d", ret);
		cal_mv = 0;
	}
	
	return (uint16_t) cal_mv * BATT_SNS_SCALE_FACTOR;
}


static uint16_t ctrl_get_new_batt_avg_mv(uint16_t cur_reading)
{
	static int index = 0;
	uint32_t sum = 0;
	
	batt_avg_array[index] = cur_reading;
	if (++index >= CTRL_BATT_ARRAY_LEN) index = 0;
	
	for (int i=0; i<CTRL_BATT_ARRAY_LEN; i++) {
		sum += batt_avg_array[i];
	}
	
	// Round up if necessary
	if ((sum % CTRL_BATT_ARRAY_LEN) >= (CTRL_BATT_ARRAY_LEN / 2)) {
		sum += (CTRL_BATT_ARRAY_LEN / 2);
	}
	
	return (uint16_t) (sum / CTRL_BATT_ARRAY_LEN);
}


static void ctrl_set_led(int color)
{
	switch (color) {
		case CTRL_LED_OFF:
			gpio_set_level(BRD_RED_LED_IO, 0);
			gpio_set_level(BRD_GREEN_LED_IO, 0);
			break;
		
		case CTRL_LED_RED:
			gpio_set_level(BRD_RED_LED_IO, 1);
			gpio_set_level(BRD_GREEN_LED_IO, 0);
			break;
		
		case CTRL_LED_YEL:
			gpio_set_level(BRD_RED_LED_IO, 1);
			gpio_set_level(BRD_GREEN_LED_IO, 1);
			break;
		
		case CTRL_LED_GRN:
			gpio_set_level(BRD_RED_LED_IO, 0);
			gpio_set_level(BRD_GREEN_LED_IO, 1);
			break;
	}
}


static void ctrl_eval_batt()
{
	// Periodically update battery voltage
	if (--batt_read_timer <= 0) {
		// Reset timer for next time
		batt_read_timer = CTRL_BATT_READ_MSEC / CTRL_EVAL_MSEC;
		
		// Update battery voltage
		ctrl_batt_mv = ctrl_get_new_batt_avg_mv(ctrl_read_batt_mv());
		
		// Look for critical battery condition that can turn us off or for low battery condition
		if (batt_crit_shutdown_timer > 0) {
			if (--batt_crit_shutdown_timer == 0) {
				// Timer expired -> shutdown now
				ESP_LOGI(TAG, "Critical Battery Power off");
				ctrl_set_state(CTRL_ST_WAIT_OFF);
			}
		} else if (ctrl_batt_mv < BATT_CRIT_THRESHOLD_MV) {
			ESP_LOGI(TAG, "Critical Battery detected - %u mV", ctrl_batt_mv);
			
			// Notify output task we will be shutting down shortly
			xTaskNotify(output_task, task_crit_batt_notification, eSetBits);
			
			// Setup shutdown timer
			batt_crit_shutdown_timer = CTRL_CRIT_BATT_OFF_MSEC / CTRL_BATT_READ_MSEC;
		} else if (ctrl_batt_mv < BATT_25_THRESHOLD_MV) {
			if (batt_low_indication_timer != 0) {
				if (--batt_low_indication_timer == 0) {
					// Start a low battery blink only if we're not doing a higher priority indication
					if (ctrl_state < CTRL_ST_RESET_ALERT) {
						ctrl_set_led_state(CTRL_LED_ST_LB_OFF1);
					}
				}
			} else {
				batt_low_indication_timer = CTRL_LOW_BATT_IND_MSEC / CTRL_BATT_READ_MSEC;
			}
		}
	}
}


static void ctrl_eval_sd_present()
{
	static bool prev_sd_present = false;
	
	ctrl_debounce_sd_input(&sd_present);
	
	// Notify file_task of changes
	if (sd_present && !prev_sd_present) {
		xTaskNotify(task_handle_file, FILE_NOTIFY_CARD_PRESENT_MASK, eSetBits);
	} else if (!sd_present && prev_sd_present) {
		xTaskNotify(task_handle_file, FILE_NOTIFY_CARD_REMOVED_MASK, eSetBits);
	}
	
	prev_sd_present = sd_present;
}


static void ctrl_eval_sm()
{
	bool btn_short_press[CTRL_NUM_BTN];
	bool btn_long_press[CTRL_NUM_BTN];
	bool btn_down[CTRL_NUM_BTN];
	
	// Look for button presses
	ctrl_debounce_pwr_button(&btn_short_press[CTRL_BTN_PWR], &btn_long_press[CTRL_BTN_PWR], &btn_down[CTRL_BTN_PWR]);
	ctrl_debounce_aux_button(&btn_short_press[CTRL_BTN_AUX], &btn_long_press[CTRL_BTN_AUX], &btn_down[CTRL_BTN_AUX]);
	
	// Global power off handling
	if (btn_long_press[CTRL_BTN_PWR]) {
		ESP_LOGI(TAG, "Power Button Power off");
		ctrl_set_state(CTRL_ST_WAIT_OFF);
	}
	
	// PWR button short press is always "take picture" with web output
	if (ctrl_output_format == CTRL_OUTPUT_WIFI) {
		if (btn_short_press[CTRL_BTN_PWR]) {
			xTaskNotify(task_handle_web, WEB_NOTIFY_TAKE_PICTURE_MASK, eSetBits);
		}
	}
	
	switch (ctrl_state) {
		case CTRL_ST_STARTUP:
			// Wait to be taken out of this state by a notification
			if (notify_startup_done) {
				if (ctrl_output_format == CTRL_OUTPUT_VID) {
					ctrl_set_state(CTRL_ST_VID);
				} else {
					ctrl_set_state(CTRL_ST_NET_NOT_CONNECTED);
				}
			}
			break;
		
		case CTRL_ST_FAULT:
			// Remain here until taken out if the fault is cleared or the user resets the network
			if (ctrl_output_format == CTRL_OUTPUT_WIFI) {
				if (btn_long_press[CTRL_BTN_AUX]) {
					ctrl_set_state(CTRL_ST_RESET_ALERT);
				}
			}
			break;
		
		case CTRL_ST_VID:
			// Notify video task of button presses, otherwise remain here unless taken out by fault notification
			if (btn_short_press[CTRL_BTN_PWR]) {
				xTaskNotify(task_handle_vid, VID_NOTIFY_B1_SHORT_PRESS_MASK, eSetBits);
			}
			if (btn_short_press[CTRL_BTN_AUX]) {
				xTaskNotify(task_handle_vid, VID_NOTIFY_B2_SHORT_PRESS_MASK, eSetBits);
			}
			if (btn_long_press[CTRL_BTN_AUX]) {
				xTaskNotify(task_handle_vid, VID_NOTIFY_B2_LONG_PRESS_MASK, eSetBits);
			}
			break;

		case CTRL_ST_NET_NOT_CONNECTED:
			if (btn_long_press[CTRL_BTN_AUX]) {
				ctrl_set_state(CTRL_ST_RESET_ALERT);
			} else if (wifi_is_connected()) {
				ctrl_set_state(CTRL_ST_NET_CONNECTED);
			}
			break;
		
		case CTRL_ST_NET_CONNECTED:
			if (btn_long_press[CTRL_BTN_AUX]) {
				ctrl_set_state(CTRL_ST_RESET_ALERT);
			} else if (!wifi_is_connected()) {
				ctrl_set_state(CTRL_ST_NET_NOT_CONNECTED);
			} else if (web_has_client()) {
				ctrl_set_state(CTRL_ST_CLIENT_CONNECTED);
			}
			break;
		
		case CTRL_ST_CLIENT_CONNECTED:
			if (btn_long_press[CTRL_BTN_AUX]) {
				xTaskNotify(task_handle_web, WEB_NOTIFY_NETWORK_DISC_MASK, eSetBits);
				ctrl_set_state(CTRL_ST_RESET_ALERT);
			} else if (!web_has_client()) {
				// Goto network not connected in case this was why we lost our client.
				// If it is connected then we'll quickly go to network connected.
				ctrl_set_state(CTRL_ST_NET_NOT_CONNECTED);
			}
			break;
		
		case CTRL_ST_RESET_ALERT:
			if (--ctrl_action_timer == 0) {
				// Reset alert done - initiate actual network reset
				ctrl_set_state(CTRL_ST_RESET_ACTION);
			}
			break;
		
		case CTRL_ST_RESET_ACTION:
			if (wifi_is_enabled()) {
				if (ctrl_fault_type == CTRL_FAULT_NONE) {
					ctrl_set_state(CTRL_ST_NET_NOT_CONNECTED);
				} else {
					// Return to previous fault indication to encourage user to power-cycle since
					// we may be halted in main due to the original error
					ctrl_set_state(CTRL_ST_FAULT);
				}
			}
			break;
		
		case CTRL_ST_RESTART_ALERT:
			if (--ctrl_action_timer == 0) {
				// Restart alert done - initiate actual network restart
				ctrl_set_state(CTRL_ST_RESTART_ACTION);
			}
			break;
			
		case CTRL_ST_RESTART_ACTION:
			if (wifi_is_enabled()) {
				if (ctrl_fault_type == CTRL_FAULT_NONE) {
					ctrl_set_state(CTRL_ST_NET_NOT_CONNECTED);
				} else {
					// Return to previous fault indication to encourage user to power-cycle since
					// we may be halted in main due to the original error
					ctrl_set_state(CTRL_ST_FAULT);
				}
			}
			break;
			
		case CTRL_ST_FW_UPD_REQUEST:
			if (btn_short_press[CTRL_BTN_PWR]) {
				// Notify tasks that user has authorized fw update
				xTaskNotify(task_handle_file, FILE_NOTIFY_FW_UPD_EN_MASK, eSetBits);
				xTaskNotify(task_handle_web, WEB_NOTIFY_FW_UPD_EN_MASK, eSetBits);
			}
			break;
		
		case CTRL_ST_FW_UPD_PROCESS:
			if (btn_short_press[CTRL_BTN_PWR]) {
				// Notify tasks that user has terminated fw update
				xTaskNotify(task_handle_file, FILE_NOTIFY_FW_UPD_END_MASK, eSetBits);
				xTaskNotify(task_handle_web, WEB_NOTIFY_FW_UPD_END_MASK, eSetBits);
			}
			break;
		
		case CTRL_ST_WAIT_OFF:
			if (!btn_down[CTRL_BTN_PWR]) {
				gpio_set_level(BRD_PWR_HOLD_IO, 0);
			}
			break;
		
		default:
			ctrl_set_state(CTRL_ST_STARTUP);
	}
}


static void ctrl_eval_led_sm()
{
	switch (ctrl_led_state) {
		case CTRL_LED_ST_SOLID:
			// Wait to be taken out of this state
			break;
		
		case CTRL_LED_ST_BLINK_ON:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_BLINK_OFF);
			}
			break;
		
		case CTRL_LED_ST_BLINK_OFF:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_BLINK_ON);
			}
			break;
			
		case CTRL_LED_ST_RST_ON:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_RST_OFF);
			}
			break;
		
		case CTRL_LED_ST_RST_OFF:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_RST_ON);
			}
			break;
		
		case CTRL_LED_ST_FLT_ON:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_FLT_OFF);
			}
			break;
		
		case CTRL_LED_ST_FLT_OFF:
			if (--ctrl_led_timer == 0) {
				if (--ctrl_fault_led_count == 0) {
					ctrl_set_led_state(CTRL_LED_ST_FLT_IDLE);
				} else {
					ctrl_set_led_state(CTRL_LED_ST_FLT_ON);
				}
			}
			break;
		
		case CTRL_LED_ST_FLT_IDLE:
			if (--ctrl_led_timer == 0) {
				ctrl_fault_led_count = ctrl_fault_type;
				ctrl_set_led_state(CTRL_LED_ST_FLT_ON);
			}
			break;

		case CTRL_LED_ST_FW_REQ_R:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_FW_REQ_G);
			}
			break;
		
		case CTRL_LED_ST_FW_REQ_G:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_FW_REQ_R);
			}
			break;
		
		case CTRL_LED_ST_FW_UPD_ON:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_FW_UPD_OFF);
			}
			break;
		
		case CTRL_LED_ST_FW_UPD_OFF:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_FW_UPD_ON);
			}
			break;
		
		case CTRL_LED_ST_LB_OFF1:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_LB_ON);
			}
			break;
			
		case CTRL_LED_ST_LB_ON:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_LB_OFF2);
			}
			break;
			
		case CTRL_LED_ST_LB_OFF2:
			if (--ctrl_led_timer == 0) {
				// Return to previous state
				ctrl_set_state(ctrl_state);
			}
			break;
					
		case CTRL_LED_ST_SAVE_ON:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_SAVE_OFF);
			}
			break;
		
		case CTRL_LED_ST_SAVE_OFF:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_SAVE_ON);
			}
			break;
	}
}


static void ctrl_set_state(int new_st)
{	
	switch (new_st) {
		case CTRL_ST_STARTUP:
			ctrl_set_led(CTRL_LED_RED);
			ctrl_set_led_state(CTRL_LED_ST_SOLID);
			break;
			
		case CTRL_ST_FAULT:
			if (ctrl_fault_type != CTRL_FAULT_NONE) {
				// Setup to blink fault code
				ctrl_fault_led_count = ctrl_fault_type;
				ctrl_set_led_state(CTRL_LED_ST_FLT_ON);
				
				// Save the existing state to return to when the fault is cleared
				ctrl_pre_activity_state = ctrl_state;
			}
			break;
				
		case CTRL_ST_VID:
			ctrl_set_led(CTRL_LED_GRN);
			ctrl_set_led_state(CTRL_LED_ST_SOLID);
			break;
	
		case CTRL_ST_NET_NOT_CONNECTED:
			// Start a slow blink
			ctrl_set_led_state(CTRL_LED_ST_BLINK_ON);
			break;
			
		case CTRL_ST_NET_CONNECTED:
			ctrl_set_led(CTRL_LED_YEL);
			ctrl_set_led_state(CTRL_LED_ST_SOLID);
			break;
		
		case CTRL_ST_CLIENT_CONNECTED:
			ctrl_set_led(CTRL_LED_GRN);
			ctrl_set_led_state(CTRL_LED_ST_SOLID);
			break;
		
		case CTRL_ST_RESET_ALERT:
			// Indicate network restart triggered
			ctrl_set_led_state(CTRL_LED_ST_RST_ON);
			ctrl_action_timer = CTRL_RESET_ALERT_MSEC / CTRL_EVAL_MSEC;
			break;
		
		case CTRL_ST_RESET_ACTION:
			// Re-initialize the network info in persistent storage
			if (ps_reinit_config(PS_CONFIG_TYPE_NET)) {
				// Attempt to re-initialize the network stack
				if (!wifi_reinit()) {
					// Change to fault state
					ctrl_set_led_state(CTRL_LED_ST_FLT_ON);
					ctrl_state = CTRL_ST_FAULT;
				}
			} else {
				// Change to fault state
				ctrl_set_led_state(CTRL_LED_ST_FLT_ON);
				ctrl_state = CTRL_ST_FAULT;
			}
			break;
		
		case CTRL_ST_RESTART_ALERT:
			// Indicate network restart triggered
			ctrl_set_led_state(CTRL_LED_ST_RST_ON);
			ctrl_action_timer = CTRL_RESET_ALERT_MSEC / CTRL_EVAL_MSEC;
			break;
		
		case CTRL_ST_RESTART_ACTION:
			// Attempt to restart the network stack with the new PS parameters
			if (!wifi_reinit()) {
				// Change to fault state
				ctrl_set_led_state(CTRL_LED_ST_FLT_ON);
				ctrl_state = CTRL_ST_FAULT;
			}
			break;
		
		case CTRL_ST_FW_UPD_REQUEST:
			// Start alternating red/green blink to indicate fw update request
			ctrl_set_led_state(CTRL_LED_ST_FW_REQ_R);
			break;
		
		case CTRL_ST_FW_UPD_PROCESS:
			// Start fast green blink to indicate fw update is in progress
			ctrl_set_led_state(CTRL_LED_ST_FW_UPD_ON);
			break;

		case CTRL_ST_WAIT_OFF:
			// Switch off LED and notify the output task as a hint to user to release button
			ctrl_set_led(CTRL_LED_OFF);
			ctrl_set_led_state(CTRL_LED_ST_SOLID);
			xTaskNotify(output_task, task_shutdown_notification, eSetBits);
			break;
	}
	
	ctrl_state = new_st;
}


static void ctrl_set_led_state(int new_st)
{	
	switch (new_st) {
		case CTRL_LED_ST_SOLID:
			// LED Color set outside this call
			break;
		
		case CTRL_LED_ST_BLINK_ON:
			ctrl_led_timer = CTRL_SLOW_BLINK_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_YEL);
			break;
		
		case CTRL_LED_ST_BLINK_OFF:
			ctrl_led_timer = CTRL_SLOW_BLINK_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_OFF);
			break;
		
		case CTRL_LED_ST_RST_ON:
			ctrl_led_timer = CTRL_FAST_BLINK_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_YEL);
			break;
		
		case CTRL_LED_ST_RST_OFF:
			ctrl_led_timer = CTRL_FAST_BLINK_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_OFF);
			break;
		
		case CTRL_LED_ST_FLT_ON:
			ctrl_led_timer = CTRL_FAULT_BLINK_ON_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_RED);
			break;
		
		case CTRL_LED_ST_FLT_OFF:
			ctrl_led_timer = CTRL_FAULT_BLINK_OFF_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_OFF);
			break;
			
		case CTRL_LED_ST_FLT_IDLE:
			ctrl_led_timer = CTRL_FAULT_IDLE_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_OFF);
			break;

		case CTRL_LED_ST_FW_REQ_R:
			ctrl_led_timer = CTRL_FW_UPD_REQ_BLINK_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_RED);
			break;
			
		case CTRL_LED_ST_FW_REQ_G:
			ctrl_led_timer = CTRL_FW_UPD_REQ_BLINK_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_GRN);
			break;

		case CTRL_LED_ST_FW_UPD_ON:
			ctrl_led_timer = CTRL_FAST_BLINK_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_GRN);
			break;

		case CTRL_LED_ST_FW_UPD_OFF:
			ctrl_led_timer = CTRL_FAST_BLINK_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_OFF);
			break;
		
		case CTRL_LED_ST_LB_OFF1:
			ctrl_led_ret_state = ctrl_led_state;
			ctrl_led_timer = CTRL_FW_LB_BLINK_OFF_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_OFF);
			break;
			
		case CTRL_LED_ST_LB_ON:
			ctrl_led_timer = CTRL_FW_LB_BLINK_ON_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_RED);
			break;
			
		case CTRL_LED_ST_LB_OFF2:
			ctrl_led_timer = CTRL_FW_LB_BLINK_OFF_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_OFF);
			break;
					
		case CTRL_LED_ST_SAVE_ON:
			ctrl_led_timer = CTRL_FAST_BLINK_MSEC / CTRL_EVAL_MSEC;
			if (save_notification_success) {
				ctrl_set_led(CTRL_LED_YEL);
			} else {
				ctrl_set_led(CTRL_LED_RED);
			}
			break;
		
		case CTRL_LED_ST_SAVE_OFF:
			ctrl_led_timer = CTRL_FAST_BLINK_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_OFF);
			break;
	}
	
	ctrl_led_state = new_st;
}


static void ctrl_handle_notifications()
{
	uint32_t notification_value;
	
	notification_value = 0;
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, CTRL_NOTIFY_STARTUP_DONE)) {
			// Note startup is done
			notify_startup_done = true;
			
			// Setup for our button notifications (output tasks should be running by now)
			ctrl_setup_output_notifications();
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_FAULT)) {
			ctrl_set_state(CTRL_ST_FAULT);
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_FAULT_CLEAR)) {
			// Return to the pre-fault state
			ctrl_set_state(ctrl_pre_activity_state);
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_SHUTDOWN)) {
			ESP_LOGI(TAG, "Remote Power off");
			vTaskDelay(pdMS_TO_TICKS(10));
			ctrl_set_state(CTRL_ST_WAIT_OFF);
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_FW_UPD_REQ)) {
			// Save the existing state to return to when the update is cleared
			if (ctrl_state != CTRL_ST_FAULT) {
				ctrl_pre_activity_state = ctrl_state;
			}
			ctrl_set_state(CTRL_ST_FW_UPD_REQUEST);
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_FW_UPD_PROCESS)) {
			ctrl_set_state(CTRL_ST_FW_UPD_PROCESS);
		}

		if (Notification(notification_value, CTRL_NOTIFY_FW_UPD_DONE)) {
			// Return to the pre-fault state
			ctrl_set_state(ctrl_pre_activity_state);
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_FW_UPD_REBOOT)) {
			// Delay a bit to allow any final communication to occur and then reboot
			vTaskDelay(pdMS_TO_TICKS(500));
			esp_restart();
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_SAVE_START)) {
			// Start the save notification only if we're not doing a higher priority indication
			if (ctrl_state < CTRL_ST_RESET_ALERT) {
				ctrl_set_led_state(CTRL_LED_ST_SAVE_ON);
			}			
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_SAVE_END)) {
			// Restore the previous LED state by resetting the main ctrl state
			if ((ctrl_led_state == CTRL_LED_ST_SAVE_ON) || (ctrl_led_state == CTRL_LED_ST_SAVE_OFF)) {
				ctrl_set_state(ctrl_state);
			}
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_RESTART_NETWORK)) {
			// This should only be triggered with a client connected.  Start the restart indication.
			//
			// Save the existing state to return to when the update is cleared
			if (ctrl_state != CTRL_ST_FAULT) {
				ctrl_pre_activity_state = ctrl_state;
			}
			ctrl_set_state(CTRL_ST_RESTART_ALERT);
			xTaskNotify(task_handle_web, WEB_NOTIFY_NETWORK_DISC_MASK, eSetBits);
		}
	}
}


// Must be called after output tasks are running
static void ctrl_setup_output_notifications()
{
	if (ctrl_output_format == CTRL_OUTPUT_VID) {
		output_task = task_handle_vid;
		task_crit_batt_notification = VID_NOTIFY_CRIT_BATT_DET_MASK;
		task_shutdown_notification = VID_NOTIFY_SHUTDOWN_MASK;
	} else {
		output_task = task_handle_web;
		task_crit_batt_notification = WEB_NOTIFY_CRIT_BATT_DET_MASK;
		task_shutdown_notification = WEB_NOTIFY_SHUTDOWN_MASK;
	}
}

#endif /* CONFIG_BUILD_ICAM_MINI */
