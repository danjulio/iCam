/*
 * File Task - manage all file related activities: maintaining the file catalog for quick
 * filesystem information access and saving/getting images.
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
#ifndef FILE_TASK_H
#define FILE_TASK_H

#include <stdint.h>
#include <stdbool.h>


//
// File Task Constants
//


// Period between checks for card present state.
#define FILE_CARD_CHECK_PERIOD_MSEC       2000

// File message display length
#define FILE_MSG_DISPLAY_MSEC             1000

//
// File Task notifications
//
#define FILE_NOTIFY_CARD_PRESENT_MASK     0x00000001
#define FILE_NOTIFY_CARD_REMOVED_MASK     0x00000002

#define FILE_NOTIFY_SAVE_JPG_MASK         0x00000010
#define FILE_NOTIFY_T1C_FRAME_MASK        0x00000020

#define FILE_NOTIFY_GUI_GET_CATALOG_MASK  0x00000100
#define FILE_NOTIFY_GUI_GET_IMAGE_MASK    0x00000200
#define FILE_NOTIFY_GUI_DEL_FILE_MASK     0x00000400
#define FILE_NOTIFY_GUI_DEL_DIR_MASK      0x00000800
#define FILE_NOTIFY_GUI_FORMAT_MASK       0x00001000

#define FILE_NOTIFY_FW_UPD_EN_MASK        0x00010000
#define FILE_NOTIFY_FW_UPD_END_MASK       0x00020000



//
// File Task API
//
void file_task();
bool file_card_available();
char* file_get_file_save_status_string();  // To be called by an output task after getting file save notification
void file_set_catalog_index(int type);     // -1 = folder index, 0.. file index for specified folder index
char* file_get_catalog(int* num, int* type);
void file_set_delete_file(int dir_index, int file_index);
void file_set_image_fileinfo(int dir_index, int file_index);
void file_set_timelapse_info(bool en, bool notify, uint32_t interval, uint32_t num);

#endif /* FILE_TASK_H */
