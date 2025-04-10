/*
 * Control Interface Task - Hardware management task for iCamMini.  Device-specific
 * functionality for monitoring the battery, controls, SD-card presence, video/web
 * jumper and controlling the Status LED.
 *
 * Copyright 2020-s024 Dan Julio
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
#ifndef CTRL_TASK_H
#define CTRL_TASK_H

#include <stdbool.h>
#include <stdint.h>
#include "system_config.h"


//
// Control Task Constants
//

// Control Task evaluation interval
#define CTRL_EVAL_MSEC             50

// Battery read interval
#define CTRL_BATT_READ_MSEC        1000

// Battery low indication interval
#define CTRL_LOW_BATT_IND_MSEC     (LOW_BATT_INDICATION_SEC * 1000)

// Critical battery shutdown period
#define CTRL_CRIT_BATT_OFF_MSEC   (CRIT_BATTERY_OFF_SEC * 1000)

// Timeouts (multiples of CTRL_EVAL_MSEC)
#define CTRL_BTN_LONG_PRESS_MSEC   3000
#define CTRL_RESET_ALERT_MSEC      3000
#define CTRL_FAST_BLINK_MSEC       100
#define CTRL_SLOW_BLINK_MSEC       1000
#define CTRL_FAULT_BLINK_ON_MSEC   200
#define CTRL_FAULT_BLINK_OFF_MSEC  300
#define CTRL_FAULT_IDLE_MSEC       2000
#define CTRL_FW_UPD_REQ_BLINK_MSEC 250
#define CTRL_FW_LB_BLINK_ON_MSEC   150
#define CTRL_FW_LB_BLINK_OFF_MSEC  50

// Fault Types - sets blink count too
#define CTRL_FAULT_NONE           0
#define CTRL_FAULT_ESP32_INIT     1
#define CTRL_FAULT_PERIPH_INIT    2
#define CTRL_FAULT_MEM_INIT       3
#define CTRL_FAULT_T1C_CCI        4
#define CTRL_FAULT_T1C_VOSPI      5
#define CTRL_FAULT_T1C_SYNC       6
#define CTRL_FAULT_VIDEO          7
#define CTRL_FAULT_NETWORK        7
#define CTRL_FAULT_WEB_SERVER     8
#define CTRL_FAULT_FW_UPDATE      9

// Output mode
#define CTRL_OUTPUT_VID           0
#define CTRL_OUTPUT_WIFI          1

// Control Task notifications
#define CTRL_NOTIFY_STARTUP_DONE      0x00000001
#define CTRL_NOTIFY_FAULT             0x00000002
#define CTRL_NOTIFY_FAULT_CLEAR       0x00000004
#define CTRL_NOTIFY_SHUTDOWN          0x00000008

#define CTRL_NOTIFY_FW_UPD_REQ        0x00000010
#define CTRL_NOTIFY_FW_UPD_PROCESS    0x00000020
#define CTRL_NOTIFY_FW_UPD_DONE       0x00000040
#define CTRL_NOTIFY_FW_UPD_REBOOT     0x00000080

#define CTRL_NOTIFY_SAVE_START        0x00000100
#define CTRL_NOTIFY_SAVE_END          0x00000200

#define CTRL_NOTIFY_RESTART_NETWORK   0x00001000


//
// Control Task API
//
void ctrl_task();
int ctrl_get_output_mode();
void ctrl_set_fault_type(int f);
void ctrl_set_save_success(bool s);
uint16_t ctrl_get_batt_mv();
int ctrl_get_batt_percent();
bool ctrl_get_sdcard_present();

#endif /* CTRL_TASK_H */