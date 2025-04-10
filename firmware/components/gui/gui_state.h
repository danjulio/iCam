/*
 * GUI side state management.  Contains state for all GUI controls and mechanism to initially
 * request it from the main app.
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
#ifndef GUI_STATE_H
#define GUI_STATE_H

#include <stdbool.h>
#include <stdint.h>



//
// Constants
//

// Initialization mask
#define GUI_STATE_INIT_AMBIENT_C  0x00000001
#define GUI_STATE_INIT_BACKLIGHT  0x00000002
#define GUI_STATE_INIT_BRIGHTNESS 0x00000004
#define GUI_STATE_INIT_CARD_PRES  0x00000008
#define GUI_STATE_INIT_EMISSIVITY 0x00000010
#define GUI_STATE_INIT_GAIN       0x00000020
#define GUI_STATE_INIT_MIN_MAX    0x00000040
#define GUI_STATE_INIT_PALETTE    0x00000080
#define GUI_STATE_INIT_REGION     0x00000100
#define GUI_STATE_INIT_SAVE_OVL   0x00000200
#define GUI_STATE_INIT_SHUTTER    0x00000400
#define GUI_STATE_INIT_SPOT       0x00000800
#define GUI_STATE_INIT_UNIT       0x00001000
#define GUI_STATE_INIT_WIFI       0x00002000

#ifdef ESP_PLATFORM
// iCam doesn't need wifi
#define GUI_STATE_INIT_ALL_MASK   (GUI_STATE_INIT_AMBIENT_C | \
                                   GUI_STATE_INIT_BACKLIGHT | \
                                   GUI_STATE_INIT_BRIGHTNESS | \
                                   GUI_STATE_INIT_CARD_PRES | \
                                   GUI_STATE_INIT_EMISSIVITY | \
                                   GUI_STATE_INIT_GAIN | \
                                   GUI_STATE_INIT_MIN_MAX | \
                                   GUI_STATE_INIT_PALETTE | \
                                   GUI_STATE_INIT_REGION | \
                                   GUI_STATE_INIT_SAVE_OVL | \
                                   GUI_STATE_INIT_SHUTTER | \
                                   GUI_STATE_INIT_SPOT | \
                                   GUI_STATE_INIT_UNIT \
                                  )
#else
// iCamMini doesn't need backlight
#define GUI_STATE_INIT_ALL_MASK   (GUI_STATE_INIT_AMBIENT_C | \
                                   GUI_STATE_INIT_BRIGHTNESS | \
                                   GUI_STATE_INIT_CARD_PRES | \
                                   GUI_STATE_INIT_EMISSIVITY | \
                                   GUI_STATE_INIT_GAIN | \
                                   GUI_STATE_INIT_MIN_MAX | \
                                   GUI_STATE_INIT_PALETTE | \
                                   GUI_STATE_INIT_REGION | \
                                   GUI_STATE_INIT_SAVE_OVL | \
                                   GUI_STATE_INIT_SHUTTER | \
                                   GUI_STATE_INIT_SPOT | \
                                   GUI_STATE_INIT_UNIT | \
                                   GUI_STATE_INIT_WIFI \
                                  )
#endif

// Field lengths
#define GUI_SSID_MAX_LEN          32
#define GUI_PW_MAX_LEN            63



//
// Typedefs
//
// GUI state
typedef struct {
	bool auto_ffc_en;
	bool card_present;
	bool high_gain;
	bool mdns_en;
	bool min_max_mrk_enable;
	bool refl_equals_ambient;
	bool region_enable;
	bool save_ovl_en;
	bool spotmeter_enable;
	bool sta_mode;
	bool sta_static_ip;
	bool temp_unit_C;
	bool timelapse_enable;
	bool timelapse_notify;
	bool timelapse_running;
	bool use_auto_ambient;
	char ap_ssid[GUI_SSID_MAX_LEN+1];
	char sta_ssid[GUI_SSID_MAX_LEN+1];
	char ap_pw[GUI_PW_MAX_LEN+1];
	char sta_pw[GUI_PW_MAX_LEN+1];
	uint8_t ap_ip_addr[4];
	uint8_t sta_ip_addr[4];
	uint8_t sta_netmask[4];
	int32_t atmospheric_temp;
	uint32_t brightness;
	uint32_t distance;
	uint32_t emissivity;
	uint32_t ffc_temp_threshold_x10;
	uint32_t humidity;
	uint32_t lcd_brightness;
	uint32_t min_ffc_interval;
	uint32_t max_ffc_interval;
	uint32_t palette_index;
	int32_t reflected_temp;
	uint32_t timelapse_interval_sec;
	uint32_t timelapse_num_img;
} gui_state_t;


//
// Externally accessible state
//
extern gui_state_t gui_state;


//
// API
//
void gui_state_init();
void gui_state_note_item_inited(uint32_t mask);
bool gui_state_init_complete();

#endif /* GUI_STATE_H */