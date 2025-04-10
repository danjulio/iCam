/*
 * Web Task - implement simple web server to send compressed single webpage app
 * to connected browser and then manage communications with the page over a websocket.
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
#ifndef WEB_TASK_H
#define WEB_TASK_H

#include <stdbool.h>
#include <stdint.h>

//
// WEB Task Constants
//

//
// WEB Task notifications
//
// From t1c_task
#define WEB_NOTIFY_T1C_FRAME_MASK_1         0x00000001
#define WEB_NOTIFY_T1C_FRAME_MASK_2         0x00000002

// From ctrl_task/web_task
#define WEB_NOTIFY_TAKE_PICTURE_MASK        0x00000010
#define WEB_NOTIFY_NETWORK_DISC_MASK        0x00000020
#define WEB_NOTIFY_FW_UPD_EN_MASK           0x00000040
#define WEB_NOTIFY_FW_UPD_END_MASK          0x00000080
#define WEB_NOTIFY_CRIT_BATT_DET_MASK       0x00000100
#define WEB_NOTIFY_SHUTDOWN_MASK            0x00000200

// From file_task
#define WEB_NOTIFY_FILE_MSG_ON_MASK         0x00001000
#define WEB_NOTIFY_FILE_MSG_OFF_MASK        0x00002000
#define WEB_NOTIFY_FILE_CATALOG_READY_MASK  0x00004000
#define WEB_NOTIFY_FILE_IMAGE_READY_MASK    0x00008000
#define WEB_NOTIFY_FILE_TIMELAPSE_ON_MASK   0x00010000
#define WEB_NOTIFY_FILE_TIMELAPSE_OFF_MASK  0x00020000

// From a controller activity
#define WEB_NOTIFY_CTRL_ACT_SUCCEEDED_MASK  0x00100000
#define WEB_NOTIFY_CTRL_ACT_FAILED_MASK     0x00200000


//
// WEB Task API
//
void web_task();
bool web_has_client();

#endif /* WEB_TASK_H */
