/*
 * Tiny1C Task
 *
 * Initializes and then repeatedly reads image data from the Tiny1C core via SPI and also
 * reads spotmeter, min/max and optionally region temperature information via the I2C
 * CCI interface.  The camera  module is operated in the Y16 mode.  Applies a linear
 * transformation to convert the image data into 8-bit words loaded into ping-pong buffers
 * for other tasks.
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
#ifndef T1C_TASK_H
#define T1C_TASK_H

#include <stdbool.h>
#include <stdint.h>


//
// Tiny1C Task Constants
//

// Camera info
#define T1C_FPS    25


// Customized default initial shutter timing values
#define T1C_SHUTTER_PREVIEW_1_SECS       2
#define T1C_SHUTTER_PREVIEW_2_SECS       4
#define T1C_SHUTTER_GAIN_CHG_1_SECS      2
#define T1C_SHUTTER_GAIN_CHG_2_SECS      4


//
// Notifications
//

// From ourselves
#define T1C_NOTIFY_SET_SPOT_LOC_MASK     0x00000001
#define T1C_NOTIFY_SET_REGION_LOC_MASK   0x00000002

// From a command handler
#define T1C_NOTIFY_RESTORE_DEFAULT_MASK  0x00000010
#define T1C_NOTIFY_CAL_1_MASK            0x00000020
#define T1C_NOTIFY_CAL_2L_MASK           0x00000040
#define T1C_NOTIFY_CAL_2H_MASK           0x00000080
#define T1C_NOTIFY_FFC_MASK              0x00000100
#define T1C_NOTIFY_ENV_UPD_MASK          0x00000200
#define T1C_NOTIFY_UPD_T1C_CONFIG        0x00000400

// From env_task
#define T1C_NOTIFY_SET_T_H_MASK          0x00001000
#define T1C_NOTIFY_SET_DIST_MASK         0x00002000

// From file_task
#define T1C_NOTIFY_FILE_GET_IMAGE_MASK   0x00010000



//
// Tiny1C Task API
//
void t1c_task();
bool t1c_task_ready();

// API calls for other tasks
void t1c_set_spot_enable(bool en);
void t1c_set_spot_location(uint16_t x, uint16_t y);
void t1c_set_minmax_marker_enable(bool en);
void t1c_set_minmax_temp_enable(bool en);
void t1c_set_region_enable(bool en);
void t1c_set_region_location(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

void t1c_set_ambient_temp(int16_t t, bool valid);
void t1c_set_ambient_humidity(uint16_t h, bool valid);
void t1c_set_target_distance(uint16_t cm, bool valid);

bool t1c_set_param_shutter(uint8_t param, uint16_t param_value);
bool t1c_set_param_image(uint8_t param, uint16_t param_value);
bool t1c_set_param_tpd(uint8_t param, uint16_t param_value);

// Called before initiating a calibration activity, contains the temp in °K
void t1c_set_blackbody_temp(uint16_t temp_k);

char* t1c_get_module_version();
char* t1c_get_module_sn();

#endif /* T1C_TASK_H */