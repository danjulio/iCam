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
#include "file_task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "file_render.h"
#include "file_utilities.h"
#include "palettes.h"
#include "out_state_utilities.h"
#include "system_config.h"
#include "sys_utilities.h"
#include "t1c_task.h"
#include "time_utilities.h"
#include "tiny1c.h"
#include "tjpgd.h"
#include <stdio.h>
#include <string.h>

#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

#ifdef CONFIG_BUILD_ICAM_MINI
	#include "ctrl_task.h"
	#include "video_task.h"
	#include "web_task.h"
#else
	#include "gcore_task.h"
	#include "gui_task.h"
#endif



//
// File Task private constants
//
#define FILE_TASK_EVAL_NORM_MSEC 50
#define FILE_TASK_EVAL_FAST_MSEC 10

// Uncomment to log various file processing timestamps
//#define LOG_WRITE_TIMESTAMP
//#define LOG_READ_TIMESTAMP

// Bytes per pixel for tjpgd decoder
#ifdef CONFIG_BUILD_ICAM_MINI
	#define TJPGD_NUM_BPP        3
#else
	#define TJPGD_NUM_BPP        2
#endif

// tjpgd decoder work buffer length (seem to use about 2776 bytes)
#define TJPGD_WORK_BUF_LEN       3500



//
// File Task private typedefs
//

// Session identifier for tjpgd decoder input/output functions
typedef struct {
    FILE *fp;               /* Input stream */
    uint8_t *fbuf;          /* Output frame buffer */
    unsigned int wfbuf;     /* Width of the frame buffer [pix] */
} tjpgd_iodev_t;

// Timelapse configuration
typedef struct {
	bool timelapse_en;
	bool timelapse_notify;
	uint32_t timelapse_interval;
	uint32_t timelapse_count;
} timelapse_config_t;



//
// File Task private variables
//
static const char* TAG = "file_task";

// Counter used to probe card for presence 
static int card_check_count = FILE_CARD_CHECK_PERIOD_MSEC / FILE_TASK_EVAL_NORM_MSEC;

// Hardware card detection state from the hardware switch on the card socket
static bool card_present = false;

// Card present and available for access
static bool card_available = false;

// Notifications
static bool save_image_requested = false;
static bool notify_image = false;

// Mode dependent notification variables
static TaskHandle_t output_task;
static uint32_t task_file_disp_msg_notification;
static uint32_t task_file_clear_msg_notification;
static uint32_t task_file_act_succeeded_notification;
static uint32_t task_file_act_failed_notification;
static uint32_t task_file_catalog_ready_notification;
static uint32_t task_file_image_ready_notification;
static uint32_t task_file_timelapse_start_notification;
static uint32_t task_file_timelapse_stop_notification;

// File save information string (file name or messages)
static char file_save_info[80];

// Filesystem catalog information for GUI commands
//  - Used to synchronize between this task and gui tasks
//  - Statically allocated buffer size based on longest possible name type
static int catalog_type;                            // -1 is list of directories
static int num_catalog_names;                       // Set with catalog_names_buffer
static char catalog_names_buffer[FILE_MAX_CATALOG_NAMES * FILE_NAME_LEN];

// Filesystem delete indicies for GUI commands
static int del_dir;
static int del_file;

// File image read directory + filename for GUI commands
static char file_read_filename[DIR_NAME_LEN + FILE_NAME_LEN + 2];

// Timelapse control
static bool timelapse_running = false;
static uint32_t timelapse_img_count;
static int64_t timelapse_trig_usec;
static timelapse_config_t cur_timelapse_config;
static timelapse_config_t new_timelapse_config;

// tjpgd work buffer
static uint8_t tjpgd_work_buf[TJPGD_WORK_BUF_LEN];



//
// File Task Forward Declarations for internal functions
//
static void _setup_notifications();
static void _handle_notifications();
static void _update_card_present_info();
static bool _catalog_filesystem();
static void _eval_timelapse();
static void _set_timelapse(bool en);
static bool _delete_dir(int dir_index);
static bool _delete_file(int dir_index, int file_index);
static bool _format_card();
static bool _read_jpeg_image();
static void _save_image_to_jpeg();
static void _notify_save_msg_start(bool success);
static void _notify_save_msg_end();
static size_t _tjpgd_in_func(JDEC* jd, uint8_t* buff, size_t nbyte);
static int _tjpgd_out_func(JDEC* jd, void* bitmap, JRECT* rect);
static char* _tjpgd_comment_func(int item_index, char* buf);


//
// File Task API
//
void file_task()
{
	ESP_LOGI(TAG, "Start task");
	
	// Setup our notifications
	_setup_notifications();
	
	while (1) {	
		if (save_image_requested) {
			vTaskDelay(pdMS_TO_TICKS(FILE_TASK_EVAL_FAST_MSEC));
		} else {
			vTaskDelay(pdMS_TO_TICKS(FILE_TASK_EVAL_NORM_MSEC));
		}
		
		_handle_notifications();
		
		_update_card_present_info();
		
		if (timelapse_running) {
			_eval_timelapse();
		}
		
		// Note: _save_image_to_jpeg may take significant time
		if (notify_image) {
			notify_image = false;
			if (save_image_requested) {
				save_image_requested = false;
				_save_image_to_jpeg();
				
				// Look for end of timelapse series
				if (timelapse_running && (timelapse_img_count >= cur_timelapse_config.timelapse_count)) {
					_set_timelapse(false);
				}
			}
		}
	}
}


/**
* Called by other tasks so HW card present and catalog present remain synced
* We don't protect it since it's a boolean...
*/
bool file_card_available()
{
	return card_available;
}


/**
* Called by an output task after getting a file save notification to retrieve the file
* name or error message
*/
char* file_get_file_save_status_string()
{
	return file_save_info;
}


/**
 * Called by a command handler prior to sending FILE_NOTIFY_GUI_GET_CATALOG_MASK
 */
void file_set_catalog_index(int type)
{
	catalog_type = type;
}


/**
 * Called by the output task to get the catalog list after notification
 */
char* file_get_catalog(int* num, int* type)
{
	*num = num_catalog_names;
	*type = catalog_type;
	return catalog_names_buffer;
}


/**
 * Called by a command handler prior to sending a delete notification
 */
void file_set_delete_file(int dir_index, int file_index)
{
	del_dir = dir_index;
	del_file = file_index;
}


/**
 * Called by a command handler prior to sending FILE_NOTIFY_GUI_GET_IMAGE_MASK
 */
void file_set_image_fileinfo(int dir_index, int file_index)
{
	directory_node_t* dn;
	file_node_t* fn;
	
	dn = file_get_indexed_directory(dir_index);
	if (dn != NULL) {
		fn = file_get_indexed_file(dn, file_index);
		if (fn != NULL) {
			sprintf(file_read_filename, "%s/%s", dn->nameP, fn->nameP);
		}
	}
}


/**
 * Called by a command handler prior to sending FILE_NOTIFY_TIMELAPSE_MASK
 */
void file_set_timelapse_info(bool en, bool notify, uint32_t interval, uint32_t num)
{
	new_timelapse_config.timelapse_en = en;
	new_timelapse_config.timelapse_notify = notify;
	new_timelapse_config.timelapse_interval = interval;
	new_timelapse_config.timelapse_count = num;
}



//
// File Task internal functions
//
/**
 * Setup the variables used to notify the current running output task of various events
 */
static void _setup_notifications()
{
#ifdef CONFIG_BUILD_ICAM_MINI
	if (ctrl_get_output_mode() == CTRL_OUTPUT_VID) {
		output_task = task_handle_vid;
		task_file_disp_msg_notification = VID_NOTIFY_FILE_MSG_ON_MASK;
		task_file_clear_msg_notification = VID_NOTIFY_FILE_MSG_OFF_MASK;
		task_file_act_succeeded_notification = 0;
		task_file_act_failed_notification = 0;
		task_file_catalog_ready_notification = 0;
		task_file_image_ready_notification = 0;
		task_file_timelapse_start_notification = VID_NOTIFY_FILE_TIMELAPSE_ON_MASK;
		task_file_timelapse_stop_notification = VID_NOTIFY_FILE_TIMELAPSE_OFF_MASK;
	} else {
		output_task = task_handle_web;
		task_file_disp_msg_notification = WEB_NOTIFY_FILE_MSG_ON_MASK;
		task_file_clear_msg_notification = WEB_NOTIFY_FILE_MSG_OFF_MASK;
		task_file_act_succeeded_notification = WEB_NOTIFY_CTRL_ACT_SUCCEEDED_MASK;
		task_file_act_failed_notification = WEB_NOTIFY_CTRL_ACT_FAILED_MASK;
		task_file_catalog_ready_notification = WEB_NOTIFY_FILE_CATALOG_READY_MASK;
		task_file_image_ready_notification = WEB_NOTIFY_FILE_IMAGE_READY_MASK;
		task_file_timelapse_start_notification = WEB_NOTIFY_FILE_TIMELAPSE_ON_MASK;
		task_file_timelapse_stop_notification = WEB_NOTIFY_FILE_TIMELAPSE_OFF_MASK;
	}
#else
	output_task = task_handle_gui;
	task_file_disp_msg_notification = GUI_NOTIFY_FILE_MSG_ON_MASK;
	task_file_clear_msg_notification = GUI_NOTIFY_FILE_MSG_OFF_MASK;
	task_file_act_succeeded_notification = GUI_NOTIFY_CTRL_ACT_SUCCEEDED_MASK;
	task_file_act_failed_notification = GUI_NOTIFY_CTRL_ACT_FAILED_MASK;
	task_file_catalog_ready_notification = GUI_NOTIFY_FILE_CATALOG_READY_MASK;
	task_file_image_ready_notification = GUI_NOTIFY_FILE_IMAGE_READY_MASK;
	task_file_timelapse_start_notification = GUI_NOTIFY_FILE_TIMELAPSE_ON_MASK;
	task_file_timelapse_stop_notification = GUI_NOTIFY_FILE_TIMELAPSE_OFF_MASK;
#endif
}


/**
 * Process notifications from other tasks
 */
static void _handle_notifications()
{
	uint32_t notification_value;
	
	notification_value = 0;
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, FILE_NOTIFY_CARD_PRESENT_MASK)) {
			card_present = true;
		}
		if (Notification(notification_value, FILE_NOTIFY_CARD_REMOVED_MASK)) {
			card_present = false;
		}
		
		if (Notification(notification_value, FILE_NOTIFY_SAVE_JPG_MASK)) {
			if (timelapse_running) {
				// Receiving this while a timelapse series is in progress ends the timelapse
				_set_timelapse(false);
			} else {
				if (new_timelapse_config.timelapse_en) {
					// Start timelapse
					_set_timelapse(true);
					
					// Clear timelapse request
					new_timelapse_config.timelapse_en = false;
				} else {
					// Single picture: Ask t1c_task for an image
					xTaskNotify(task_handle_t1c, T1C_NOTIFY_FILE_GET_IMAGE_MASK, eSetBits);
					save_image_requested = true;
				}
			}
		}
		
		if (Notification(notification_value, FILE_NOTIFY_T1C_FRAME_MASK)) {
			notify_image = true;
		}
		
		// note: we process deletions before get catalog for the case we're getting
		// a catalog after issuing a deletion command
		if (Notification(notification_value, FILE_NOTIFY_GUI_DEL_DIR_MASK)) {
			(void) _delete_dir(del_dir);
		}
		
		if (Notification(notification_value, FILE_NOTIFY_GUI_DEL_FILE_MASK)) {
			(void) _delete_file(del_dir, del_file);
		}

		if (Notification(notification_value, FILE_NOTIFY_GUI_GET_CATALOG_MASK)) {
			num_catalog_names = file_get_name_list(catalog_type, catalog_names_buffer);
			xTaskNotify(output_task, task_file_catalog_ready_notification, eSetBits);
		}
		
		if (Notification(notification_value, FILE_NOTIFY_GUI_GET_IMAGE_MASK)) {
			// Read image with previously set name
			if (_read_jpeg_image()) {
				xTaskNotify(output_task, task_file_image_ready_notification, eSetBits);
			}
		}
		
		if (Notification(notification_value, FILE_NOTIFY_GUI_FORMAT_MASK)) {
			if (_format_card()) {
				xTaskNotify(output_task, task_file_act_succeeded_notification, eSetBits);
			} else {
				xTaskNotify(output_task, task_file_act_failed_notification, eSetBits);
			}
		}
	}
}


/**
 * Handle card insertion/removal detection.  Initialize the a new card.  Update the
 * card present status available from file_utilities and notify the app_task of changes.
 */
static void _update_card_present_info()
{
	if (--card_check_count == 0) {
		if (card_present) {
			if (!card_available) {
				// Card has just shown up, see if we can initialize it
				if (file_reinit_card()) {
					ESP_LOGI(TAG, "SD Card detected inserted");
					
					// Mount it briefly to force a format if necessary and create an initial
					// filesystem information structure (catalog), then unmount it.
					if (file_mount_sdcard()) {
						card_available = _catalog_filesystem();
						file_unmount_sdcard();
					}
				}
			}
		} else {
			if (card_available) {
				// Card just removed, clear memory of it
				file_delete_filesystem_info();  // Delete the filesystem information structure (catalog)
				ESP_LOGI(TAG, "SD Card removed");
				card_available = false;
			}
		}
		
		// Reset timer
		card_check_count = FILE_CARD_CHECK_PERIOD_MSEC / FILE_TASK_EVAL_NORM_MSEC;
	}
}


/**
 * Generate the filesystem information structure.  Sends an event to app_task if 
 * the catalog was successfully created so that the system can use the filesystem.
 */
static bool _catalog_filesystem()
{
	bool valid = false;
	uint32_t kb_free;
	
	if (file_create_filesystem_info()) {
		kb_free = (uint32_t) (file_get_storage_free() / 1024);
		ESP_LOGI(TAG, "Filesystem catalog created");
		ESP_LOGI(TAG, "    %d files", file_get_num_files());
		if (kb_free > (1024*1024)) {
			ESP_LOGI(TAG, "    %1.1f GB free", (float) kb_free / (1024*1024));
		} else if (kb_free > 1024) {
			ESP_LOGI(TAG, "    %1.1f MB free", (float) kb_free / 1024);
		} else {
			ESP_LOGI(TAG, "    %lu KB free", kb_free);
		}
		valid = true;
	} else {
		ESP_LOGE(TAG, "Could not index filesystem");
	}
	
	return valid;
}


/**
 * Evaluate the timelapse timer and logic to trigger image captures
 *   Assumes timelapse has been initiated
 *   Assumes timelapse will be ended by processing of last image
 */
static void _eval_timelapse()
{
	int64_t cur_usec = esp_timer_get_time();
	
	if (cur_usec >= timelapse_trig_usec) {
		// Update trigger timestampe for next time
		timelapse_trig_usec += ((int64_t) cur_timelapse_config.timelapse_interval * 1000000);
		
		// Increment image count
		timelapse_img_count += 1;
		
		// Ask t1c_task for an image
		xTaskNotify(task_handle_t1c, T1C_NOTIFY_FILE_GET_IMAGE_MASK, eSetBits);
		save_image_requested = true; 
	}
}
/*
	
*/

/**
 * Start or stop timelapse picture taking
 */
static void _set_timelapse(bool en)
{
	if (en) {
		if (!timelapse_running) {
			ESP_LOGI(TAG, "Start Timelapse: %lu @ %lu seconds each", cur_timelapse_config.timelapse_count, cur_timelapse_config.timelapse_interval);
			
			// Setup the new configuration
			cur_timelapse_config = new_timelapse_config;
			timelapse_running = true;
			
			// Reset our count
			timelapse_img_count = 0;
			
			// Setup the first image trigger timestamp (one second from now)
			timelapse_trig_usec = esp_timer_get_time() + 1000000;
			
			// Inform the output task that we're starting timelapse operation
			xTaskNotify(output_task, task_file_timelapse_start_notification, eSetBits);
		}
	} else {
		if (timelapse_running) {
			ESP_LOGI(TAG, "Stop Timelapse");
			timelapse_running = false;
			
			// Inform the output task that we're stopping timelapse operation
			xTaskNotify(output_task, task_file_timelapse_stop_notification, eSetBits);
		}
	}
}


/**
 * Delete a directory.  Update the catalog.
 */
static bool _delete_dir(int dir_index)
{
	bool success = true;
	directory_node_t* dir_node;
	int i;
	
	// Get the directory node associated with this index
	dir_node = file_get_indexed_directory(dir_index);
	if (dir_node != NULL) {
		// Attempt to mount the filesystem
		success = file_mount_sdcard();
		if (success) {
			// Attempt to delete the directory (and all files in it)
			success = file_delete_directory(dir_node->nameP);
			if (success) {
				// Delete file entries in this directory node from the catalog
				if (dir_node->num_files != 0) {
					for (i=dir_node->num_files-1; i>=0; i--) {
						file_delete_file_info(dir_node, i);
					}
				}
				
				// Delete the directory node
				file_delete_directory_info(dir_index);
			}
			
			file_unmount_sdcard();
		}
	}
	
	return success;
}


/**
 * Delete a file.  Update the catalog.
 */
static bool _delete_file(int dir_index, int file_index)
{
	bool success = true;
	directory_node_t* dir_node;
	file_node_t* file_node;
	
	// Get the directory node associated with this index
	dir_node = file_get_indexed_directory(dir_index);
	
	if (dir_node != NULL) {
		// Attempt to mount the filesystem
		success = file_mount_sdcard();
		if (success) {
			// Attempt to delete the file
			file_node = file_get_indexed_file(dir_node, file_index);
			if (file_node != NULL) {
				success = file_delete_file(dir_node->nameP, file_node->nameP);
				if (success) {
					// Delete the file entry from the catalog
					file_delete_file_info(dir_node, file_index);
				}
			}
			
			file_unmount_sdcard();
		}			
	}
	
	return success;
}


/**
 * Format the storage medium.  Stop any ongoing playback or recording.
 */ 
static bool _format_card()
{
	if (!card_available) {
		ESP_LOGI(TAG, "No SD Card for format");
		return false;
	}
	
	// Execute the format and delete the filesystem information structure (catalog)
	if (file_format_card()) {
		ESP_LOGI(TAG, "Format SD Card");
		file_delete_filesystem_info();
	} else {
		ESP_LOGE(TAG, "Format SD Card failed");
		return false;
	}
	
	// Mount it briefly to update the card statistics
	if (file_mount_sdcard()) {
		file_unmount_sdcard();
	}	
	
	return true;
}


static void _save_image_to_jpeg()
{
	bool new_dir;
	char* dir_name;
	char* file_name;
	directory_node_t* cat_dir_node;
	FILE* fd;
	int ret;
	t1c_buffer_t* t1cP = &file_t1c_buffer;
	
	// Make sure a card is inserted
	if (!card_available) {
		strcpy(file_save_info, "No SD Card");
		_notify_save_msg_start(false);
		vTaskDelay(pdMS_TO_TICKS(FILE_MSG_DISPLAY_MSEC));
		_notify_save_msg_end();
		return;
	}
	
	// Attempt to open the card
	if (!file_mount_sdcard()) {
		strcpy(file_save_info, "Can't mount SD Card");
		_notify_save_msg_start(false);
		vTaskDelay(pdMS_TO_TICKS(FILE_MSG_DISPLAY_MSEC));
		_notify_save_msg_end();
		return;
	}
	
	// Attempt to get a file to write to
	if (!file_open_image_write_file(&fd)) {
		strcpy(file_save_info, "Can't write to SD Card");
		_notify_save_msg_start(false);
		vTaskDelay(pdMS_TO_TICKS(FILE_MSG_DISPLAY_MSEC));
		_notify_save_msg_end();
		return;
	}
	
	// Let the output task know what file we're writing too
	dir_name = file_get_open_write_dirname(&new_dir);
	file_name = file_get_open_write_filename();
	ESP_LOGI(TAG, "Writing %s/%s %s", dir_name, file_name, new_dir ? "(new dir)" : "");
	sprintf(file_save_info, "Saving %s", file_name);
	_notify_save_msg_start(true);
	
	// Render the raw Tiny1C data into the 24-bit RGB (RGB888) buffer
	file_render_t1c_data(t1cP, rgb_save_image);
	
	// Render overlay data if enabled
	if (out_state.save_ovl_en) {
		// Configure the image orientation 
		file_render_set_orientation(out_state.is_portrait);
		
		// Draw enabled markers
		if (out_state.min_max_mrk_enable && t1cP->minmax_valid) {
			file_render_min_max_markers(t1cP, rgb_save_image, &out_state);
		}
		
		if (out_state.region_enable && t1cP->region_valid) {
			file_render_region_marker(t1cP, rgb_save_image, &out_state);
			file_render_region_temps(t1cP, rgb_save_image, &out_state);
		}
		
		if (out_state.spotmeter_enable && t1cP->spot_valid) {
			file_render_spotmeter(t1cP, rgb_save_image, &out_state);
		}
		
		// Then draw the palette (over a marker if necessary)
		file_render_palette(rgb_save_image, &out_state);
		file_render_min_max_temps(t1cP, rgb_save_image, &out_state);
		if (out_state.spotmeter_enable && t1cP->spot_valid) {
			file_render_palette_marker(t1cP, rgb_save_image, &out_state);
		}
		
		// Finally add environmental conditions if they exist
		file_render_env_info(t1cP, rgb_save_image, &out_state);
	}
	
	// Compress and write the jpeg file (closes file)
	tje_register_comment_callback(_tjpgd_comment_func);
	ret = tje_encode_to_file_at_quality(fd, 3, T1C_WIDTH, T1C_HEIGHT, 4, (unsigned char*) rgb_save_image);
	if (ret == 1) {
		// Add the file to our filesystem catalog
		if (new_dir) {
			cat_dir_node = file_add_directory_info(dir_name);
		} else {
			ret = file_get_named_directory_index(dir_name);
			if (ret < 0) ret = file_get_num_directories() - 1;
			cat_dir_node = file_get_indexed_directory(ret);
		}
		(void) file_add_file_info(cat_dir_node, file_name);
		_notify_save_msg_end();
	} else {
		// End previous display
		_notify_save_msg_end();
		vTaskDelay(pdMS_TO_TICKS(50));  // Allow the output task to clear the previous message
		
		// Start new display
		ESP_LOGE(TAG, "Jpeg save failed");
		strcpy(file_save_info, "File save failed");
		_notify_save_msg_start(false);
		vTaskDelay(pdMS_TO_TICKS(FILE_MSG_DISPLAY_MSEC));
		_notify_save_msg_end();
	}
	
	file_unmount_sdcard();
}


static bool _read_jpeg_image()
{
	bool success = true;
	FILE* fd;
	JRESULT res;
	JDEC jdec;
	tjpgd_iodev_t devid;
	
	// Attempt to open the card
	if (!file_mount_sdcard()) {
		strcpy(file_save_info, "Can't mount SD Card");
		return false;
	}
	
	if (file_open_image_read_file(file_read_filename, &fd)) {
		// Setup tjpgd to decompress the file
		devid.fp = fd;
		res = jd_prepare(&jdec, _tjpgd_in_func, tjpgd_work_buf, TJPGD_WORK_BUF_LEN, &devid);
//		ESP_LOGI(TAG, "jpeg dim = %d, %d, work = %d", (int) jdec.width, (int) jdec.height, TJPGD_WORK_BUF_LEN - jdec.sz_pool);
		if (res == JDR_OK) {
			// Start the decompression
			devid.fbuf = (uint8_t*) rgb_file_image;
			devid.wfbuf = jdec.width;
			res = jd_decomp(&jdec, _tjpgd_out_func, 0);
			if (res != JDR_OK) {
				ESP_LOGE(TAG, "jd_decomp failed with %d", (int) res);
				success = false;
			}
		} else {
			ESP_LOGE(TAG, "jd_prepare failed with %d", (int) res);
			success = false;
		}
		file_close_file(fd);
	} else {
		ESP_LOGE(TAG, "Open %s failed", file_read_filename);
		success = false;
	}
	
	file_unmount_sdcard();
	
	return success;
}


static void _notify_save_msg_start(bool success)
{
	// Display notification for all single saved images and for timelapse images if enabled
	if (!(timelapse_running && (cur_timelapse_config.timelapse_notify == false))) {
		xTaskNotify(output_task, task_file_disp_msg_notification, eSetBits);
	}
	
#ifdef CONFIG_BUILD_ICAM_MINI
	ctrl_set_save_success(success);
	xTaskNotify(task_handle_ctrl, CTRL_NOTIFY_SAVE_START, eSetBits);
#endif
}


static void _notify_save_msg_end()
{
	if (!(timelapse_running && (cur_timelapse_config.timelapse_notify == false))) {
		xTaskNotify(output_task, task_file_clear_msg_notification, eSetBits);
	}
	
#ifdef CONFIG_BUILD_ICAM_MINI
	xTaskNotify(task_handle_ctrl, CTRL_NOTIFY_SAVE_END, eSetBits);
#endif
}


static size_t _tjpgd_in_func (JDEC* jd, uint8_t* buff, size_t nbyte)
{
	tjpgd_iodev_t *dev = (tjpgd_iodev_t*)jd->device;   /* Session identifier (5th argument of jd_prepare function) */

    if (buff) {
    	// Read data from imput stream
        return fread(buff, 1, nbyte, dev->fp);
    } else {
    	// Remove data from input stream
        return fseek(dev->fp, nbyte, SEEK_CUR) ? 0 : nbyte;
    }
}


static int _tjpgd_out_func (JDEC* jd, void* bitmap, JRECT* rect)
{
	tjpgd_iodev_t *dev = (tjpgd_iodev_t*)jd->device;   /* Session identifier (5th argument of jd_prepare function) */
    uint8_t *src, *dst;
    uint16_t y, bws;
    unsigned int bwd;

	// Copy the output image rectangle to the frame buffer 
    src = (uint8_t*)bitmap;
    dst = dev->fbuf + TJPGD_NUM_BPP * (rect->top * dev->wfbuf + rect->left);
    bws = TJPGD_NUM_BPP * (rect->right - rect->left + 1);
    bwd = TJPGD_NUM_BPP * dev->wfbuf;
    for (y = rect->top; y <= rect->bottom; y++) {
        memcpy(dst, src, bws);
        src += bws; dst += bwd;
    }
	
    return 1;    /* Continue to decompress */
}


static char* _tjpgd_comment_func(int item_index, char* buf)
{
	const esp_app_desc_t* app_desc;
	float d;
	int i, t;
	uint16_t mv;
	tmElements_t te;
	
	switch (item_index) {
		case 0:
#ifdef CONFIG_BUILD_ICAM_MINI
			strcpy(buf, "Platform: iCamMini");
#else
			strcpy(buf, "Platform: iCam");
#endif
			break;
		case 1:
			app_desc = esp_app_get_description();
			sprintf(buf, "Firmware Version: %s", app_desc->version);
			break;
		case 2:
			sprintf(buf, "Tiny1C Version: %s", t1c_get_module_version());
			break;
		case 3:
			sprintf(buf, "Tiny1C Serial Number: %s", t1c_get_module_sn());
			break;
		case 4:
			time_get(&te);
			strcpy(buf, "Time: ");
			time_get_disp_string(&te, &buf[6]);
			break;
		case 5:
			if (timelapse_running == false) {
				strcpy(buf, "Type: Single");
			} else {
				sprintf(buf, "Type: Timelapse %lu of %lu", timelapse_img_count, cur_timelapse_config.timelapse_count);
			}
			break;
		case 6:
#ifdef CONFIG_BUILD_ICAM_MINI
			mv = ctrl_get_batt_mv();
#else
			mv = gcore_get_batt_mv();
#endif
			if (mv > 4200) {
				sprintf(buf, "Internal DC: %1.2f V", (float) mv / 1000.0);
			} else {
				sprintf(buf, "Battery: %1.2f V", (float) mv / 1000.0);
			}
			break;
		case 7:
			sprintf(buf, "Palette: %s", get_palette_name(out_state.sav_palette_index));
			break;
		case 8:
			sprintf(buf, "Scene Min: %1.1f °%c (%u, %u)", temp_to_float_temp(file_t1c_buffer.max_min_temp_info.min_temp, out_state.temp_unit_C),
			        out_state.temp_unit_C ? 'C' : 'F', file_t1c_buffer.max_min_temp_info.min_temp_point.x, file_t1c_buffer.max_min_temp_info.min_temp_point.y);
			break;
		case 9:
			sprintf(buf, "Scene Max: %1.1f °%c (%u, %u)", temp_to_float_temp(file_t1c_buffer.max_min_temp_info.max_temp, out_state.temp_unit_C),
			        out_state.temp_unit_C ? 'C' : 'F', file_t1c_buffer.max_min_temp_info.max_temp_point.x, file_t1c_buffer.max_min_temp_info.max_temp_point.y);
			break;
		case 10:
			if (out_state.spotmeter_enable) {
				sprintf(buf, "Spot Meter: %1.1f °%c (%u, %u)", temp_to_float_temp(file_t1c_buffer.spot_temp, out_state.temp_unit_C),
				        out_state.temp_unit_C ? 'C' : 'F', file_t1c_buffer.spot_point.x, file_t1c_buffer.spot_point.y);
			} else {
				strcpy(buf, "Spot Meter: Not Enabled");
			}
			break;
		case 11:
			if (out_state.region_enable) {
				sprintf(buf, "Region Meter Avg: %1.1f °%c (%u, %u) - (%u, %u)", temp_to_float_temp(file_t1c_buffer.region_temp_info.temp_info_value.ave_temp, out_state.temp_unit_C),
				        out_state.temp_unit_C ? 'C' : 'F',
				        file_t1c_buffer.region_points.start_point.x, file_t1c_buffer.region_points.start_point.y,
				        file_t1c_buffer.region_points.end_point.x, file_t1c_buffer.region_points.end_point.y);
			} else {
				strcpy(buf, "Region Meter Avg: Not Enabled");
			}
			break;
		case 12:
			if (out_state.region_enable) {
				sprintf(buf, "Region Meter Min: %1.1f °%c (%u, %u)", temp_to_float_temp(file_t1c_buffer.region_temp_info.temp_info_value.min_temp, out_state.temp_unit_C),
				        out_state.temp_unit_C ? 'C' : 'F', file_t1c_buffer.region_temp_info.min_temp_point.x, file_t1c_buffer.region_temp_info.min_temp_point.y);
			} else {
				strcpy(buf, "Region Meter Min: Not Enabled");
			}
			break;
		case 13:
			if (out_state.region_enable) {
				sprintf(buf, "Region Meter Max: %1.1f °%c (%u, %u)", temp_to_float_temp(file_t1c_buffer.region_temp_info.temp_info_value.max_temp, out_state.temp_unit_C),
				        out_state.temp_unit_C ? 'C' : 'F', file_t1c_buffer.region_temp_info.max_temp_point.x, file_t1c_buffer.region_temp_info.max_temp_point.y);
			} else {
				strcpy(buf, "Region Meter Max: Not Enabled");
			}
			break;
		case 14:
			if (file_t1c_buffer.amb_temp_valid) {
				t = (int) file_t1c_buffer.amb_temp;
				if (!out_state.temp_unit_C) {
					t = t * 9.0 / 5.0 + 32.0;
				}
				sprintf(buf, "Environmental Temp: %d °%c (Correction %s)", t, out_state.temp_unit_C ? 'C' : 'F',
				        out_state.use_auto_ambient ? "Enabled" : "Disabled");
			} else {
				strcpy(buf, "Environmental Temp: Not available");
			}
			break;
		case 15:
			if (file_t1c_buffer.amb_hum_valid) {
				sprintf(buf, "Environmental Humidity: %u%% (Correction %s)", file_t1c_buffer.amb_hum, out_state.use_auto_ambient ? "Enabled" : "Disabled");
			} else {
				strcpy(buf, "Environmental Humidity: Not available");
			}
			break;
		case 16:
			if (file_t1c_buffer.distance_valid) {
				if (out_state.temp_unit_C) {
					d = (float) file_t1c_buffer.distance / 100.0;
					sprintf(buf, "Environmental Distance: %1.2f m (Correction %s)", d, out_state.use_auto_ambient ? "Enabled" : "Disabled");
				} else {
					d = (float) file_t1c_buffer.distance / (2.54 * 12);
					i = floor(d);
					t = round((d - i) * 12);
					if (t == 12) {
						t = 0;
						i += 1;
					}
					sprintf(buf, "Environmental Distance: %d' %d\" (Correction %s)", i, t, out_state.use_auto_ambient ? "Enabled" : "Disabled");
				}
			} else {
				strcpy(buf, "Environmental Distance: Not available");
			}
			break; 
		case 17:
			sprintf(buf, "TNR: %u", file_t1c_meta.image_params[IMAGE_PROP_LEVEL_TNR]);
			break;
		case 18:
			sprintf(buf, "SNR: %u", file_t1c_meta.image_params[IMAGE_PROP_LEVEL_SNR]);
			break;
		case 19:
			sprintf(buf, "DDE: %u", file_t1c_meta.image_params[IMAGE_PROP_LEVEL_DDE]);
			break;
		case 20:
			sprintf(buf, "Brightness: %u", file_t1c_meta.image_params[IMAGE_PROP_LEVEL_BRIGHTNESS]);
			break;
		case 21:
			sprintf(buf, "Contrast: %u", file_t1c_meta.image_params[IMAGE_PROP_LEVEL_CONTRAST]);
			break;
		case 22:
			sprintf(buf, "AGC Enable: %u", file_t1c_meta.image_params[IMAGE_PROP_ONOFF_AGC]);
			break;
		case 23:
			sprintf(buf, "AGC Level: %u", file_t1c_meta.image_params[IMAGE_PROP_MODE_AGC]);
			break;
		case 24:
			sprintf(buf, "AGC Max Gain: %u", file_t1c_meta.image_params[IMAGE_PROP_LEVEL_MAX_GAIN]);
			break;
		case 25:
			sprintf(buf, "AGC Brightness Offset: %u", file_t1c_meta.image_params[IMAGE_PROP_LEVEL_BOS]);
			break;
		case 26:
			sprintf(buf, "Mirror Flip: %u", file_t1c_meta.image_params[IMAGE_PROP_SEL_MIRROR_FLIP]);
			break;
		case 27:
			i = (int) param_to_emissivity_value(file_t1c_meta.tpd_params[TPD_PROP_EMS]);
			sprintf(buf, "Emissivity: %d%%", i);
			break;
		case 28:
			d = (float) param_to_temperature_value(file_t1c_meta.tpd_params[TPD_PROP_TA]);
			if (!out_state.temp_unit_C) {
				d = d * 9.0 / 5.0 + 32.0;
			}
			sprintf(buf, "Atmospheric Temp: %1.1f °%c", d, out_state.temp_unit_C ? 'C' : 'F');
			break;
		case 29:
			d = (float) param_to_temperature_value(file_t1c_meta.tpd_params[TPD_PROP_TU]);
			if (!out_state.temp_unit_C) {
				d = d * 9.0 / 5.0 + 32.0;
			}
			sprintf(buf, "Reflected Temp: %1.1f °%c", d, out_state.temp_unit_C ? 'C' : 'F');
			break;
		case 30:
			d = (float) param_to_dist_cm_value(file_t1c_meta.tpd_params[TPD_PROP_DISTANCE]);
			if (out_state.temp_unit_C) {
				d = d / 100.0;
				sprintf(buf, "Distance: %1.2f m", d);
			} else {
				d = d / (2.54 * 12);
				i = floor(d);
				t = round((d - i) * 12);
				if (t == 12) {
					t = 0;
					i += 1;
				}
				sprintf(buf, "Distance: %d' %d\"", i, t);
			}
			break;
		case 31:
			sprintf(buf, "Gain: %s", (file_t1c_meta.tpd_params[TPD_PROP_GAIN_SEL] == 0) ? "Low" : "High");
			break;
		default:
			return NULL;
	}
	
	return buf;
}
