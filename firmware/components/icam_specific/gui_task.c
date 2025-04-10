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
#include "esp_system.h"
#ifndef CONFIG_BUILD_ICAM_MINI

#include <arpa/inet.h>
#include "cmd_list.h"
#include "cmd_handlers.h"
#include "cmd_utilities.h"
#include "gcore_task.h"
#include "gui_cmd_handlers.h"
#include "gui_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_freertos_hooks.h"
#include "file_task.h"
#include "gui_page_image.h"
#include "gui_page_settings.h"
#include "gui_page_file_browser.h"
#include "gui_render.h"
#include "gui_state.h"
#include "gui_utilities.h"
#include "lv_conf.h"
#include "palettes.h"
#include "sys_utilities.h"
#include "system_config.h"
#include "tiny1c.h"
#include "disp_spi.h"
#include "disp_driver.h"
#include "touch_driver.h"

#if (CONFIG_SCREENDUMP_ENABLE == true)
#include "mem_fb.h"
#endif



//
// Local constants
//

// Undefine to display LVGL memory utilization on page change
//#define DUMP_MEM_INFO



//
// GUI Task typedefs
//
// Command packet data types
typedef enum {
	SEND_CMD_NETWORK_RESET,
	SEND_CMD_FW_UPD_EN,
	SEND_CMD_FW_UPD_END,
	SEND_CMD_CRIT_BATT,
	SEND_CMD_SHUTDOWN,
	SEND_CMD_FILE_MSG_ON,
	SEND_CMD_FILE_MSG_OFF
} send_cmd_type_t;



//
// GUI Task variables
//

static const char* TAG = "gui_task";

// Dual display update buffers to allow DMA/SPI transfer of one while the other is updated
static lv_color_t lvgl_disp_buf1[DISP_BUF_SIZE];
static lv_color_t lvgl_disp_buf2[DISP_BUF_SIZE];
static lv_disp_buf_t lvgl_disp_buf;

// Display driver
static lv_disp_drv_t lvgl_disp_drv;

// Touchscreen driver
static lv_indev_drv_t lvgl_indev_drv;

// Screen
static lv_obj_t* screen;

// Screen object array
static lv_obj_t* lv_pages[GUI_NUM_MAIN_PAGES];

// Page size
static uint16_t page_w;
static uint16_t page_h;



//
// GUI Task internal function forward declarations
//
static bool _gui_cmd_init();
static void _gui_send_image(int render_buf_index);
static void _gui_notification_handler();
static void _gui_lvgl_init();
static bool _gui_send_get_file_catalog_response();
static bool _gui_send_get_file_image_response(); 
static void _cmd_handler_set_shutdown(cmd_data_t data_type, uint32_t len, uint8_t* data);
static void IRAM_ATTR _lv_tick_callback();
#if (CONFIG_SCREENDUMP_ENABLE == true)
static void _gui_do_screendump();
#endif



//
// GUI Task API
//

void gui_task()
{
	ESP_LOGI(TAG, "Start task");
	
	// Initialize cmd interface
	if (!_gui_cmd_init()) {
		ESP_LOGE(TAG, "Could not initialize command interface");
		vTaskDelete(NULL);
	}

	// Initialize LVGL
	_gui_lvgl_init();
	
	// Our page dimensions are fixed
	page_w = LV_HOR_RES_MAX;
	page_h = LV_VER_RES_MAX;
	
	// Initialize the rendering engine
	if (!gui_render_init()) {
		ESP_LOGE(TAG, "gui_render_init failed");
		vTaskDelete(NULL);
	}
	
	// Setup the top-level screen
	screen = lv_obj_create(NULL, NULL);
	lv_obj_set_pos(screen, 0, 0);
	lv_obj_set_size(screen, page_w, page_h);
	lv_obj_set_style_local_bg_color(screen, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(0x0,0x0,0x0));
	lv_obj_set_click(screen, false);
	
	// Setup the application pages
	lv_pages[GUI_MAIN_PAGE_IMAGE] = gui_page_image_init(screen, page_w, page_h, false);
	lv_pages[GUI_MAIN_PAGE_SETTINGS] = gui_page_settings_init(screen, page_w, page_h, false);
	lv_pages[GUI_MAIN_PAGE_LIBRARY] = gui_page_file_browser_init(screen, page_w, page_h, false);

	// Start the display
	lv_scr_load(screen);
	
	// Display LVGL memory utilization
	gui_dump_mem_info();
	
	// Get the GUI state (this completes immediately so we don't need to poll for it done
	// like we do when running over a websocket)
	gui_state_init();
	
	// Configure the save palette
	set_save_palette(gui_state.palette_index);
	
	// Set the initially displayed screen
	gui_main_set_page(GUI_MAIN_PAGE_IMAGE);
	
	while (1) {
		lv_task_handler();
		
		// Handle incoming notifications
		_gui_notification_handler();
		
		// Handle incoming commands
		if (cmd_handler_take_picture_notification()) {
			// Trigger an image save and wait to be given the filename or message to display
			xTaskNotify(task_handle_file, FILE_NOTIFY_SAVE_JPG_MASK, eSetBits);
		}
		
		if (cmd_handler_stream_enabled()) {
			vTaskDelay(pdMS_TO_TICKS(GUI_TASK_FAST_EVAL_MSEC));
		} else {
			vTaskDelay(pdMS_TO_TICKS(GUI_TASK_NORM_EVAL_MSEC));
		}
	}
}


void gui_main_set_page(uint32_t page)
{
	if (page > GUI_NUM_MAIN_PAGES) return;
	
	gui_page_image_set_active(page == GUI_MAIN_PAGE_IMAGE);
	gui_page_settings_set_active(page == GUI_MAIN_PAGE_SETTINGS);
	gui_page_file_browser_set_active(page == GUI_MAIN_PAGE_LIBRARY);
	
#ifdef DUMP_MEM_INFO
	gui_dump_mem_info();
#endif
}



//
// GUI Task Internal functions
//
static bool _gui_cmd_init()
{
	// Initialize the command system
	if (!cmd_init_local()) {
		return false;
	}
	
	// Register command handlers (get, set, rsp)
	(void) cmd_register_cmd_id(CMD_AMBIENT_CORRECT, cmd_handler_get_ambient_correct, cmd_handler_set_ambient_correct, cmd_handler_rsp_ambient_correct);
	(void) cmd_register_cmd_id(CMD_BACKLIGHT, cmd_handler_get_backlight, cmd_handler_set_backlight, cmd_handler_rsp_backlight);
	(void) cmd_register_cmd_id(CMD_SAVE_BACKLIGHT, NULL, cmd_handler_set_save_backlight, NULL);
	(void) cmd_register_cmd_id(CMD_BATT_LEVEL, cmd_handler_get_batt_level, NULL, cmd_handler_rsp_batt_info);
	(void) cmd_register_cmd_id(CMD_BRIGHTNESS, cmd_handler_get_brightness, cmd_handler_set_brightness, cmd_handler_rsp_brightness);
	(void) cmd_register_cmd_id(CMD_CRIT_BATT, NULL, cmd_handler_set_critical_batt, NULL);
	(void) cmd_register_cmd_id(CMD_CTRL_ACTIVITY, NULL, cmd_handler_set_ctrl_activity, cmd_handler_rsp_ctrl_activity);
	(void) cmd_register_cmd_id(CMD_CARD_PRESENT, cmd_handler_get_card_present, NULL, cmd_handler_rsp_card_present);
	(void) cmd_register_cmd_id(CMD_EMISSIVITY, cmd_handler_get_emissivity, cmd_handler_set_emissivity, cmd_handler_rsp_emissivity);
	(void) cmd_register_cmd_id(CMD_FILE_CATALOG, cmd_handler_get_file_catalog, NULL, cmd_handler_rsp_file_catalog);
	(void) cmd_register_cmd_id(CMD_FILE_DELETE, NULL, cmd_handler_set_file_delete, NULL);
	(void) cmd_register_cmd_id(CMD_FILE_GET_IMAGE, cmd_handler_get_file_image, NULL, cmd_handler_rsp_file_image);
	(void) cmd_register_cmd_id(CMD_FFC, NULL, cmd_handler_set_ffc, NULL);
	(void) cmd_register_cmd_id(CMD_GAIN, cmd_handler_get_gain, cmd_handler_set_gain, cmd_handler_rsp_gain);
	(void) cmd_register_cmd_id(CMD_IMAGE, NULL, cmd_handler_set_image, NULL);
	(void) cmd_register_cmd_id(CMD_MIN_MAX_EN, cmd_handler_get_min_max_enable, cmd_handler_set_min_max_enable, cmd_handler_rsp_min_max_en);
	(void) cmd_register_cmd_id(CMD_MSG_ON, NULL, cmd_handler_set_msg_on, NULL);
	(void) cmd_register_cmd_id(CMD_MSG_OFF, NULL, cmd_handler_set_msg_off, NULL);
	(void) cmd_register_cmd_id(CMD_ORIENTATION, NULL, cmd_handler_set_orientation, NULL);
	(void) cmd_register_cmd_id(CMD_PALETTE, cmd_handler_get_palette, cmd_handler_set_palette, cmd_handler_rsp_palette);
	(void) cmd_register_cmd_id(CMD_POWEROFF, NULL, cmd_handler_set_poweroff, NULL);
	(void) cmd_register_cmd_id(CMD_REGION_EN, cmd_handler_get_region_enable, cmd_handler_set_region_enable, cmd_handler_rsp_region_enable);
	(void) cmd_register_cmd_id(CMD_REGION_LOC, NULL, cmd_handler_set_region_location, NULL);
	(void) cmd_register_cmd_id(CMD_SAVE_OVL_EN, cmd_handler_get_save_ovl_en, cmd_handler_set_save_ovl_en, cmd_handler_rsp_save_ovl_en);
	(void) cmd_register_cmd_id(CMD_SHUTTER_INFO, cmd_handler_get_shutter, cmd_handler_set_shutter, cmd_handler_rsp_shutter);
	(void) cmd_register_cmd_id(CMD_SAVE_PALETTE, NULL, cmd_handler_set_save_palette, NULL);
	(void) cmd_register_cmd_id(CMD_SHUTDOWN, NULL, _cmd_handler_set_shutdown, NULL);
	(void) cmd_register_cmd_id(CMD_SPOT_EN, cmd_handler_get_spot_enable, cmd_handler_set_spot_enable, cmd_handler_rsp_spot_enable);
	(void) cmd_register_cmd_id(CMD_SPOT_LOC, NULL, cmd_handler_set_spot_location, NULL);
	(void) cmd_register_cmd_id(CMD_STREAM_EN, NULL, cmd_handler_set_stream_enable, NULL);
	(void) cmd_register_cmd_id(CMD_SYS_INFO, cmd_handler_get_sys_info, NULL, cmd_handler_rsp_sys_info);
	(void) cmd_register_cmd_id(CMD_TAKE_PICTURE, NULL, cmd_handler_set_take_picture, NULL);
	(void) cmd_register_cmd_id(CMD_TIME, cmd_handler_get_time, cmd_handler_set_time, cmd_handler_rsp_time);
	(void) cmd_register_cmd_id(CMD_TIMELAPSE_CFG, NULL, cmd_handler_set_timelapse_cfg, NULL);
	(void) cmd_register_cmd_id(CMD_TIMELAPSE_STATUS, NULL, cmd_handler_set_timelapse_status, NULL);
	(void) cmd_register_cmd_id(CMD_UNITS, cmd_handler_get_units, cmd_handler_set_units, cmd_handler_rsp_units);
	(void) cmd_register_cmd_id(CMD_WIFI_INFO, cmd_handler_get_wifi, cmd_handler_set_wifi, cmd_handler_rsp_wifi);
	
	return true;
}


static void _gui_send_image(int render_buf_index)
{
	uint8_t buf[4];
	
	t1c_buffer_t* t1cP = (render_buf_index == 0) ? &out_t1c_buffer[0] : &out_t1c_buffer[1];
	
	buf[0] = ((uint32_t) t1cP >> 24) & 0xFF;
	buf[1] = ((uint32_t) t1cP >> 16) & 0xFF;
	buf[2] = ((uint32_t) t1cP >> 8) & 0xFF;
	buf[3] = (uint32_t) t1cP & 0xFF;
	
	// Send the t1c_buffer pointer
	(void) cmd_send_binary(CMD_SET, CMD_IMAGE, 4, buf);
}


static void _gui_notification_handler()
{
	uint32_t notification_value;
	
	// Handle take pic
	
	// Look for incoming notifications (clear them upon reading)
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, GUI_NOTIFY_T1C_FRAME_MASK_1)) {
			if (cmd_handler_stream_enabled()) {
				_gui_send_image(0);
			}
		}
		
		if (Notification(notification_value, GUI_NOTIFY_T1C_FRAME_MASK_2)) {
			if (cmd_handler_stream_enabled()) {
				_gui_send_image(1);
			}
		}
		
		if (Notification(notification_value, GUI_NOTIFY_TAKE_PICTURE_MASK)) {
			xTaskNotify(task_handle_file, FILE_NOTIFY_SAVE_JPG_MASK, eSetBits);
		}
				
		if (Notification(notification_value, GUI_NOTIFY_CRIT_BATT_DET_MASK)) {
			(void) cmd_send(CMD_SET, CMD_CRIT_BATT);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_SHUTDOWN_MASK)) {
			(void) cmd_send(CMD_SET, CMD_SHUTDOWN);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_FW_UPD_EN_MASK)) {
			(void) cmd_send(CMD_SET, CMD_FW_UPD_EN);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_FW_UPD_END_MASK)) {
			(void) cmd_send(CMD_SET, CMD_FW_UPD_END);
		}

		if (Notification(notification_value, GUI_NOTIFY_FILE_MSG_ON_MASK)) {
			(void) cmd_send_string(CMD_SET, CMD_MSG_ON, file_get_file_save_status_string());
		}
		
		if (Notification(notification_value, GUI_NOTIFY_FILE_MSG_OFF_MASK)) {
			(void) cmd_send(CMD_SET, CMD_MSG_OFF);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_FILE_CATALOG_READY_MASK)) {
			(void) _gui_send_get_file_catalog_response();
		}
		
		if (Notification(notification_value, GUI_NOTIFY_FILE_IMAGE_READY_MASK)) {
			(void) _gui_send_get_file_image_response();
		}
		
		if (Notification(notification_value, GUI_NOTIFY_FILE_TIMELAPSE_ON_MASK)) {
			(void) cmd_send_int32(CMD_SET, CMD_TIMELAPSE_STATUS, 1);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_FILE_TIMELAPSE_OFF_MASK)) {
			(void) cmd_send_int32(CMD_SET, CMD_TIMELAPSE_STATUS, 0);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_CTRL_ACT_SUCCEEDED_MASK)) {
			(void) cmd_send_int32(CMD_RSP, CMD_CTRL_ACTIVITY, 1);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_CTRL_ACT_FAILED_MASK)) {
			(void) cmd_send_int32(CMD_RSP, CMD_CTRL_ACTIVITY, 0);
		}

#if (CONFIG_SCREENDUMP_ENABLE == true)
		if (Notification(notification_value, GUI_NOTIFY_SCREENDUMP_MASK)) {
			_gui_do_screendump();
		}
#endif
	}
}


static void _gui_lvgl_init()
{
	// Initialize lvgl
	lv_init();
	
	// Interface and driver initialization
	disp_driver_init(true);
	touch_driver_init();
	
	// Install the display driver
	lv_disp_buf_init(&lvgl_disp_buf, lvgl_disp_buf1, lvgl_disp_buf2, DISP_BUF_SIZE);
	lv_disp_drv_init(&lvgl_disp_drv);
	lvgl_disp_drv.flush_cb = disp_driver_flush;
	lvgl_disp_drv.buffer = &lvgl_disp_buf;
	lv_disp_drv_register(&lvgl_disp_drv);
	
	// Install the touchscreen driver
    lv_indev_drv_init(&lvgl_indev_drv);
    lvgl_indev_drv.read_cb = touch_driver_read;
    lvgl_indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&lvgl_indev_drv);
	
    // Hook LVGL's timebase to the CPU system tick so it can keep track of time
    esp_register_freertos_tick_hook(_lv_tick_callback);
}


// gui_task specific routine to send the catalog to our own response handler
static bool _gui_send_get_file_catalog_response()
{
	bool ret;
	char* catalog_list;
	int catalog_type;
	int num_entries;
	uint8_t buf[4];
	int16_t short_type;
	uint32_t t;
	
	// Get the catalog information from file_task
	catalog_list = file_get_catalog(&num_entries, &catalog_type);
	
	// First, send an integer with the type and number of entries packed in it
	//   Low 16 bits: type
	//   High 16 bits: number of entries
	short_type = (int16_t) catalog_type;
	t = ((uint16_t) short_type) | (((uint16_t) num_entries) << 16);
	ret = cmd_send_int32(CMD_RSP, CMD_FILE_CATALOG, (int32_t) t);
	
	// Then send the string pointer as an integer
	buf[0] = ((uint32_t) catalog_list >> 24) & 0xFF;
	buf[1] = ((uint32_t) catalog_list >> 16) & 0xFF;
	buf[2] = ((uint32_t) catalog_list >> 8) & 0xFF;
	buf[3] = (uint32_t) catalog_list & 0xFF;
	ret &= cmd_send_binary(CMD_RSP, CMD_FILE_CATALOG, 4, buf);
	
	return ret;
}


// gui_task specific routine to send an image to our own response handler (we only have to send a pointer)
static bool _gui_send_get_file_image_response()
{
	// We only have to send a notification since we are using a shared image buffer
	return cmd_send(CMD_RSP, CMD_FILE_GET_IMAGE);
}


// gui_task specific handling of shutdown command
static void _cmd_handler_set_shutdown(cmd_data_t data_type, uint32_t len, uint8_t* data)
{
	if ((data_type == CMD_DATA_NONE) && (len == 0)) {
		// Dummy for now since we don't ever expect the GUI part to shutdown on iCam
	}
}


static void _lv_tick_callback()
{
	lv_tick_inc(portTICK_PERIOD_MS);
}


#if (CONFIG_SCREENDUMP_ENABLE == true)
// This task blocks gui_task
void _gui_do_screendump()
{
	char line_buf[161];   // Large enough for 32 16-bit hex values with a space between them
	int i, j, n;
	int len = MEM_FB_W * MEM_FB_H;
	uint16_t* fb;
	
	// Configure the display driver to render to the screendump frame buffer
	disp_driver_en_dump(true);
	
	// Force LVGL to redraw the entire screen (to the screendump frame buffer)
	lv_obj_invalidate(lv_scr_act());
	lv_refr_now(lv_disp_get_default());
	
	// Reconfigure the driver back to the LCD
	disp_driver_en_dump(false);
	
	// Dump the fb
	fb = (uint16_t*) mem_fb_get_buffer();
	i = 0;
	while (i < len) {
		n = 0;
		for (j=0; j<32; j++) {
			sprintf(line_buf + n, "%x ", *fb++);
			n = strlen(line_buf);
		}
		i += j;
		printf("%s: FB: %s\n", TAG, line_buf);
		vTaskDelay(pdMS_TO_TICKS(20));
	}
}
#endif

#endif /* !CONFIG_BUILD_ICAM_MINI */
