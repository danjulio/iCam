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
#ifndef POWER_UTILITIES_H
#define POWER_UTILITIES_H

#include <stdbool.h>
#include <stdint.h>


//
// Power Utilities constants
//

// Averaging sample counts
#define BATT_NUM_AVG_SAMPLES  16
#define POWER_AUX_AVG_SAMPLES 8



//
// Battery status data structures
//
enum BATT_STATE_t {
	BATT_100,
	BATT_75,
	BATT_50,
	BATT_25,
	BATT_0,
	BATT_CRIT
};

enum CHARGE_STATE_t {
	CHARGE_OFF,
	CHARGE_ON,
	CHARGE_DONE,
	CHARGE_FAULT
};

typedef struct {
	float batt_voltage;
	float usb_voltage;
	uint16_t load_ma;
	uint16_t usb_ma;
	enum BATT_STATE_t batt_state;
	enum CHARGE_STATE_t charge_state;
} batt_status_t;



//
// Power Utilities API
//
bool power_init();
void power_set_brightness(int percent);
void power_batt_update();
void power_get_batt(batt_status_t* bs);
bool power_button_pressed();
bool power_get_sdcard_present();
void power_off();

#endif /* POWER_UTILITIES_H */