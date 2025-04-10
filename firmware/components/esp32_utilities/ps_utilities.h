/*
 * Persistent Storage Module
 *
 * Manage the persistent storage kept in the ESP32 NVS and provide access
 * routines to it.
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
 *
 */
#ifndef PS_UTILITIES_H
#define PS_UTILITIES_H

#include <stdbool.h>
#include <stdint.h>



//
// PS Utilities Constants
//

//
// Configuration types
#define PS_NUM_CONFIGS           3

#define PS_CONFIG_TYPE_NET       0
#define PS_CONFIG_TYPE_T1C       1
#define PS_CONFIG_TYPE_OUT       2

// Net 

// Output module enable flag masks (32-bit flag)
#define PS_EN_FLAG_VID_IS_PAL    0x00000001
#define PS_EN_FLAG_SPOTMETER     0x00000002
#define PS_EN_FLAG_MINMAX_MRK    0x00000004
#define PS_EN_FLAG_MINMAX_TMP    0x00000008
#define PS_EN_FLAG_UNITS_METRIC  0x00000010
#define PS_EN_FLAG_SAVE_OVL      0x00000020

// Field lengths
#define PS_SSID_MAX_LEN          32
#define PS_PW_MAX_LEN            63


//
// Default values

// Network
// Base part of the default SSID/Camera name - the last 4 nibbles of the ESP32's
// mac address are appended as ASCII characters
#ifdef CONFIG_BUILD_ICAM_MINI
#define PS_DEFAULT_AP_SSID      "iCam-Mini-"
#else
#define PS_DEFAULT_AP_SSID      "iCam-"
#endif

// Tiny1C configuration
#define PS_DEF_AUTO_FFC          true
#define PS_DEF_HIGH_GAIN         true
#define PS_DEF_USE_AUTO          false
#define PS_DEF_REFL_EQ_AMB       false
#define PS_DEF_ATMOSPHERIC_TEMP  25
#define PS_DEF_BRIGHTNESS        50
#define PS_DEF_DISTANCE          100
#define PS_DEF_EMISSIVITY        100
#define PS_DEF_FFC_DELTA_T_X10   15
#define PS_DEF_HUMIDITY          30
#define PS_DEF_REFL_TEMP         25
#define PS_DEF_MIN_FFC_INT       5
#define PS_DEF_MAX_FFC_INT       300

// Output module flags (set to 0 to disable, corresponding flag mask to enable)
#define PS_DEF_FLAG_VID_IS_PAL  0
#define PS_DEF_FLAG_SPOTMETER   PS_EN_FLAG_SPOTMETER
#define PS_DEF_FLAG_MINMAX_M    0
#define PS_DEF_FLAG_MINMAX_T    PS_EN_FLAG_MINMAX_TMP
#define PS_DEF_FLAG_METRIC      0

#define PS_DEF_FLAG_SETUP       (PS_DEF_FLAG_VID_IS_PAL | PS_DEF_FLAG_SPOTMETER | PS_DEF_FLAG_MINMAX_M | PS_DEF_FLAG_MINMAX_T | PS_DEF_FLAG_METRIC)

// Palettes
#define PS_DEF_PALETTE_INDEX    0



//
// PS Utilities config typedefs

typedef struct {
	bool mdns_en;                      // 0: mDNS discovery disabled, 1: mDNS discovery enabled
	bool sta_mode;                     // 0: AP mode, 1: STA mode
	bool sta_static_ip;                // In station mode: 0: DHCP-served IP, 1: Static IP
	char ap_ssid[PS_SSID_MAX_LEN+1];   // AP SSID is also the Camera Name
	char sta_ssid[PS_SSID_MAX_LEN+1];
	char ap_pw[PS_PW_MAX_LEN+1];
	char sta_pw[PS_PW_MAX_LEN+1];
	uint8_t ap_ip_addr[4];
	uint8_t sta_ip_addr[4];
	uint8_t sta_netmask[4];
} net_config_t;

typedef struct {
	bool auto_ffc_en;                  // Enable automatic FFC
	bool high_gain;                    // Set for high gain, clear for low gain
    bool use_auto_ambient;             // Use ambient values from sensors
    bool refl_equals_ambient;          // Use ambient temp for reflective temp when set
    int32_t atmospheric_temp;          // °C
	uint32_t brightness;               // 0 - 100
	uint32_t distance;                 // cm
	uint32_t emissivity;               // 0 - 100
	uint32_t ffc_temp_threshold_x10;   // °C * 10
	uint32_t humidity;                 // 0 - 100
	uint32_t min_ffc_interval;         // seconds 5 - 655
	uint32_t max_ffc_interval;         // seconds min - 655
	int32_t reflected_temp;            // °C
} t1c_config_t;

typedef struct {
	uint32_t config_flags;
	uint32_t gui_palette_index;        // Used for GUI and save operations with LVGL GUI output
	uint32_t sav_palette_index;        // Used for save operations with video output
	uint32_t vid_palette_index;        // Used for GUI with video output
	uint32_t lcd_brightness;           // 0 - 100, Used for gCore LCD backlight
} out_config_t;


//
// PS Utilities API
//
bool ps_init();
bool ps_get_config(int index, void* cfg);
bool ps_set_config(int index, void* cfg);
bool ps_reinit_all();
bool ps_reinit_config(int index);
bool ps_has_new_cam_name(const char* name);
char ps_nibble_to_ascii(uint8_t n);

#endif /* PS_UTILITIES_H */