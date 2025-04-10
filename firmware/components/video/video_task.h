/*
 * Video Task
 *
 * Initialize video library and render Tiny1C data info a frame buffer
 * for PAL or NTSC video output.  Manage text-based user-interface.
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
#ifndef VID_TASK_H
#define VID_TASK_H

#include <stdbool.h>
#include <stdint.h>

//
// VID Task Constants
//

// Battery state update interval
#define VID_BATT_STATE_UPDATE_MSEC          5000

// Splash screen display time
#define VID_SPLASH_DISPLAY_MSEC             10000

// Timelapse start/stop message time
#define VID_TIMELAPSE_MSG_MSEC              700

// Timelapse indication update period
#define VID_TIMELAPSE_TOGGLE_MSEC           1000

//
// VID Task notifications
//
// From lep_task
#define VID_NOTIFY_T1C_FRAME_MASK_1         0x00000001
#define VID_NOTIFY_T1C_FRAME_MASK_2         0x00000002

// From ctrl_task
#define VID_NOTIFY_B1_SHORT_PRESS_MASK      0x00000010
#define VID_NOTIFY_B2_SHORT_PRESS_MASK      0x00000020
#define VID_NOTIFY_B2_LONG_PRESS_MASK       0x00000040
#define VID_NOTIFY_CRIT_BATT_DET_MASK       0x00000100
#define VID_NOTIFY_SHUTDOWN_MASK            0x00000200

// From file_task
#define VID_NOTIFY_FILE_MSG_ON_MASK         0x00001000
#define VID_NOTIFY_FILE_MSG_OFF_MASK        0x00002000
#define VID_NOTIFY_FILE_TIMELAPSE_ON_MASK   0x00004000
#define VID_NOTIFY_FILE_TIMELAPSE_OFF_MASK  0x00008000



//
// VID Task API
//
void vid_task();

#endif /* VID_TASK_H */