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
#include "esp_system.h"
#ifndef CONFIG_BUILD_ICAM_MINI

#include "gui_state.h"
#include "cmd_list.h"
#include "cmd_utilities.h"



//
// Externally accessible state
//
gui_state_t gui_state = { 0 };


//
// Internal variables
//
static uint32_t gui_init_mask;



//
// API
//
void gui_state_init()
{
	// Request GUI state from the controller - this has to be updated whenever gui_state_t
	// is changed
	gui_init_mask = 0;
	(void) cmd_send(CMD_GET, CMD_AMBIENT_CORRECT);
#ifdef ESP_PLATFORM
	(void) cmd_send(CMD_GET, CMD_BACKLIGHT);
#endif
	(void) cmd_send(CMD_GET, CMD_BRIGHTNESS);
	(void) cmd_send(CMD_GET, CMD_CARD_PRESENT);
	(void) cmd_send(CMD_GET, CMD_EMISSIVITY);
	(void) cmd_send(CMD_GET, CMD_GAIN);
	(void) cmd_send(CMD_GET, CMD_MIN_MAX_EN);
	(void) cmd_send(CMD_GET, CMD_PALETTE);
	(void) cmd_send(CMD_GET, CMD_REGION_EN);
	(void) cmd_send(CMD_GET, CMD_SAVE_OVL_EN);
	(void) cmd_send(CMD_GET, CMD_SPOT_EN);
	(void) cmd_send(CMD_GET, CMD_SHUTTER_INFO);
	(void) cmd_send(CMD_GET, CMD_UNITS);
#ifndef ESP_PLATFORM
	(void) cmd_send(CMD_GET, CMD_WIFI_INFO);
#endif

	// Timelapse default settings for GUI
	gui_state.timelapse_enable = false;
	gui_state.timelapse_notify = false;
	gui_state.timelapse_running = false;
	gui_state.timelapse_interval_sec = 2;
	gui_state.timelapse_num_img = 10;
}


void gui_state_note_item_inited(uint32_t mask)
{
	gui_init_mask |= mask;
}


bool gui_state_init_complete()
{
	return ((gui_init_mask & GUI_STATE_INIT_ALL_MASK) == GUI_STATE_INIT_ALL_MASK);
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
