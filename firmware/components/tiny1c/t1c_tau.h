/*
 * Utility functions for computing the TAU value based on gain-specific correction table
 * information and environmental conditions.
 *
 * Copyright 2024 Dan Julio
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
#ifndef _T1C_TEMP_H_
#define _T1C_TEMP_H_

#include <stdbool.h>
#include <stdint.h>


//
// Constants
//
// Correct table dimensions
#define TAU_TABLE_NUM_HUM   4
#define TAU_TABLE_NUM_TEMP  14
#define TAU_TABLE_NUM_DIST  64

#define TAU_TABLE_SIZE (TAU_TABLE_NUM_HUM*TAU_TABLE_NUM_TEMP*TAU_TABLE_NUM_DIST)

// Gain selectors
#define HIGH_GAIN 1
#define LOW_GAIN  0


//
// API
//
// Functions return 0 for success, -1 for failure

// read the environmental correction table from SPIFFS
int read_correct_table(int gain);

// Estimate tau using the correction table
//  ta : Ambient temp °C
//  dist : Distance (M)
//  hum : Humidity percent 0 - 100
//  returns 8-bit TAU value for Tiny1c (1 - 128)
uint16_t estimate_tau(float ta, float dist, float hum);

#endif /* _T1C_TEMP_H_ */
