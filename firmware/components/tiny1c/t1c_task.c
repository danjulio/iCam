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
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "data_rw.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "falcon_cmd.h"
#include "file_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "hal/spi_types.h"
#include "out_state_utilities.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "system_config.h"
#include "t1c_task.h"
#include "t1c_tau.h"
#include "tiny1c.h"
#include "vdcmd.h"
#include <String.h>

#ifdef CONFIG_BUILD_ICAM_MINI
	#include "ctrl_task.h"
	#include "video_task.h"
	#include "web_task.h"
#else
	#include "gui_task.h"
	#include "gcore_task.h"
#endif

//
// Tiny1C Task Private Constants
//

// Undefine to toggle diagnostic output during frame processing
//#define INCLUDE_T1C_DIAG_OUTPUT

// Undefine to display various parameters at startup
//#define INCLUDE_SHUTTER_DISPLAY
//#define INCLUDE_IMAGE_DISPLAY
#define INCLUDE_TPD_DISPLAY


// VOSPI interface
#define VOSPI_TX_DUMMY_LEN  (512)
#define VOSPI_ROW_LEN       (T1C_WIDTH*2)

// VOSPI header
#define VOSPI_HEADER_OFFSET (VOSPI_TX_DUMMY_LEN-32)
// Header indicies
#define HEADER_FRAME_VALID     0
#define HEADER_FRAME_INDEX_L   2
#define HEADER_FRAME_INDEX_H   3
#define HEADER_SHUTTER_STATE   4
#define HEADER_VTEMP_L         5
#define HEADER_VTEMP_H         6
#define HEADER_GAIN_STATE      9
#define HEADER_FREEZE_STATE    12


// Main loop evaluation period (uSec)
#define EVAL_USEC               (1000000/T1C_FPS)

// Pattern display period (mSec)
#define PATTERN_DISP_MSEC       2000

// CCI Access state
#define CCI_ACCESS_ST_IDLE      0
#define CCI_ACCESS_ST_REQ_SPOT  1
#define CCI_ACCESS_ST_WAIT_SPOT 2
#define CCI_ACCESS_ST_REQ_MM    3
#define CCI_ACCESS_ST_WAIT_MM   4
#define CCI_ACCESS_ST_REQ_REG   5
#define CCI_ACCESS_ST_WAIT_REG  6
#define CCI_ACCESS_ST_EX_CMD    7
#define CCI_ACCESS_ST_WAIT_CMD  8

// Parameter buffer types
#define PARAM_BUF_TYPE_SHUTTER  0
#define PARAM_BUF_TYPE_IMAGE    1
#define PARAM_BUF_TYPE_TPD      2

// Number of parameters we keep local copies of for each type (based on falcon_cmd.h valid enums)
#define PARAM_NUM_TYPE_SHUTTER  (SHUTTER_CHANGE_GAIN_2ND_DELAY+1)
#define PARAM_NUM_TYPE_IMAGE    (IMAGE_PROP_SEL_MIRROR_FLIP+1)
#define PARAM_NUM_TYPE_TPD      (TPD_PROP_GAIN_SEL+1)

// Calibration activities (for _t1c_perform_cal())
#define CAL_1_PT                0
#define CAL_2_PT_L              1
#define CAL_2_PT_H              2


// Environmental conditions entry
typedef struct {
	bool ambient_temp_valid;
	bool ambient_humidity_valid;
	bool target_distance_valid;
	int16_t ambient_temp;                  // Degrees C
	uint16_t ambient_humidity;             // Percent 0 - 100
	uint16_t target_distance;              // cm
} environmental_conditions_t;


// Param buffer entry
typedef struct {
	uint8_t type;
	uint8_t param;
	uint16_t value;
} param_buffer_entry_t;
#define PARAM_BUFFER_LEN       (16*(8+sizeof(param_buffer_entry_t)))


// Preview setups
static const PreviewStartParam_t stream_param = {
  PREVIEW_PATH0, /* Path */
  0, /* 0: Camera, 1: Fixed pattern */
  T1C_WIDTH,
  T1C_HEIGHT,
  T1C_FPS,
  8 /* SPI Interface */
};



//
// Tiny1C Task Variables
//
static const char* TAG = "t1c_task";

// Task ready for other tasks to command it
static bool t1c_task_running = false;

// SPI Interface
static spi_device_handle_t spi;
static spi_transaction_t spi_trans;

// SPI DMA buffers allocated in on-board memory
static uint8_t vospi_TxBuf[VOSPI_TX_DUMMY_LEN];
static uint8_t vospi_RxBuf1[VOSPI_TX_DUMMY_LEN];
static uint8_t vospi_RxBuf2[VOSPI_TX_DUMMY_LEN];

// Per-image data from vospi header
static bool frame_high_gain;
static bool frame_pix_freeze;

// Image processing
static uint16_t y16_min;
static uint16_t y16_max;

// Preview mode data inversion
static bool invert_y16_data = false;

// CCI Access 
static int cci_state = CCI_ACCESS_ST_IDLE;
static uint16_t shutter_settings_values[PARAM_NUM_TYPE_SHUTTER] = { 0 };
static uint16_t image_settings_values[PARAM_NUM_TYPE_IMAGE] = { 0 };
static uint16_t tpd_settings_values[PARAM_NUM_TYPE_TPD] = { 0 };

// Image metadata handling
//
// Environmental (ambient) conditions
static environmental_conditions_t env_cond = { 0 };
static environmental_conditions_t new_env_cond = { 0 };

// Image min/max temp
static MaxMinTempInfo_t max_min_temp_data;

// PS Config used to initially configure the Tiny1C
static t1c_config_t t1c_config;

// Parameter setting buffer
static RingbufHandle_t param_buf_handle;
static SemaphoreHandle_t param_mutex;

// Min/max temp
static bool minmax_en = false;
static bool minmax_valid = false;

// Spot temp
static bool spot_en = false;
static bool spot_valid = false;
static uint16_t spot_temp_raw;
static IrPoint_t spot_param = {0, 0};
static IrPoint_t spot_new_param;

// Region temp
static bool region_en = false;
static bool region_valid = false;
static TpdLineRectTempInfo_t region_temp_info;
static IrRect_t region_param = {{0, 0}, {10, 10}};
static IrRect_t region_new_param;

// File task related
static bool notify_get_file_image = false;

// Mode dependent notification variables
static TaskHandle_t platform_task;
static TaskHandle_t output_task;
static uint32_t task_frame_1_notification;
static uint32_t task_frame_2_notification;
static uint32_t task_shutdown_notification;
static uint32_t task_ctrl_act_succeeded_notification;
static uint32_t task_ctrl_act_failed_notification;

// Calibration related
static bool cal_2pt_in_progress = false;        // Prevents TPD updates between L and H points
static uint16_t bb_temp_k;                      // Calibration blackbody temperature (°K)

// Tiny1C info (buffer size dictated by Tiny1C Info command length)
static char t1c_version_buf[64];
static char t1c_sn_buf[64];



//
// Tiny1C Task Forward declarations for internal functions
//
static bool _t1c_init_spi();
static bool _t1c_init_cci();
static bool _t1c_init_param_buffer();
static bool _t1c_read_params(int type);
static ir_error_t _t1c_restore_default_config();
static ir_error_t _t1c_perform_cal(int type);
static bool _set_y16_mode(enum y16_isp_stream_src_types n);
static void _get_frame();
static void _process_y16_line(uint16_t* src, uint16_t* dst, int len);
static void _push_frame(t1c_buffer_t* buf);
static void _push_metadata();
static void _handle_notifications();
static void _update_tpd_params(bool force_update);
static void _eval_cci();
static void _cci_send_get_point_temp();
static void _cci_set_get_min_max_temp();
static void _cci_set_get_region_temp();
static void _cci_write_param(uint8_t sub_cmd, uint8_t param, uint16_t value);
static void _cci_fast_set_param(param_buffer_entry_t* buf_entryP);
#ifdef INCLUDE_SHUTTER_DISPLAY
static void _display_shutter_values();
#endif
#ifdef INCLUDE_IMAGE_DISPLAY
static void _display_image_values();
#endif
#ifdef INCLUDE_TPD_DISPLAY
static void _display_tpd_values();
#endif



//
// Tiny1C Task API
//
void t1c_task()
{
	int vid_buf_index = 0;     // 0 or 1 for ping-pong
	int64_t cur_usec;
	int64_t prev_usec = 0;
	
	// Create the parameter setting queue (buffer) to allow other tasks to configure the Tiny1C
	if (!_t1c_init_param_buffer()) {
		ESP_LOGE(TAG, "Could not initialize parameter buffer");
#ifdef CONFIG_BUILD_ICAM_MINI
		ctrl_set_fault_type(CTRL_FAULT_MEM_INIT);
#else
		// ???
#endif
		vTaskDelete(NULL);
	}
	
	// Delay to allow Tiny1C to boot after reset
	vTaskDelay(pdMS_TO_TICKS(1000));
	
	ESP_LOGI(TAG, "Start task");
	
	// Get our initial configuration
	(void) ps_get_config(PS_CONFIG_TYPE_T1C, &t1c_config);
	
	// Attempt to initialize our SPI interface for VOSPI communication
	if (!_t1c_init_spi()) {
		ESP_LOGE(TAG, "Could not initialize Tiny1C VOSPI");
#ifdef CONFIG_BUILD_ICAM_MINI
		ctrl_set_fault_type(CTRL_FAULT_T1C_CCI);
#else
		// ???
#endif
		vTaskDelete(NULL);
	}
	
	// Attempt to initialize the Tiny1C and start it streaming
	if (!_t1c_init_cci()) {
		ESP_LOGE(TAG, "Could not initialize Tiny1C CCI");
#ifdef CONFIG_BUILD_ICAM_MINI
		ctrl_set_fault_type(CTRL_FAULT_T1C_VOSPI);
#else
		// ???
#endif
		vTaskDelete(NULL);
	}
	
	// Initial configuration of GUI related items
	t1c_set_spot_location(T1C_WIDTH/2, T1C_HEIGHT/2);
	t1c_set_spot_enable(out_state.spotmeter_enable);
	t1c_set_minmax_temp_enable(out_state.min_max_tmp_enable);
	t1c_set_minmax_marker_enable(out_state.min_max_mrk_enable);
	t1c_set_region_location(T1C_WIDTH/4, T1C_HEIGHT/4, 3*T1C_WIDTH/4, 3*T1C_HEIGHT/4);
	t1c_set_region_enable(out_state.region_enable);
	
	// Setup our notifications
#ifdef CONFIG_BUILD_ICAM_MINI
	platform_task = task_handle_ctrl;
	if (ctrl_get_output_mode() == CTRL_OUTPUT_VID) {
		output_task = task_handle_vid;
		task_frame_1_notification = VID_NOTIFY_T1C_FRAME_MASK_1;
		task_frame_2_notification = VID_NOTIFY_T1C_FRAME_MASK_2;
	} else {
		output_task = task_handle_web;
		task_frame_1_notification = WEB_NOTIFY_T1C_FRAME_MASK_1;
		task_frame_2_notification = WEB_NOTIFY_T1C_FRAME_MASK_2;
	}
	task_shutdown_notification = CTRL_NOTIFY_SHUTDOWN;
	task_ctrl_act_succeeded_notification = WEB_NOTIFY_CTRL_ACT_SUCCEEDED_MASK;
	task_ctrl_act_failed_notification = WEB_NOTIFY_CTRL_ACT_FAILED_MASK;
#else
	platform_task = task_handle_gcore;
	output_task = task_handle_gui;
	task_frame_1_notification = GUI_NOTIFY_T1C_FRAME_MASK_1;
	task_frame_2_notification = GUI_NOTIFY_T1C_FRAME_MASK_2;
	task_shutdown_notification = GCORE_NOTIFY_SHUTOFF_MASK;
	task_ctrl_act_succeeded_notification = GUI_NOTIFY_CTRL_ACT_SUCCEEDED_MASK;
	task_ctrl_act_failed_notification = GUI_NOTIFY_CTRL_ACT_FAILED_MASK;
#endif

	t1c_task_running = true;
	
	cur_usec = esp_timer_get_time();
	
	// Process frames
	while (1) {
		// Spin until time to get a frame
		while ((cur_usec - prev_usec) < EVAL_USEC) {
			// Tickle the task WDT
			vTaskDelay(pdMS_TO_TICKS(1));
			cur_usec = esp_timer_get_time();
		}
		prev_usec = cur_usec;
		
#ifdef INCLUDE_T1C_DIAG_OUTPUT
		gpio_set_level(BRD_DIAG_IO, 1);
#endif
		// Process any incoming notifications
		_handle_notifications();
		
		// Get an image from the Tiny1C
		_get_frame();  
		
		// Send to our output task
		if (vid_buf_index == 0) {
			_push_frame(&out_t1c_buffer[0]);
			xTaskNotify(output_task, task_frame_1_notification, eSetBits);
			vid_buf_index = 1;
		} else {
			_push_frame(&out_t1c_buffer[1]);
			xTaskNotify(output_task, task_frame_2_notification, eSetBits);
			vid_buf_index = 0;
		}
		
		// Send to file_task if requested
		if (notify_get_file_image) {
			_push_frame(&file_t1c_buffer);
			_push_metadata();
			xTaskNotify(task_handle_file, FILE_NOTIFY_T1C_FRAME_MASK, eSetBits);
			notify_get_file_image = false;
		}
		
		_eval_cci();
		
#ifdef INCLUDE_T1C_DIAG_OUTPUT
		gpio_set_level(BRD_DIAG_IO, 0);
#endif
	}
	ESP_LOGI(TAG, "All done");
	vTaskDelete(NULL);
}


bool t1c_task_ready()
{
	return t1c_task_running;
}


void t1c_set_spot_enable(bool en)
{
	spot_en = en;
	spot_valid = false;
}


void t1c_set_spot_location(uint16_t x, uint16_t y)
{
	spot_new_param.x = x;
	spot_new_param.y = y;
	
	// Notify ourselves so we can atomically set these values internally
	xTaskNotify(task_handle_t1c, T1C_NOTIFY_SET_SPOT_LOC_MASK, eSetBits);
}


void t1c_set_minmax_marker_enable(bool en)
{
	// Always make sure we output minmax if enabled (but leave disabling to t1c_set_minmax_temp_enable)
	if (en) minmax_en = true;
}


void t1c_set_minmax_temp_enable(bool en)
{
	minmax_en = en;
	minmax_valid = false;
}


void t1c_set_region_enable(bool en)
{
	region_en = en;
	region_valid = false;
}


void t1c_set_region_location(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	region_new_param.start_point.x = x1;
	region_new_param.start_point.y = y1;
	region_new_param.end_point.x = x2;
	region_new_param.end_point.y = y2;
	
	// Notify ourselves so we can atomically set these values internally
	xTaskNotify(task_handle_t1c, T1C_NOTIFY_SET_REGION_LOC_MASK, eSetBits);
}


void t1c_set_ambient_temp(int16_t t, bool valid)
{
	new_env_cond.ambient_temp = t;
	new_env_cond.ambient_temp_valid = valid;
}


void t1c_set_ambient_humidity(uint16_t h, bool valid)
{
	new_env_cond.ambient_humidity = h;
	new_env_cond.ambient_humidity_valid = valid;
}


void t1c_set_target_distance(uint16_t cm, bool valid)
{
	new_env_cond.target_distance = cm;
	new_env_cond.target_distance_valid = valid;
}


bool t1c_set_param_shutter(uint8_t param, uint16_t param_value)
{
	param_buffer_entry_t param_entry;
	UBaseType_t ret;
	
	param_entry.type = PARAM_BUF_TYPE_SHUTTER;
	param_entry.param = param;
	param_entry.value = param_value;
	
	xSemaphoreTake(param_mutex, portMAX_DELAY);
	ret = xRingbufferSend(param_buf_handle, &param_entry, sizeof(param_entry), pdMS_TO_TICKS(1000));
	xSemaphoreGive(param_mutex);
	
	return (ret == pdTRUE);
}


bool t1c_set_param_image(uint8_t param, uint16_t param_value)
{
	param_buffer_entry_t param_entry;
	UBaseType_t ret;
	
	param_entry.type = PARAM_BUF_TYPE_IMAGE;
	param_entry.param = param;
	param_entry.value = param_value;
	
	xSemaphoreTake(param_mutex, portMAX_DELAY);
	ret = xRingbufferSend(param_buf_handle, &param_entry, sizeof(param_entry), pdMS_TO_TICKS(1000));
	xSemaphoreGive(param_mutex);
	
	return (ret == pdTRUE);
}


bool t1c_set_param_tpd(uint8_t param, uint16_t param_value)
{
	param_buffer_entry_t param_entry;
	UBaseType_t ret;
	
	param_entry.type = PARAM_BUF_TYPE_TPD;
	param_entry.param = param;
	param_entry.value = param_value;

	xSemaphoreTake(param_mutex, portMAX_DELAY);
	ret = xRingbufferSend(param_buf_handle, &param_entry, sizeof(param_entry), pdMS_TO_TICKS(1000));
	xSemaphoreGive(param_mutex);
	
	return (ret == pdTRUE);
}


void t1c_set_blackbody_temp(uint16_t temp_k)
{
	bb_temp_k = temp_k;
}


char* t1c_get_module_version()
{
	return t1c_version_buf;
}


char* t1c_get_module_sn()
{
	return t1c_sn_buf;
}




//
// Tiny1C Task internal functions
//
static bool _t1c_init_spi()
{
	// Attempt to initialize the SPI Master used by t1c_task to read the Tiny1C
	spi_bus_config_t spi_buscfg = {
		.miso_io_num=BRD_T1C_MISO_IO,
		.mosi_io_num=BRD_T1C_MOSI_IO,
		.sclk_io_num=BRD_T1C_SCK_IO,
		.max_transfer_sz=VOSPI_TX_DUMMY_LEN,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1
	};
	
	spi_device_interface_config_t devcfg = {
		.command_bits = 0,
		.address_bits = 0,
		.clock_speed_hz = T1C_SPI_FREQ_HZ,
		.mode = 3,
		.spics_io_num = BRD_T1C_CSN_IO,
		.queue_size = 1,
		.flags = 0,
		.cs_ena_pretrans = 2
	};

	if (spi_bus_initialize(T1C_SPI_HOST, &spi_buscfg, T1C_DMA_NUM) != ESP_OK) {
		ESP_LOGE(TAG, "SPI Master initialization failed");
		return false;
	}
	
	if (spi_bus_add_device(T1C_SPI_HOST, &devcfg, &spi) != ESP_OK) {
		ESP_LOGE(TAG, "Could not add SPI device");
		return false;
	}
	
	// Initialize the SPI transaction we use
	memset(&spi_trans, 0, sizeof(spi_transaction_t));
	spi_trans.tx_buffer = vospi_TxBuf;
	spi_trans.rx_buffer = vospi_RxBuf1;
	
	return true;
}


static bool _t1c_init_cci()
{
	uint16_t param_value;
	
	// Read the TAU table
	if (read_correct_table(t1c_config.high_gain ? HIGH_GAIN : LOW_GAIN) != 0) {
		return false;
	}
	
	// Initialize the Tiny1C interface
	if (vdcmd_init_by_type(VDCMD_I2C_VDCMD) != IR_SUCCESS) {
		ESP_LOGE(TAG, "vdcmd_init_by_type failed");
		return false;
	}
	
	// Get the firmware version
	if (get_device_info(DEV_INFO_FW_BUILD_VERSION_INFO, (uint8_t*) &t1c_version_buf[0]) == IR_SUCCESS) {
		ESP_LOGI(TAG, "Tiny1C Firmware version: %s", t1c_version_buf);
	} else {
		ESP_LOGE(TAG, "Could not read firmware version");
	}
	
	// Get the camera serial number
	if (get_device_info(DEV_INFO_GET_SN, (uint8_t*) &t1c_sn_buf[0]) == IR_SUCCESS) {
		ESP_LOGI(TAG, "Tiny1C Serial Number: %s", t1c_sn_buf);
	} else {
		ESP_LOGE(TAG, "Could not read serial number");
	}
	
	// Start the Tiny1C video stream
	if (preview_start((PreviewStartParam_t*)&stream_param) != IR_SUCCESS) {
		ESP_LOGE(TAG, "preview_start failed");
		return false;
	}
	
	// Configure the output for Y16 data output
	//   Set the output from the GAMMA process since that utilizes the entire video processing
	//   chain and looks best with a linear transformation to 8-bits
	if (!_set_y16_mode(Y16_MODE_GAMMA)) {
		ESP_LOGE(TAG, "set_y16_mode failed");
		return false;
	}
	
	// Get initial (power-on) parameter configuration
	if (!_t1c_read_params(PARAM_BUF_TYPE_SHUTTER)) return false;
	if (!_t1c_read_params(PARAM_BUF_TYPE_IMAGE)) return false;
	if (!_t1c_read_params(PARAM_BUF_TYPE_TPD)) return false;
/*	
#ifdef INCLUDE_SHUTTER_DISPLAY
	_display_shutter_values();
#endif
#ifdef INCLUDE_IMAGE_DISPLAY
	_display_image_values();
#endif
#ifdef INCLUDE_TPD_DISPLAY
	_display_tpd_values();
#endif
*/

	// Configure the Tiny1C with our settings
	//

	// Set Auto Shutter enable
	param_value = t1c_config.auto_ffc_en ? 1 : 0;
	if (set_prop_auto_shutter_params(SHUTTER_PROP_SWITCH, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize auto shutter enable failed");
		return false;
	}
	shutter_settings_values[SHUTTER_PROP_SWITCH] = param_value;
	
	// Set Auto shutter minimum interval
	param_value = (uint16_t) t1c_config.min_ffc_interval;
	if (set_prop_auto_shutter_params(SHUTTER_PROP_MIN_INTERVAL, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize auto shutter min interval failed");
		return false;
	}
	shutter_settings_values[SHUTTER_PROP_MIN_INTERVAL] = param_value;
	
	// Set Auto shutter maximum interval
	param_value = (uint16_t) t1c_config.max_ffc_interval;
	if (set_prop_auto_shutter_params(SHUTTER_PROP_MAX_INTERVAL, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize auto shutter max interval failed");
		return false;
	}
	shutter_settings_values[SHUTTER_PROP_MAX_INTERVAL] = param_value;
	
	// Set Auto shutter temp threshold
	param_value = (uint16_t) (t1c_config.ffc_temp_threshold_x10 * 36 / 10);
	if (set_prop_auto_shutter_params(SHUTTER_PROP_TEMP_THRESHOLD_B, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize auto shutter temp threshold failed");
		return false;
	}
	shutter_settings_values[SHUTTER_PROP_TEMP_THRESHOLD_B] = param_value;
	
	// Set Manual shutter minimum interval
	param_value = (uint16_t) t1c_config.min_ffc_interval;
	if (set_prop_auto_shutter_params(SHUTTER_PROP_ANY_INTERVAL, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize auto shutter any interval failed");
		return false;
	}
	shutter_settings_values[SHUTTER_PROP_ANY_INTERVAL] = param_value;
	
	// Shutter timing on preview start
	param_value = T1C_SHUTTER_PREVIEW_1_SECS;
	if (set_prop_auto_shutter_params(SHUTTER_PREVIEW_START_1ST_DELAY, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize auto shutter preview 1 failed");
		return false;
	}
	shutter_settings_values[SHUTTER_PREVIEW_START_1ST_DELAY] = param_value;
	
	param_value = T1C_SHUTTER_PREVIEW_2_SECS;
	if (set_prop_auto_shutter_params(SHUTTER_PREVIEW_START_2ND_DELAY, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize auto shutter preview 2 failed");
		return false;
	}
	shutter_settings_values[SHUTTER_PREVIEW_START_2ND_DELAY] = param_value;
	
	// Shutter timing on gain change
	param_value = T1C_SHUTTER_GAIN_CHG_1_SECS;
	if (set_prop_auto_shutter_params(SHUTTER_CHANGE_GAIN_1ST_DELAY, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize auto shutter gain 1 failed");
		return false;
	}
	shutter_settings_values[SHUTTER_CHANGE_GAIN_1ST_DELAY] = param_value;
	
	param_value = T1C_SHUTTER_GAIN_CHG_2_SECS;
	if (set_prop_auto_shutter_params(SHUTTER_CHANGE_GAIN_2ND_DELAY, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize auto shutter gain 2 failed");
		return false;
	}
	shutter_settings_values[SHUTTER_CHANGE_GAIN_2ND_DELAY] = param_value;
	
	// Set the default brightness
	param_value = brightness_to_param_value(t1c_config.brightness);
	if (set_prop_image_params(IMAGE_PROP_LEVEL_BRIGHTNESS, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize brightness failed");
		return false;
	}
	image_settings_values[IMAGE_PROP_LEVEL_BRIGHTNESS] = param_value;
	
	// Setup the default distance
	param_value = dist_cm_to_param_value(t1c_config.distance);
	if (set_prop_tpd_params(TPD_PROP_DISTANCE, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize distance failed");
		return false;
	}
	tpd_settings_values[TPD_PROP_DISTANCE] = param_value;
	
	// Set the default emissivity
	param_value = emissivity_to_param_value(t1c_config.emissivity);
	if (set_prop_tpd_params(TPD_PROP_EMS, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize emissivity failed");
		return false;
	}
	tpd_settings_values[TPD_PROP_EMS] = param_value;
	
	// Set the default atmospheric temp
	param_value = temperature_to_param_value(t1c_config.atmospheric_temp);
	if (set_prop_tpd_params(TPD_PROP_TA, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize atmospheric temp failed");
		return false;
	}
	tpd_settings_values[TPD_PROP_TA] = param_value;
	
	// Set the default reflective temp
	param_value = temperature_to_param_value(t1c_config.reflected_temp);
	if (set_prop_tpd_params(TPD_PROP_TU, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize reflective temp failed");
		return false;
	}
	tpd_settings_values[TPD_PROP_TU] = param_value;
	
	// Set the default TAU (ignore humidity for now)
	param_value = estimate_tau((float) t1c_config.atmospheric_temp, (float) t1c_config.distance/100.0, 0.0);
	if (set_prop_tpd_params(TPD_PROP_TAU, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize TAU failed");
		return false;
	}
	tpd_settings_values[TPD_PROP_TAU] = param_value;
	
	// Set gain mode
	param_value = t1c_config.high_gain ? 1 : 0;
	if (set_prop_tpd_params(TPD_PROP_GAIN_SEL, param_value) != IR_SUCCESS) {
		ESP_LOGE(TAG, "Initialize gain %u failed", param_value);
		return false;
	}
	tpd_settings_values[TPD_PROP_GAIN_SEL] = param_value;
	
#ifdef INCLUDE_SHUTTER_DISPLAY
	_display_shutter_values();
#endif
#ifdef INCLUDE_IMAGE_DISPLAY
	_display_image_values();
#endif
#ifdef INCLUDE_TPD_DISPLAY
	_display_tpd_values();
#endif

	return true;
}

static ir_error_t _t1c_restore_default_config()
{
	ir_error_t ret;
	
	ESP_LOGI(TAG, "Restore default values");
	
	// Restore factor default values
	ret = restore_default_cfg(DEF_CFG_ALL);
/*	
	// Save to internal flash
	if (ret == IR_SUCCESS) {
		ret = spi_config_save(SPI_MOD_CFG_ALL);
	}
*/	
	return ret;
}


static ir_error_t _t1c_perform_cal(int type)
{
	ir_error_t ret = IR_SUCCESS;
	
	ESP_LOGI(TAG, "Starting calibration type %d, blackbody temp = %u °K", type, bb_temp_k);
	
	// Restore default configuration
	if ((type == CAL_1_PT) || (type == CAL_2_PT_L)) {
		ESP_LOGI(TAG, "  restore cfg");
		ret = restore_default_cfg(DEF_CFG_TPD);
	}
	
#ifdef INCLUDE_TPD_DISPLAY
	_display_tpd_values();
#endif
	
	// Prevent main loop TPD updates from occurring between the L and H settings for 2 point cal
	cal_2pt_in_progress = (type == CAL_2_PT_L);
	
	// Disable automatic FFC
	if ((ret == IR_SUCCESS) && ((type == CAL_1_PT) || (type == CAL_2_PT_L))) {
		ESP_LOGI(TAG, "  shutter 0");
		ret = set_prop_auto_shutter_params(SHUTTER_PROP_SWITCH, 0);
	}
	
	// Update B value (FFC)
	if (ret == IR_SUCCESS) {
		ESP_LOGI(TAG, "  b update");
		ret = ooc_b_update(B_UPDATE);
	}
	
	// Collect blackbody data and set new calibration value
	if (ret == IR_SUCCESS) {
		ESP_LOGI(TAG, "  recal");
		if (type == CAL_1_PT) {
			ret = tpd_ktbt_recal_1point(bb_temp_k);
		} else if (type == CAL_2_PT_L) {
			ret = tpd_ktbt_recal_2point(TPD_KTBT_RECAL_P1, bb_temp_k);
		} else if (type == CAL_2_PT_H) {
			ret = tpd_ktbt_recal_2point(TPD_KTBT_RECAL_P2, bb_temp_k);
		}
		
		// Attempt to restore default values if calibration fails
		if (ret != IR_SUCCESS) {
			ESP_LOGI(TAG, "  restore");
			(void) restore_default_cfg(DEF_CFG_TPD);
		}
	}
	
	// Restart automatic FFC when done (even if failure)
	if ((ret == IR_SUCCESS) && ((type == CAL_1_PT) || (type == CAL_2_PT_H))) {
		ESP_LOGI(TAG, "  shutter 1");
		if (ret == IR_SUCCESS) {
			ret = set_prop_auto_shutter_params(SHUTTER_PROP_SWITCH, 1);
		} else {
			(void) set_prop_auto_shutter_params(SHUTTER_PROP_SWITCH, 1);
		}
	}
	
	// Save the TPD parameters after a successful cal
	/*
	if ((ret == IR_SUCCESS) && ((type == CAL_1_PT) || (type == CAL_2_PT_H))) {
		ESP_LOGI(TAG, "  save");
		ret = spi_config_save(SPI_MOD_CFG_ALL);
	}*/
	
	return ret;
}


static bool _t1c_init_param_buffer()
{
	param_buf_handle = xRingbufferCreate(PARAM_BUFFER_LEN, RINGBUF_TYPE_NOSPLIT);
	
	param_mutex = xSemaphoreCreateMutex();
	
	return (param_buf_handle != NULL) && (param_mutex != NULL);
}


static bool _t1c_read_params(int type)
{
	int n;
	uint16_t read_value;
	
	switch (type) {
		case PARAM_BUF_TYPE_SHUTTER:
			n = PARAM_NUM_TYPE_SHUTTER;
			break;
		case PARAM_BUF_TYPE_IMAGE:
			n =  PARAM_NUM_TYPE_IMAGE;
			break;
		default:
			n = PARAM_NUM_TYPE_TPD;
	}
	
	for (int i=0; i<n; i++) {
		switch (type) {
			case PARAM_BUF_TYPE_SHUTTER:
				if (get_prop_auto_shutter_params(SHUTTER_PROP_SWITCH + i, &read_value) != IR_SUCCESS) {
					ESP_LOGE(TAG, "Get Shutter Param %d failed", i);
					return false;
				} else {
					shutter_settings_values[i] = read_value;
				}
				break;
			
			case PARAM_BUF_TYPE_IMAGE:
				if (get_prop_image_params(IMAGE_PROP_LEVEL_TNR + i, &read_value) != IR_SUCCESS) {
					ESP_LOGE(TAG, "Get Image Param %d failed", i);
					return false;
				} else {
					image_settings_values[i] = read_value;
				}
				break;
			
			default:
				if (get_prop_tpd_params(TPD_PROP_DISTANCE + i, &read_value) != IR_SUCCESS) {
					ESP_LOGE(TAG, "Get TPD Param %d failed", i);
					return false;
				} else {
					tpd_settings_values[i] = read_value;
				}
		}
	}
	
	return true;
}


static bool _set_y16_mode(enum y16_isp_stream_src_types n)
{
	if (y16_preview_start(PREVIEW_PATH0, n) != IR_SUCCESS) {
		ESP_LOGE(TAG, "y16_preview_start failed");
		return false;
	}

  if ((n == Y16_MODE_TEMPERATURE) || (n == Y16_MODE_GAMMA)) {
    // Data does not need to be inverted for RAD (TEMP) or GAMMA modes
    invert_y16_data = false;
  } else {
    // Data needs to be inverted for all other modes
    invert_y16_data = true;
  }
  
  return true;
}


static void _get_frame()
{
	int row = 0;
	uint8_t* hdrP;
	uint16_t* bufP;
	
	y16_min = 0xFFFF;
	y16_max = 0;
	
	// Start acquiring frame - read dummy + header data
	vospi_TxBuf[0]= 0xAA;
	spi_trans.length = VOSPI_TX_DUMMY_LEN*8;
	spi_trans.rxlength = spi_trans.length;
	spi_trans.rx_buffer = vospi_RxBuf1;
	(void) spi_device_polling_transmit(spi, &spi_trans);
	
	// Parse some header data
	hdrP = &vospi_RxBuf1[VOSPI_HEADER_OFFSET];
	frame_high_gain = *(hdrP + HEADER_GAIN_STATE) == 0 ? false : true;
	frame_pix_freeze = *(hdrP + HEADER_FREEZE_STATE) == 0 ? false : true;
	
    // Read a frame into the image buffer
    bufP = t1c_y16_buffer;
    vospi_TxBuf[0]= 0x55;
    spi_trans.length = VOSPI_ROW_LEN*8;
    spi_trans.rxlength = spi_trans.length;
    spi_trans.rx_buffer = ((row % 2) == 0) ? vospi_RxBuf1 : vospi_RxBuf2;
    (void) spi_device_polling_start(spi, &spi_trans, portMAX_DELAY);  // Prime pipeline
    while (row<T1C_HEIGHT) {
    	// Wait for previous transfer
		(void) spi_device_polling_end(spi, portMAX_DELAY);
		
		if (row < (T1C_HEIGHT-1)) {
			// Start next transfer on all but last row
			spi_trans.rx_buffer = (((row+1) % 2) == 0) ? vospi_RxBuf1 : vospi_RxBuf2;
			(void) spi_device_polling_start(spi, &spi_trans, portMAX_DELAY);
		}
		
		// Copy the SPI buffer to the image buffer and update min/max
		_process_y16_line((uint16_t*) (((row % 2) == 0) ? vospi_RxBuf1 : vospi_RxBuf2), bufP, T1C_WIDTH);
		bufP += T1C_WIDTH;
		row += 1;
    }
}


static void _process_y16_line(uint16_t* src, uint16_t* dst, int len)
{
	uint16_t v;
	
	while (len--) {
		v = invert_y16_data ? ~(*src++) : *src++;
		if (v < y16_min) {
			y16_min = v;
		}
		if (v > y16_max) {
			y16_max = v;
		}
		*dst++ = v;
	}
}


static void _push_frame(t1c_buffer_t* buf)
{
	uint16_t* imgP;
	uint16_t* imgEndP;
	uint16_t* frameP;
	
	// Lock data structure
	xSemaphoreTake(buf->mutex, portMAX_DELAY);
	
	// Save the current header info
	buf->high_gain = frame_high_gain;
	buf->vid_frozen = frame_pix_freeze;
	
	// Copy the image data
	imgP = buf->img_data;
	imgEndP = imgP + T1C_WIDTH*T1C_HEIGHT;
	frameP = t1c_y16_buffer;
	while (imgP < imgEndP) {
		*imgP++ = *frameP++;
	}
	
	// Copy the raw min/max values for scaling
	buf->y16_min = y16_min;
	buf->y16_max = y16_max;
	
	// Add environmental conditions
	buf->amb_temp_valid = env_cond.ambient_temp_valid;
	buf->amb_temp = env_cond.ambient_temp;
	buf->amb_hum_valid = env_cond.ambient_humidity_valid;
	buf->amb_hum = env_cond.ambient_humidity;
	buf->distance_valid = env_cond.target_distance_valid;
	buf->distance = env_cond.target_distance;
	
	// Copy temperature metadata
	buf->minmax_valid = minmax_valid;
	buf->max_min_temp_info = max_min_temp_data;
	
	buf->spot_valid = spot_valid;
	buf->spot_point = spot_param;
	buf->spot_temp = spot_temp_raw;
	
	buf->region_valid = region_valid;
	buf->region_temp_info = region_temp_info;
	buf->region_points = region_param;
	
	// Unlock data structure
	xSemaphoreGive(buf->mutex);
	
}


static void _push_metadata()
{
	int i;
	
	for (i=0; i<PARAM_NUM_TYPE_IMAGE; i++) {
		file_t1c_meta.image_params[i] = image_settings_values[i];
	}
	
	for (i=0; i<PARAM_NUM_TYPE_TPD; i++) {
		file_t1c_meta.tpd_params[i] = tpd_settings_values[i];
	}
}


static void _handle_notifications()
{
	uint32_t notification_value;
	
	// Look for incoming notifications (clear them upon reading)
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, T1C_NOTIFY_SET_SPOT_LOC_MASK)) {
			spot_param = spot_new_param;
		}
		
		if (Notification(notification_value, T1C_NOTIFY_SET_REGION_LOC_MASK)) {
			region_param = region_new_param;
		}
		
		if (Notification(notification_value, T1C_NOTIFY_RESTORE_DEFAULT_MASK)) {
			ir_error_t ret = _t1c_restore_default_config();
			if (ret != IR_SUCCESS) {
				ESP_LOGE(TAG, "Restore default failed with %d", ret);
				xTaskNotify(output_task, task_ctrl_act_failed_notification, eSetBits);
			} else {
				xTaskNotify(output_task, task_ctrl_act_succeeded_notification, eSetBits);
			}
			
			// Delay to let the GUI display the result
			vTaskDelay(pdMS_TO_TICKS(1500));
			
			// This command always shuts down the system when complete
			xTaskNotify(platform_task, task_shutdown_notification, eSetBits);
		}
		
		if (Notification(notification_value, T1C_NOTIFY_CAL_1_MASK)) {
			ir_error_t ret = _t1c_perform_cal(CAL_1_PT);
			if (ret != IR_SUCCESS) {
				ESP_LOGE(TAG, "One point calibration failed with %d", ret);
				xTaskNotify(output_task, task_ctrl_act_failed_notification, eSetBits);
			} else {
				xTaskNotify(output_task, task_ctrl_act_succeeded_notification, eSetBits);
			}
		}
		
		if (Notification(notification_value, T1C_NOTIFY_CAL_2L_MASK)) {
			ir_error_t ret = _t1c_perform_cal(CAL_2_PT_L);
			if (ret != IR_SUCCESS) {
				ESP_LOGE(TAG, "Two point calibration (low) failed with %d", ret);
				xTaskNotify(output_task, task_ctrl_act_failed_notification, eSetBits);
			} else {
				xTaskNotify(output_task, task_ctrl_act_succeeded_notification, eSetBits);
			}
		}
		
		if (Notification(notification_value, T1C_NOTIFY_CAL_2H_MASK)) {
			ir_error_t ret = _t1c_perform_cal(CAL_2_PT_H);
			if (ret != IR_SUCCESS) {
				ESP_LOGE(TAG, "Two point calibration (high) failed with %d", ret);
				xTaskNotify(output_task, task_ctrl_act_failed_notification, eSetBits);
			} else {
				xTaskNotify(output_task, task_ctrl_act_succeeded_notification, eSetBits);
			}
		}
		
		if (Notification(notification_value, T1C_NOTIFY_FFC_MASK)) {
			ESP_LOGI(TAG, "Manual FFC triggered");
			ir_error_t ret = ooc_b_update(B_UPDATE);
			if (ret != IR_SUCCESS) {
				ESP_LOGE(TAG, "FFC failed with %d", ret);
			}
		}
		
		// Do this ahead of anything that calls _update_tpd_params()
		if (Notification(notification_value, T1C_NOTIFY_UPD_T1C_CONFIG)) {
			// Get updated configuration parameters
			(void) ps_get_config(PS_CONFIG_TYPE_T1C, &t1c_config);
		}
		
		if (Notification(notification_value, T1C_NOTIFY_SET_T_H_MASK)) {
			// Currently we don't use humidity
			env_cond.ambient_humidity = new_env_cond.ambient_humidity;
			env_cond.ambient_humidity_valid = new_env_cond.ambient_humidity_valid;
			
			if ((env_cond.ambient_temp_valid != new_env_cond.ambient_temp_valid) ||
			    (env_cond.ambient_temp != new_env_cond.ambient_temp)) {
			    
			    env_cond.ambient_temp = new_env_cond.ambient_temp;
				env_cond.ambient_temp_valid = new_env_cond.ambient_temp_valid;
				
				if (t1c_config.use_auto_ambient) {
					// Update the Tiny1C if necessary
					_update_tpd_params(false);
				}
			}
		}
		
		if (Notification(notification_value, T1C_NOTIFY_SET_DIST_MASK)) {			
			if ((env_cond.target_distance_valid != new_env_cond.target_distance_valid) ||
			    (env_cond.target_distance != new_env_cond.target_distance)) {
			    
			    env_cond.target_distance = new_env_cond.target_distance;
				env_cond.target_distance_valid = new_env_cond.target_distance_valid;
				
				if (t1c_config.use_auto_ambient) {
					// Update the Tiny1C if necessary
					_update_tpd_params(false);
				}
			}
		}
		
		if (Notification(notification_value, T1C_NOTIFY_FILE_GET_IMAGE_MASK)) {
			notify_get_file_image = true;
		}
		
		if (Notification(notification_value, T1C_NOTIFY_ENV_UPD_MASK)) {			
			// Update the Tiny1C if necessary
			_update_tpd_params(false);
		}
	}
}


static void _update_tpd_params(bool force_update)
{
	bool update_tau = force_update;
	float ta = 0;
	float dist = 0;
	uint16_t new_param_val;
	
	// Look at environmental conditions that affect tau and reconfigure the Tiny1C as necessary
	//
	// Ambient temperature
	if (t1c_config.use_auto_ambient && env_cond.ambient_temp_valid) {
		new_param_val = temperature_to_param_value(env_cond.ambient_temp);
		ta = (float) env_cond.ambient_temp;
	} else {
		new_param_val = temperature_to_param_value(t1c_config.atmospheric_temp);
		ta = (float) t1c_config.atmospheric_temp;
	}
	if (new_param_val != tpd_settings_values[TPD_PROP_TA]) {
		t1c_set_param_tpd(TPD_PROP_TA, new_param_val);
		update_tau = true;
	}
	
	// Reflective temp
	if (!t1c_config.refl_equals_ambient) {
		new_param_val = temperature_to_param_value(t1c_config.reflected_temp);
	} else if (t1c_config.use_auto_ambient && env_cond.ambient_temp_valid) {
		new_param_val = temperature_to_param_value(env_cond.ambient_temp);
	} else {
		new_param_val = temperature_to_param_value(t1c_config.atmospheric_temp);
	}
	if (new_param_val != tpd_settings_values[TPD_PROP_TU]) {
		t1c_set_param_tpd(TPD_PROP_TU, new_param_val);
	}
	
	// Distance
	if (t1c_config.use_auto_ambient && env_cond.target_distance_valid) {
		new_param_val = dist_cm_to_param_value(env_cond.target_distance);
		dist = (float) env_cond.target_distance / 100.0;
	} else {
		new_param_val = dist_cm_to_param_value(t1c_config.distance);
		dist = (float) t1c_config.distance / 100.0;
	}
	if (new_param_val != tpd_settings_values[TPD_PROP_DISTANCE]) {
		t1c_set_param_tpd(TPD_PROP_DISTANCE, new_param_val);
		update_tau = true;
	}
	
	// Tau
	if (update_tau) {
		new_param_val = estimate_tau(ta, dist, 0);
		t1c_set_param_tpd(TPD_PROP_TAU, new_param_val);
	}
}


// Access the Tiny1C CCI between frames.  We do this because I found that accessing the
// CCI while reading frame data could cause a malfunction.  In order to minimize impact
// on the frame rate we re-implement some of the Falcon CCI commands here so that we can
// separate polling for CCI ready from the actual access.  We poll between frames until
// the CCI is ready then we make the access after the next frame.
static void _eval_cci()
{
	uint8_t data[16];
	uint8_t status;
	size_t param_len;
	uint8_t* param;
	
	switch (cci_state) {
		case CCI_ACCESS_ST_IDLE:
			if (spot_en) {
				cci_state = CCI_ACCESS_ST_REQ_SPOT;
			} else if (minmax_en) {
				cci_state = CCI_ACCESS_ST_REQ_MM;
			} else if (region_en) {
				cci_state = CCI_ACCESS_ST_REQ_REG;
			} else {
				cci_state = CCI_ACCESS_ST_EX_CMD;
			}		
			break;
		
		case CCI_ACCESS_ST_REQ_SPOT:
			// Initiate the TPD_GET_POINT_TEMP command
			_cci_send_get_point_temp();
			cci_state = CCI_ACCESS_ST_WAIT_SPOT; 
			break;
		
		case CCI_ACCESS_ST_WAIT_SPOT:
			// Check if the Tiny1C is no longer busy processing the command
			if (i2c_data_read(I2C_SLAVE_ID, I2C_VD_BUFFER_STATUS, 1, &status) != IR_SUCCESS) {
				ESP_LOGE(TAG, "read (spot) status failed");
			} else if ((status & (VCMD_BUSY_STS_BIT)) == VCMD_BUSY_STS_IDLE) {
				if ((status & VCMD_RST_STS_BIT) == VCMD_RST_STS_PASS) {
					// Read the spot location
					if (i2c_data_read(I2C_SLAVE_ID, I2C_VD_BUFFER_RW + 16, 2, data) != IR_SUCCESS) {
						ESP_LOGE(TAG, "read spot data failed");
					} else {
						spot_temp_raw = ((uint16_t)data[0] << 8) + data[1];
						spot_valid = true;
					}
				} else {
					ESP_LOGE(TAG, "read (spot) status returned error 0x%x", status & VCMD_ERR_STS_BIT);
				}
				
				if (minmax_en) {
					cci_state = CCI_ACCESS_ST_REQ_MM;
				} else if (region_en) {
					cci_state = CCI_ACCESS_ST_REQ_REG;
				} else {
					cci_state = CCI_ACCESS_ST_EX_CMD;
				}
			}
			break;
		
		case CCI_ACCESS_ST_REQ_MM:
			// Initiate the TPD_GET_MAX_MIN_TEMP command
			_cci_set_get_min_max_temp();
			cci_state = CCI_ACCESS_ST_WAIT_MM;
			break;
		
		case CCI_ACCESS_ST_WAIT_MM:
			// Check if the Tiny1C is no longer busy processing the command
			if (i2c_data_read(I2C_SLAVE_ID, I2C_VD_BUFFER_STATUS, 1, &status) != IR_SUCCESS) {
				ESP_LOGE(TAG, "read (minmax) status failed");
			} else if ((status & (VCMD_BUSY_STS_BIT)) == VCMD_BUSY_STS_IDLE) {
				if ((status & VCMD_RST_STS_BIT) == VCMD_RST_STS_PASS) {
					// Read the spot location
					if (i2c_data_read(I2C_SLAVE_ID, I2C_VD_BUFFER_RW + 16, 12, data) != IR_SUCCESS) {
						ESP_LOGE(TAG, "read minmax data failed");
					} else {
					    max_min_temp_data.max_temp = ((uint16_t)data[0] << 8) + data[1];
					    max_min_temp_data.min_temp = ((uint16_t)data[2] << 8) + data[3];
					    max_min_temp_data.max_temp_point.x = ((uint16_t)data[4] << 8) + data[5];
					    max_min_temp_data.max_temp_point.y = ((uint16_t)data[6] << 8) + data[7];
					    max_min_temp_data.min_temp_point.x = ((uint16_t)data[8] << 8) + data[9];
					    max_min_temp_data.min_temp_point.y = ((uint16_t)data[10] << 8) + data[11];
					    minmax_valid = true;
					}
				} else {
					ESP_LOGE(TAG, "read (minmax) status returned error 0x%x", status & VCMD_ERR_STS_BIT);
				}
				
				if (region_en) {
					cci_state = CCI_ACCESS_ST_REQ_REG;
				} else {
					cci_state = CCI_ACCESS_ST_EX_CMD;
				}
			}
			break;
		
		case CCI_ACCESS_ST_REQ_REG:
			// Initiate the TPD_GET_RECT_TEMP command
			_cci_set_get_region_temp();
			cci_state = CCI_ACCESS_ST_WAIT_REG;
			break;
		
		case CCI_ACCESS_ST_WAIT_REG:
			// Check if the Tiny1C is no longer busy processing the command
			if (i2c_data_read(I2C_SLAVE_ID, I2C_VD_BUFFER_STATUS, 1, &status) != IR_SUCCESS) {
				ESP_LOGE(TAG, "read (rect) status failed");
			} else if ((status & (VCMD_BUSY_STS_BIT)) == VCMD_BUSY_STS_IDLE) {
				if ((status & VCMD_RST_STS_BIT) == VCMD_RST_STS_PASS) {
					// Read the region temp info
					if (i2c_data_read(I2C_SLAVE_ID, I2C_VD_BUFFER_RW + 16, 14, data) != IR_SUCCESS) {
						ESP_LOGE(TAG, "read rect data failed");
					} else {
						    region_temp_info.temp_info_value.ave_temp = ((uint16_t)data[0] << 8) + data[1];
						    region_temp_info.temp_info_value.max_temp = ((uint16_t)data[2] << 8) + data[3];
						    region_temp_info.temp_info_value.min_temp = ((uint16_t)data[4] << 8) + data[5];
						    region_temp_info.max_temp_point.x = ((uint16_t)data[6] << 8) + data[7];
						    region_temp_info.max_temp_point.y = ((uint16_t)data[8] << 8) + data[9];
						    region_temp_info.min_temp_point.x = ((uint16_t)data[10] << 8) + data[11];
						    region_temp_info.min_temp_point.y = ((uint16_t)data[12] << 8) + data[13];
						    region_valid = true;
					}
				} else {
					ESP_LOGE(TAG, "read (rect) status returned error 0x%x", status & VCMD_ERR_STS_BIT);
				}
				
				cci_state = CCI_ACCESS_ST_EX_CMD;
			}
			break;
		
		case CCI_ACCESS_ST_EX_CMD:
			// Look for a parameter update (we process one at a time as we don't expect them frequently
			// and that has the least impact to frame rate)
			param = (uint8_t*) xRingbufferReceive(param_buf_handle, &param_len, 0);
			if ((param != NULL) && (!cal_2pt_in_progress)) {
				if (param_len == sizeof(param_buffer_entry_t)) {
					_cci_fast_set_param((param_buffer_entry_t*) param);
				} else {
					ESP_LOGE(TAG, "Unexpected parameter length %d", param_len);
				}
				vRingbufferReturnItem(param_buf_handle, (void *)param);
				
				cci_state = CCI_ACCESS_ST_WAIT_CMD;
			} else {			
				if (spot_en) {
					cci_state = CCI_ACCESS_ST_REQ_SPOT;
				} else if (minmax_en) {
					cci_state = CCI_ACCESS_ST_REQ_MM;
				} else if (region_en) {
					cci_state = CCI_ACCESS_ST_REQ_REG;
				} else {
					cci_state = CCI_ACCESS_ST_EX_CMD;
				}
			}
			break;
		
		case CCI_ACCESS_ST_WAIT_CMD:
			// Check if the Tiny1C is no longer busy processing the command
			if (i2c_data_read(I2C_SLAVE_ID, I2C_VD_BUFFER_STATUS, 1, &status) != IR_SUCCESS) {
				ESP_LOGE(TAG, "read (rect) status failed");
			} else if ((status & (VCMD_BUSY_STS_BIT)) == VCMD_BUSY_STS_IDLE) {
				if (spot_en) {
					cci_state = CCI_ACCESS_ST_REQ_SPOT;
				} else if (minmax_en) {
					cci_state = CCI_ACCESS_ST_REQ_MM;
				} else if (region_en) {
					cci_state = CCI_ACCESS_ST_REQ_REG;
				} else {
					cci_state = CCI_ACCESS_ST_EX_CMD;
				}
			}
			break;
		
		default:
			cci_state = CCI_ACCESS_ST_IDLE;
	}
}


// Fast get_spot_temp API call (Tiny1C must be ready)
static void _cci_send_get_point_temp()
{
	uint8_t cci_reg_array[8];
	
	// Simulate a long command 
	cci_reg_array[0] = CMDTYPE_LONG_TYPE_TPD;            // byCmdType   (wParam[15:8])
	cci_reg_array[1] = SUBCMD_TPD_GET_POINT_TEMP;        // bySubCmd    (wParam[7:0])
	cci_reg_array[2] = 0;                                // byParam_h
	cci_reg_array[3] = 0;                                // byParam_l
	cci_reg_array[4] = spot_param.x >> 8;                // byAddr1_hh  (dwAddr1[31:24])
	cci_reg_array[5] = spot_param.x & 0xFF;              // byAddr1_h   (dwAddr1[23:16])
	cci_reg_array[6] = spot_param.y >> 8;                // byAddr1_l   (dwAddr1[15:8])
	cci_reg_array[7] = spot_param.y & 0xFF;              // byAddr1_ll  (dwAddr1[7:0])
	if (i2c_data_write_no_wait(I2C_SLAVE_ID, I2C_VD_BUFFER_HLD, 8, cci_reg_array) != IR_SUCCESS) {
		ESP_LOGE(TAG, "write I2C_VD_BUFFER_HLD failed");
		return;
	}
	
	cci_reg_array[0] = 0;                                // byAddr2_hh  (dwAddr2[31:24])
	cci_reg_array[1] = 0;                                // byAddr2_h   (dwAddr2[23:16])
	cci_reg_array[2] = 0;                                // byAddr2_l   (dwAddr2[15:8])
	cci_reg_array[3] = 0;                                // byAddr2_ll  (dwAddr2[7:0])
	cci_reg_array[4] = 0;                                // byLen_hh    (dwLen[31:24])
	cci_reg_array[5] = 0;                                // byLen_h     (dwLen[23:16])
	cci_reg_array[6] = 0;                                // byLen_l     (dwLen[15:8])
	cci_reg_array[7] = 2;                                // byLen_ll    (dwLen[7:0])
	if (i2c_data_write_no_wait(I2C_SLAVE_ID, I2C_VD_BUFFER_RW + 8, 8, cci_reg_array) != IR_SUCCESS) {
		ESP_LOGE(TAG, "write I2C_VD_BUFFER_RW failed");
	}
}


// Fast get_min_max_temp API call (Tiny1C must be ready)
static void _cci_set_get_min_max_temp()
{
	uint8_t cci_reg_array[8];
	
	// Simulate a long command 
	cci_reg_array[0] = CMDTYPE_LONG_TYPE_TPD;
	cci_reg_array[1] = SUBCMD_TPD_GET_MAX_MIN_TEMP_INFO;
	cci_reg_array[2] = 0;
	cci_reg_array[3] = 0;
	cci_reg_array[4] = 0;
	cci_reg_array[5] = 0;
	cci_reg_array[6] = 0;
	cci_reg_array[7] = 0;
	if (i2c_data_write_no_wait(I2C_SLAVE_ID, I2C_VD_BUFFER_HLD, 8, cci_reg_array) != IR_SUCCESS) {
		ESP_LOGE(TAG, "write I2C_VD_BUFFER_HLD failed");
		return;
	}
	
	cci_reg_array[0] = 0;
	cci_reg_array[1] = 0;
	cci_reg_array[2] = 0;
	cci_reg_array[3] = 0;
	cci_reg_array[4] = 0;
	cci_reg_array[5] = 0;
	cci_reg_array[6] = 0;
	cci_reg_array[7] = 12;
	if (i2c_data_write_no_wait(I2C_SLAVE_ID, I2C_VD_BUFFER_RW + 8, 8, cci_reg_array) != IR_SUCCESS) {
		ESP_LOGE(TAG, "write I2C_VD_BUFFER_RW failed");
	}
}


// Fast get_rect_temp API call (Tiny1C must be ready)
static void _cci_set_get_region_temp()
{
	uint8_t cci_reg_array[8];
	
	// Simulate a long command 
	cci_reg_array[0] = CMDTYPE_LONG_TYPE_TPD;
	cci_reg_array[1] = SUBCMD_TPD_GET_RECT_TEMP;
	cci_reg_array[2] = 0;
	cci_reg_array[3] = 0;
	cci_reg_array[4] = region_param.start_point.x >> 8;
	cci_reg_array[5] = region_param.start_point.x & 0xFF;
	cci_reg_array[6] = region_param.start_point.y >> 8;
	cci_reg_array[7] = region_param.start_point.y & 0xFF;
	if (i2c_data_write_no_wait(I2C_SLAVE_ID, I2C_VD_BUFFER_HLD, 8, cci_reg_array) != IR_SUCCESS) {
		ESP_LOGE(TAG, "write I2C_VD_BUFFER_HLD failed");
		return;
	}
	
	cci_reg_array[0] = region_param.end_point.x >> 8;
	cci_reg_array[1] = region_param.end_point.x & 0xFF;
	cci_reg_array[2] = region_param.end_point.y >> 8;
	cci_reg_array[3] = region_param.end_point.y & 0xFF;
	cci_reg_array[4] = 0;
	cci_reg_array[5] = 0;
	cci_reg_array[6] = 0;
	cci_reg_array[7] = 14;
	if (i2c_data_write_no_wait(I2C_SLAVE_ID, I2C_VD_BUFFER_RW + 8, 8, cci_reg_array) != IR_SUCCESS) {
		ESP_LOGE(TAG, "write I2C_VD_BUFFER_RW failed");
	}
}

// Fast implementation of parameter setting routines
static void _cci_write_param(uint8_t sub_cmd, uint8_t param, uint16_t value)
{
	uint8_t cci_reg_array[8];
	
	// Simulate a long command 
	cci_reg_array[0] = CMDTYPE_LONG_TYPE_PROP_PAGE;
	cci_reg_array[1] = sub_cmd;
	cci_reg_array[2] = 0;
	cci_reg_array[3] = param;
	cci_reg_array[4] = 0;
	cci_reg_array[5] = 0;
	cci_reg_array[6] = value >> 8;
	cci_reg_array[7] = value & 0xFF;
	if (i2c_data_write_no_wait(I2C_SLAVE_ID, I2C_VD_BUFFER_HLD, 8, cci_reg_array) != IR_SUCCESS) {
		ESP_LOGE(TAG, "write I2C_VD_BUFFER_HLD failed");
		return;
	}
	
	cci_reg_array[0] = 0;
	cci_reg_array[1] = 0;
	cci_reg_array[2] = 0;
	cci_reg_array[3] = 0;
	cci_reg_array[4] = 0;
	cci_reg_array[5] = 0;
	cci_reg_array[6] = 0;
	cci_reg_array[7] = 2;
	if (i2c_data_write_no_wait(I2C_SLAVE_ID, I2C_VD_BUFFER_RW + 8, 8, cci_reg_array) != IR_SUCCESS) {
		ESP_LOGE(TAG, "write I2C_VD_BUFFER_RW failed");
	}
}


static void _cci_fast_set_param(param_buffer_entry_t* buf_entryP)
{
	switch (buf_entryP->type) {
		case PARAM_BUF_TYPE_SHUTTER:
			_cci_write_param(SUBCMD_PROP_AUTO_SHUTTER_PARAM_SET, buf_entryP->param, buf_entryP->value);
			
			// Update our local copy for change detection
			if (buf_entryP->param < PARAM_NUM_TYPE_SHUTTER) {
				shutter_settings_values[buf_entryP->param] = buf_entryP->value;
			}
			break;
		case PARAM_BUF_TYPE_IMAGE:
			_cci_write_param(SUBCMD_PROP_IMAGE_PARAM_SET, buf_entryP->param, buf_entryP->value);
			
			// Update our local copy for change detection
			if (buf_entryP->param < PARAM_NUM_TYPE_IMAGE) {
				image_settings_values[buf_entryP->param] = buf_entryP->value;
			}
			break;
		case PARAM_BUF_TYPE_TPD:
			_cci_write_param(SUBCMD_PROP_TPD_PARAM_SET, buf_entryP->param, buf_entryP->value);
			
			// Update our local copy for change detection
			if (buf_entryP->param < PARAM_NUM_TYPE_TPD) {
				tpd_settings_values[buf_entryP->param] = buf_entryP->value;
			}
			
			// Re-read the TAU table on gain change
			if (buf_entryP->param == TPD_PROP_GAIN_SEL) {
				if (tpd_settings_values[TPD_PROP_GAIN_SEL] != buf_entryP->value) {
					if (read_correct_table((buf_entryP->value != 0) ? HIGH_GAIN : LOW_GAIN) != 0) {
						ESP_LOGE(TAG, "Could not read correct_table for gain %u", buf_entryP->value);
					}
					
					// Force a recompute of TAU
					_update_tpd_params(true);
				}
			}
			break;
		default:
			ESP_LOGE(TAG, "Unknown param type %u", buf_entryP->type);
	}
	
	ESP_LOGI(TAG, "Set param type %u, param %u, value %u", buf_entryP->type, buf_entryP->param, buf_entryP->value);
}


#ifdef INCLUDE_SHUTTER_DISPLAY
static void _display_shutter_values()
{
	float t;
	
	ESP_LOGI(TAG, "Shutter Auto Enable = %lu", shutter_settings_values[SHUTTER_PROP_SWITCH]);
	ESP_LOGI(TAG, "Shutter Auto Min Interval = %lu", shutter_settings_values[SHUTTER_PROP_MIN_INTERVAL]);
	ESP_LOGI(TAG, "Shutter Auto Max Interval = %lu", shutter_settings_values[SHUTTER_PROP_MAX_INTERVAL]);
	t = (float) shutter_settings_values[SHUTTER_PROP_TEMP_THRESHOLD_B] / 36.0;
	ESP_LOGI(TAG, "Shutter Temp Threshold = %f", t);
	ESP_LOGI(TAG, "Shutter Auto Protect = %lu", shutter_settings_values[SHUTTER_PROP_PROTECT_SWITCH]);
	ESP_LOGI(TAG, "Shutter Auto Any Interval = %lu", shutter_settings_values[SHUTTER_PROP_ANY_INTERVAL]);
	t = (float) shutter_settings_values[SHUTTER_PROTECT_THR_HIGH_GAIN] / 36.0;
	ESP_LOGI(TAG, "Shutter Protect High Gain Threshold = %f", t);
	t = (float) shutter_settings_values[SHUTTER_PROTECT_THR_LOW_GAIN] / 36.0;
	ESP_LOGI(TAG, "Shutter Protect Low Gain Threshold = %f", t);
	ESP_LOGI(TAG, "Shutter Preview Start 1st delay = %lu", shutter_settings_values[SHUTTER_PREVIEW_START_1ST_DELAY]);
	ESP_LOGI(TAG, "Shutter Preview Start 2nd delay = %lu", shutter_settings_values[SHUTTER_PREVIEW_START_2ND_DELAY]);
	ESP_LOGI(TAG, "Shutter Gain Change 1st delay = %lu", shutter_settings_values[SHUTTER_CHANGE_GAIN_1ST_DELAY]);
	ESP_LOGI(TAG, "Shutter Gain Change 2nd delay = %lu", shutter_settings_values[SHUTTER_CHANGE_GAIN_2ND_DELAY]);
}
#endif


#ifdef INCLUDE_IMAGE_DISPLAY
static void _display_image_values()
{
	ESP_LOGI(TAG, "Image TNR = %lu", image_settings_values[IMAGE_PROP_LEVEL_TNR]);
	ESP_LOGI(TAG, "Image SNR = %lu", image_settings_values[IMAGE_PROP_LEVEL_SNR]);
	ESP_LOGI(TAG, "Image DDE = %lu", image_settings_values[IMAGE_PROP_LEVEL_DDE]);
	ESP_LOGI(TAG, "Image Brightness = %lu", image_settings_values[IMAGE_PROP_LEVEL_BRIGHTNESS]);
	ESP_LOGI(TAG, "Image Contrast = %lu", image_settings_values[IMAGE_PROP_LEVEL_CONTRAST]);
	ESP_LOGI(TAG, "Image AGC Level = %lu", image_settings_values[IMAGE_PROP_MODE_AGC]);
	ESP_LOGI(TAG, "Image AGC Max Gain = %lu", image_settings_values[IMAGE_PROP_LEVEL_MAX_GAIN]);
	ESP_LOGI(TAG, "Image AGC Brightness Offset = %lu", image_settings_values[IMAGE_PROP_LEVEL_BOS]);
	ESP_LOGI(TAG, "Image AGC Enable = %lu", image_settings_values[IMAGE_PROP_ONOFF_AGC]);
	ESP_LOGI(TAG, "Image Mirror Flip = %lu", image_settings_values[IMAGE_PROP_SEL_MIRROR_FLIP]);
}
#endif


#ifdef INCLUDE_TPD_DISPLAY
static void _display_tpd_values()
{
	ESP_LOGI(TAG, "TPD Distance = %lu", param_to_dist_cm_value(tpd_settings_values[TPD_PROP_DISTANCE]));
	ESP_LOGI(TAG, "TPD Emissivity = %lu", param_to_emissivity_value(tpd_settings_values[TPD_PROP_EMS]));
	ESP_LOGI(TAG, "TPD Atmospheric temp = %ld", param_to_temperature_value(tpd_settings_values[TPD_PROP_TA]));
	ESP_LOGI(TAG, "TPD Reflected temp = %ld", param_to_temperature_value(tpd_settings_values[TPD_PROP_TU]));
	ESP_LOGI(TAG, "TPD TAU = %lu", param_to_emissivity_value(tpd_settings_values[TPD_PROP_TAU]));
	ESP_LOGI(TAG, "TPD Gain = %u", tpd_settings_values[TPD_PROP_GAIN_SEL]);		
}
#endif
