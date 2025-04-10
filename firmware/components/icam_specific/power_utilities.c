/*
 * Power related utilities
 *
 * Contains functions to read battery, charge and power button information in a
 * thread-safe way for gcore_task and others.
 *
 * Copyright 2021-2024 Dan Julio
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
 *
 */
#include "esp_system.h"
#ifndef CONFIG_BUILD_ICAM_MINI

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "gcore.h"
#include "power_utilities.h"
#include "system_config.h"



//
// Power Utilities variables
//

static const char* TAG = "power_utilities";

// State
static batt_status_t batt_status;
static bool power_btn_pressed;
static bool sdcard_present;
static SemaphoreHandle_t status_mutex;

// Averaging arrays
static uint16_t batt_average_array[BATT_NUM_AVG_SAMPLES];
static uint16_t load_average_array[POWER_AUX_AVG_SAMPLES];
static uint16_t vusb_average_array[POWER_AUX_AVG_SAMPLES];
static uint16_t lusb_average_array[POWER_AUX_AVG_SAMPLES];
static int batt_average_index;
static int aux_average_index;



//
// Power Utilities Forward Declarations for internal functions
//
static enum CHARGE_STATE_t gpio_to_charge_state(uint8_t reg);
static enum BATT_STATE_t batt_mv_to_level(uint16_t mv);
static bool validate_status(uint8_t s);



//
// Power Utilities API
//
bool power_init()
{
	uint8_t t8;
	uint16_t t16;
	
	// Create our mutex
	status_mutex = xSemaphoreCreateMutex();
	
	// Verify communications with the gCore EFM8
	if (!gcore_get_reg8(GCORE_REG_ID, &t8)) {
		ESP_LOGE(TAG, "Could not communicate with gCore");
		return false;
	}
	if (t8 != GCORE_FW_ID) {
		ESP_LOGE(TAG, "gCore ID returned %x instead of %x", t8, GCORE_FW_ID);
		return false;
	}
	
	// Disable any previous wake settings (perhaps a different program had been running on gCore)
	if (!gcore_set_reg8(GCORE_REG_WK_CTRL, 0)) {
		ESP_LOGE(TAG, "Could not reconfigure wake control register");
		return false;
	}
	
	// Get initial charge and battery voltage
	if (!gcore_get_reg8(GCORE_REG_GPIO, &t8)) {
		ESP_LOGE(TAG, "Could not read GPIO register");
		return false;
	}
	batt_status.charge_state = gpio_to_charge_state(t8);
	sdcard_present = (t8 & GCORE_GPIO_SD_CARD_MASK) == GCORE_GPIO_SD_CARD_MASK;
	
	if (!gcore_get_reg16(GCORE_REG_VB, &t16)) {
		ESP_LOGE(TAG, "Could not read battery voltage");
		return false;
	}
	for (t8=0; t8<BATT_NUM_AVG_SAMPLES; t8++) {
		batt_average_array[t8] = t16;
	}
	batt_average_index = 0;
	batt_status.batt_voltage = (float) t16 / 1000.0;
	batt_status.batt_state = batt_mv_to_level(t16);
	
	// Get auxiliary power information
	if (!gcore_get_reg16(GCORE_REG_IL, &t16)) {
		ESP_LOGE(TAG, "Could not read system load");
		return false;
	}
	for (t8=0; t8<POWER_AUX_AVG_SAMPLES; t8++) {
		load_average_array[t8] = t16;
	}
	batt_status.load_ma = t16;
	
	if (!gcore_get_reg16(GCORE_REG_VU, &t16)) {
		ESP_LOGE(TAG, "Could not read VUSB");
		return false;
	}
	for (t8=0; t8<POWER_AUX_AVG_SAMPLES; t8++) {
		vusb_average_array[t8] = t16;
	}
	batt_status.usb_voltage = (float) t16 / 1000.0;
	
	if (!gcore_get_reg16(GCORE_REG_IU, &t16)) {
		ESP_LOGE(TAG, "Could not read IUSB");
		return false;
	}
	for (t8=0; t8<POWER_AUX_AVG_SAMPLES; t8++) {
		lusb_average_array[t8] = t16;
	}
	batt_status.usb_ma = t16;
	aux_average_index = 0;
	
	// Read STATUS register to clear power-on button press
	(void) gcore_get_reg8(GCORE_REG_STATUS, &t8);
	power_btn_pressed = false;
	
	return true;
}


void power_set_brightness(int percent)
{
	uint8_t pwm_val;
	
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;
	
	pwm_val = percent * 255 / 255;
	
	(void) gcore_set_reg8(GCORE_REG_BL, pwm_val);
}


void power_batt_update()
{
	bool btn = false;
	bool sdcard = false;
	enum CHARGE_STATE_t cs = CHARGE_OFF;
	int i;
	uint8_t t8;
	uint16_t mv[2] = {0, 0};
	uint16_t ma[2] = {0, 0};
	uint32_t sum = 0;
	
	// Update charge state and sd card present - assume, at this point, gCore accesses are working
	if (gcore_get_reg8(GCORE_REG_GPIO, &t8)) {
		cs = gpio_to_charge_state(t8);
		sdcard = (t8 & GCORE_GPIO_SD_CARD_MASK) == GCORE_GPIO_SD_CARD_MASK;
	}
	
	// Update voltages and currents
	if (gcore_get_reg16(GCORE_REG_VB, &mv[0])) {
		batt_average_array[batt_average_index] = mv[0];
		if (++batt_average_index == BATT_NUM_AVG_SAMPLES) batt_average_index = 0;
		
		// Compute the battery voltage average mV
		for (i=0; i<BATT_NUM_AVG_SAMPLES; i++) {
			sum += batt_average_array[i];
		}
		mv[0] = sum / BATT_NUM_AVG_SAMPLES;
	}
	
	(void) gcore_get_reg16(GCORE_REG_IL, &ma[0]);
	load_average_array[aux_average_index] = ma[0];
	sum = 0;
	for (i=0; i<POWER_AUX_AVG_SAMPLES; i++) {
		sum += load_average_array[i];
	}
	ma[0] = sum / POWER_AUX_AVG_SAMPLES;
	
	(void) gcore_get_reg16(GCORE_REG_VU, &mv[1]);
	vusb_average_array[aux_average_index] = mv[1];
	sum = 0;
	for (i=0; i<POWER_AUX_AVG_SAMPLES; i++) {
		sum += vusb_average_array[i];
	}
	mv[1] = sum / POWER_AUX_AVG_SAMPLES;
	
	(void) gcore_get_reg16(GCORE_REG_IU, &ma[1]);
	lusb_average_array[aux_average_index] = ma[1];
	sum = 0;
	for (i=0; i<POWER_AUX_AVG_SAMPLES; i++) {
		sum += lusb_average_array[i];
	}
	ma[1] = sum / POWER_AUX_AVG_SAMPLES;
	if (++aux_average_index == POWER_AUX_AVG_SAMPLES) aux_average_index = 0;
	
	// Update button press state
	if (gcore_get_reg8(GCORE_REG_STATUS, &t8)) {
		if (validate_status(t8)) {
			btn = (t8 & GCORE_ST_PB_PRESS_MASK);
		} else {
			ESP_LOGE(TAG, "Illegal STATUS = 0x%x", t8);
		}
	}
	
	xSemaphoreTake(status_mutex, portMAX_DELAY);
	batt_status.batt_voltage = (float) mv[0] / 1000.0;
	batt_status.load_ma = ma[0];
	batt_status.usb_voltage = (float) mv[1] / 1000.0;
	batt_status.usb_ma = ma[1];
	batt_status.batt_state = batt_mv_to_level(mv[0]);
	batt_status.charge_state = cs;
	power_btn_pressed = btn;
	sdcard_present = sdcard;
	xSemaphoreGive(status_mutex);
}


void power_get_batt(batt_status_t* bs)
{
	xSemaphoreTake(status_mutex, portMAX_DELAY);
	bs->batt_voltage = batt_status.batt_voltage;
	bs->load_ma = batt_status.load_ma;
	bs->usb_voltage = batt_status.usb_voltage;
	bs->usb_ma = batt_status.usb_ma;
	bs->batt_state = batt_status.batt_state;
	bs->charge_state = batt_status.charge_state;
	xSemaphoreGive(status_mutex);
}


bool power_button_pressed()
{
	bool btn;
	
	xSemaphoreTake(status_mutex, portMAX_DELAY);
	btn = power_btn_pressed;
	xSemaphoreGive(status_mutex);
	
	return btn;
}


bool power_get_sdcard_present()
{
	bool present;
	
	xSemaphoreTake(status_mutex, portMAX_DELAY);
	present = sdcard_present;
	xSemaphoreGive(status_mutex);
	
	return present;
}


void power_off()
{
	(void) gcore_set_reg8(GCORE_REG_SHDOWN, GCORE_SHUTDOWN_TRIG);
}


//
// Power Utilities internal functions
//
static enum CHARGE_STATE_t gpio_to_charge_state(uint8_t reg)
{
	enum CHARGE_STATE_t cs;
	
	switch (reg & GCORE_GPIO_CHG_MASK) {
		case GCORE_CHG_IDLE:
			cs = CHARGE_OFF;
			break;
		
		case GCORE_CHG_ACTIVE:
			cs = CHARGE_ON;
			break;
		
		case GCORE_CHG_DONE:
			cs = CHARGE_DONE;
			break;
		
		default:
			cs = CHARGE_FAULT;
	}
	
	return cs;
}


static enum BATT_STATE_t batt_mv_to_level(uint16_t mv)
{
	enum BATT_STATE_t bs;
	
	// Set the battery state
	if (mv <= BATT_CRIT_THRESHOLD_MV) bs = BATT_CRIT;
	else if (mv <= BATT_0_THRESHOLD_MV) bs = BATT_0;
	else if (mv <= BATT_25_THRESHOLD_MV) bs = BATT_25;
	else if (mv <= BATT_50_THRESHOLD_MV) bs = BATT_50;
	else if (mv <= BATT_75_THRESHOLD_MV) bs = BATT_75;
	else bs = BATT_100;
	
	return bs;
}


// Return false for any illegal status value.  Occasional bad reads were seen
// during extended testing (e.g. 1 in 2 million reads) and since we use the 
// STATUS to determine when to shut off, we want to try to validate the read data.
static bool validate_status(uint8_t s)
{
	int count = 0;
	
	// First look for any reserved bits to be set
	if ((s & (~(GCORE_ST_CRIT_BATT_MASK | GCORE_ST_PB_PRESS_MASK | GCORE_ST_PWR_ON_RSN_MASK))) != 0) {
		return false;
	}
	
	// Then make sure only one of the power-on reason bits is set
	for (int i=0; i<3; i++) {
		if ((s & (GCORE_PWR_ON_BTN_MASK << i)) != 0) {
			count += 1;
		}
	}
	if (count != 1) {
		return false;
	}
	
	return true;
}

#endif /* !CONFIG_BUILD_ICAM_MINI */