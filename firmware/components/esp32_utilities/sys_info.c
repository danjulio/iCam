/*
 * Get sys_info string for display
 *
 * Copyright 2023-2024 Dan Julio
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
#include "env_task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "file_utilities.h"
#include "sys_info.h"
#include "t1c_task.h"
#include "time_utilities.h"
#include <string.h>

#ifdef CONFIG_BUILD_ICAM_MINI
	#include "ctrl_task.h"
	#include "esp_mac.h"
	#include "ps_utilities.h"
	#include "wifi_utilities.h"
#else
	#include "gcore.h"
	#include "gcore_task.h"
	#include "power_utilities.h"
#endif


//
// Variables
//

// Camera information string
static char cam_info_buf[SYS_INFO_MAX_LEN+1];

#ifdef CONFIG_BUILD_ICAM_MINI
	static net_config_t wifi_info;
#endif

static const char* copyright_info = "\niCamCtrl copyright (c) 2024\n"
                                    "by Dan Julio.  All rights reserved.\n\n"
                                    "MIT licensed code:\n"
#ifdef CONFIG_BUILD_ICAM_MINI
                                    "  Alon Zakai, et al - Emscripten\n"
                                    "   Copyright 2010-2014\n"
                                    "   MUSL Copyright 2005-2020 Rich Felker\n"
#endif
                                    "  Sergio Gonzalez - Tiny JPEG decoder\n"
                                    "   No copyright\n"
                                    "  Shifeng Li - AHT20 driver\n"
                                    "   Copyright 2015-2024 Libdriver\n"
                                    "  Gabor Kiss-Vamosi, et al - LVGL\n"
                                    "   Copyright 2020\n"
                                    "\nBSD 3-clause VL53L4CX driver\n"
                                    "  Copyright 2020, STMicro\n"
                                    "\nTJpgDec\n"
                                    "  Copyright 2021, ChaN\n";



//
// Forward declarations for internal functions
//
static void _update_info();
static int _add_platform(int n);
static int _add_fw_version(int n);
static int _add_sdk_version(int n);
static int _add_tiny1c_info(int n);
static int _add_sensor_info(int n);
static int _add_battery_info(int n);
static int _add_time(int n);
static int _add_storage_info(int n);
static int _add_mem_info(int n);
static int _add_copyright_info(int n);

#ifdef CONFIG_BUILD_ICAM_MINI
static int _add_wifi_mode(int n);
static int _add_ip_address(int n);
static int _add_mac_address(int n);
#else
static int _add_gcore_pmic_version(int n);
#endif



//
// API
//
char* sys_info_get_string()
{
	_update_info();
	
	return cam_info_buf;
}



//
// Internal functions
//
static void _update_info()
{
	int n = 0;
	
#ifdef CONFIG_BUILD_ICAM_MINI
	(void) ps_get_config(PS_CONFIG_TYPE_NET, &wifi_info);
#endif
	
	n = _add_platform(n);
	n = _add_fw_version(n);
	n = _add_sdk_version(n);
	n = _add_tiny1c_info(n);
	n = _add_sensor_info(n);
#ifndef CONFIG_BUILD_ICAM_MINI
	n = _add_gcore_pmic_version(n);
#endif
	n = _add_battery_info(n);
#ifdef CONFIG_BUILD_ICAM_MINI
	n = _add_wifi_mode(n);
	n = _add_ip_address(n);
	n = _add_mac_address(n);
#endif
	n = _add_time(n);
	n = _add_storage_info(n);
	n = _add_mem_info(n);
	n = _add_copyright_info(n);
}


static int _add_platform(int n)
{
#ifdef CONFIG_BUILD_ICAM_MINI
	sprintf(&cam_info_buf[n], "Platform: iCamMini\n");
#else
	sprintf(&cam_info_buf[n], "Platform: iCam\n");
#endif

	return (strlen(cam_info_buf));
}


static int _add_fw_version(int n)
{
	const esp_app_desc_t* app_desc;
	
	app_desc = esp_app_get_description();
	sprintf(&cam_info_buf[n], "FW Version: %s\n", app_desc->version);
	
	return (strlen(cam_info_buf));
}


static int _add_sdk_version(int n)
{
	sprintf(&cam_info_buf[n], "SDK Version: %s\n", esp_get_idf_version());
	
	return (strlen(cam_info_buf));
}


static int _add_tiny1c_info(int n)
{
	sprintf(&cam_info_buf[n], "Tiny1C Version: %s\n", t1c_get_module_version());
	
	n = strlen(cam_info_buf);
	sprintf(&cam_info_buf[n], "Tiny1C Serial Number: %s\n", t1c_get_module_sn());

	return (strlen(cam_info_buf));
}


static int _add_sensor_info(int n)
{
	bool t_h, d;
	
	env_sensor_present(&t_h, &d);
	
	if (t_h && d) {
		sprintf(&cam_info_buf[n], "Env Sensors: Temp, Hum, Distance\n");
	} else if (t_h) {
		sprintf(&cam_info_buf[n], "Env Sensors: Temp, Hum\n");
	} else if (d) {
		sprintf(&cam_info_buf[n], "Env Sensors: Distance\n");
	} else {
		sprintf(&cam_info_buf[n], "Env Sensors: None\n");
	}
	
	return (strlen(cam_info_buf));
}


static int _add_battery_info(int n)
{
#ifdef CONFIG_BUILD_ICAM_MINI
	uint16_t mv = ctrl_get_batt_mv();
	
	if (mv > 4200) {
		sprintf(&cam_info_buf[n], "Internal DC: %1.2f V\n", (float) mv / 1000.0);
	} else {
		sprintf(&cam_info_buf[n], "Battery: %1.2f V\n", (float) mv / 1000.0);
	}
#else
	enum BATT_STATE_t bs;
	enum CHARGE_STATE_t cs;
	
	gcore_get_power_state(&bs, &cs);
	sprintf(&cam_info_buf[n], "Battery: %1.2fv, %umA, Charge ",
	       (float) gcore_get_batt_mv() / 1000.0, gcore_get_load_ma());
	n = strlen(cam_info_buf);
	switch (cs) {
		case CHARGE_OFF:
			sprintf(&cam_info_buf[n], "off\n");
			break;
		case CHARGE_ON:
			sprintf(&cam_info_buf[n], "on\n");
			break;
		case CHARGE_DONE:
			sprintf(&cam_info_buf[n], "done\n");
			break;
		case CHARGE_FAULT:
			sprintf(&cam_info_buf[n], "fault\n");
			break;
	}
#endif

	return (strlen(cam_info_buf));
}


static int _add_time(int n)
{
	char buf[28];
	tmElements_t te;
	
	time_get(&te);
	time_get_disp_string(&te, buf);
	sprintf(&cam_info_buf[n], "Time: %s\n", buf);
	
	return (strlen(cam_info_buf));
}


static int _add_storage_info(int n)
{
	bool sd_card_present;
	int mb_tot;
	int mb_free;
	
#ifdef CONFIG_BUILD_ICAM_MINI
	sd_card_present = ctrl_get_sdcard_present();
#else
	sd_card_present = power_get_sdcard_present();
#endif

	if (sd_card_present) {
		mb_tot = (int) (file_get_storage_len() / (1000 * 1000));
		mb_free = (int) (file_get_storage_free() / (1000 * 1000));
		sprintf(&cam_info_buf[n], "Storage: %d MB (of %d MB)\n            %d files\n",
			mb_tot - mb_free, mb_tot, file_get_num_files());
	} else {
		sprintf(&cam_info_buf[n], "Storage: No media\n");
	}
	
	return (strlen(cam_info_buf));
}


static int _add_mem_info(int n)
{
	sprintf(&cam_info_buf[n], "Heap Free: Int %d (min %d)\n            PSRAM %d (min %d)\n",
		heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
		heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
		heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
		heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
		
	return (strlen(cam_info_buf));
}


static int _add_copyright_info(int n)
{
	sprintf(&cam_info_buf[n], copyright_info);
	
	return(strlen(cam_info_buf));
}


#ifdef CONFIG_BUILD_ICAM_MINI

static int _add_wifi_mode(int n)
{
	if (!wifi_info.sta_mode) {
		sprintf(&cam_info_buf[n], "Wifi Mode: AP\n");
	} else if (wifi_info.sta_static_ip) {
		sprintf(&cam_info_buf[n], "Wifi Mode: STA with static IP address\n");
	} else {
		sprintf(&cam_info_buf[n], "Wifi Mode: STA\n");
	}
	
	return (strlen(cam_info_buf));
}


static int _add_ip_address(int n)
{
	char buf[16];   // "XXX.XXX.XXX.XXX" + null
		
	if ((!wifi_info.sta_mode && wifi_is_enabled()) ||
	    ( wifi_info.sta_mode && wifi_is_connected())) {
	    
	    wifi_get_ipv4_addr(buf);
		sprintf(&cam_info_buf[n], "IP Address: %s\n", buf);
	} else {
		sprintf(&cam_info_buf[n], "IP Address: - \n");
	}
	
	return (strlen(cam_info_buf));
}


static int _add_mac_address(int n)
{
	uint8_t sys_mac_addr[6];
	
	esp_efuse_mac_get_default(sys_mac_addr);
	
	// Add 1 for soft AP mode (see "Miscellaneous System APIs" in the ESP-IDF documentation)
	if (!wifi_info.sta_mode) sys_mac_addr[5] += 1;
	
	sprintf(&cam_info_buf[n], "MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
		sys_mac_addr[0], sys_mac_addr[1], sys_mac_addr[2],
		sys_mac_addr[3], sys_mac_addr[4], sys_mac_addr[5]);
	
	return (strlen(cam_info_buf));
}

#else

static int _add_gcore_pmic_version(int n)
{
	uint8_t reg;
	
	if (gcore_get_reg8(GCORE_REG_VER, &reg)) {
		sprintf(&cam_info_buf[n], "RTC/PMIC Version: %d.%d\n", reg >> 4, reg & 0xF);
	} else {
		sprintf(&cam_info_buf[n], "RTC/PMIC Version: ?\n");
	}
	
	return (strlen(cam_info_buf));
}

#endif
