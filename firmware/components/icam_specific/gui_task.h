/*
 * GUI Task
 *
 * Contains functions to initialize the LVGL GUI system and a task
 * to evaluate its display related sub-tasks.  The GUI Task is responsible
 * for all access (updating) of the GUI managed by LVGL.
 *
 * Copyright 2020-2024 Dan Julio
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
#ifndef GUI_TASK_H
#define GUI_TASK_H

#include <stdbool.h>
#include <stdint.h>


//
// GUI Task Constants
//

// LVGL evaluation rate (mSec)
#define GUI_LVGL_TICK_MSEC         1
#define GUI_TASK_NORM_EVAL_MSEC    20
#define GUI_TASK_FAST_EVAL_MSEC    10

// Screen indicies
#define GUI_MAIN_PAGE_IMAGE        0
#define GUI_MAIN_PAGE_SETTINGS     1
#define GUI_MAIN_PAGE_LIBRARY      2

#define GUI_NUM_MAIN_PAGES         3

// Background color (should match theme background - A kludge, I know.  Specified here because
// themes don't allow direct access to it and IMHO the LVGL theme system is incredibly hard to use)
#define GUI_THEME_BG_COLOR         lv_color_hex(0x444b5a)
#define GUI_THEME_SLD_BG_COLOR     lv_color_hex(0x3d4351)
#define GUI_THEME_RLR_BG_COLOR     lv_color_hex(0x3d4351)
#define GUI_THEME_TBL_BG_COLOR     lv_color_hex(0x3d4351)

// Messagebox indicies
#define GUI_MSGBOX_INT_ERR         1

// Notifications
//
// From tiny1c_task
#define GUI_NOTIFY_T1C_FRAME_MASK_1         0x00000001
#define GUI_NOTIFY_T1C_FRAME_MASK_2         0x00000002

// From control tasks
#define GUI_NOTIFY_TAKE_PICTURE_MASK        0x00000010
#define GUI_NOTIFY_CRIT_BATT_DET_MASK       0x00000020
#define GUI_NOTIFY_SHUTDOWN_MASK            0x00000040
#define GUI_NOTIFY_FW_UPD_EN_MASK           0x00000100
#define GUI_NOTIFY_FW_UPD_END_MASK          0x00000200

// From file_task
#define GUI_NOTIFY_FILE_MSG_ON_MASK         0x00001000
#define GUI_NOTIFY_FILE_MSG_OFF_MASK        0x00002000
#define GUI_NOTIFY_FILE_CATALOG_READY_MASK  0x00004000
#define GUI_NOTIFY_FILE_IMAGE_READY_MASK    0x00008000
#define GUI_NOTIFY_FILE_TIMELAPSE_ON_MASK   0x00010000
#define GUI_NOTIFY_FILE_TIMELAPSE_OFF_MASK  0x00020000

// From a controller activity
#define GUI_NOTIFY_CTRL_ACT_SUCCEEDED_MASK  0x00100000
#define GUI_NOTIFY_CTRL_ACT_FAILED_MASK     0x00200000

// From gcore_task
#define GUI_NOTIFY_SCREENDUMP_MASK          0x80000000



//
// GUI Task API
//
void gui_task();

// API calls for other tasks
void gui_main_set_page(uint32_t page);

#endif /* GUI_TASK_H */