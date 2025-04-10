/*
 * Command definition for packets sent over the websocket interface.
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
#ifndef CMD_LIST_H
#define CMD_LIST_H

#include <stdbool.h>
#include <stdint.h>



//
// Global command typedef (all possible commands for all implementations)
//

// Command list (alphabetical order, starting with a value of 0)
typedef enum {
    CMD_AMBIENT_CORRECT = 0,
	CMD_BACKLIGHT,
	CMD_BATT_LEVEL,
	CMD_BRIGHTNESS,
	CMD_CARD_PRESENT,
	CMD_CRIT_BATT,
	CMD_CTRL_ACTIVITY,
	CMD_EMISSIVITY,
	CMD_FFC,
	CMD_FILE_CATALOG,
	CMD_FILE_DELETE,
	CMD_FILE_GET_IMAGE,
	CMD_FW_UPD_EN,
	CMD_FW_UPD_END,
	CMD_GAIN,
	CMD_IMAGE,
	CMD_TIME,
	CMD_TIMELAPSE_CFG,
	CMD_TIMELAPSE_STATUS,
	CMD_MIN_MAX_EN,
	CMD_MSG_ON,
	CMD_MSG_OFF,
	CMD_ORIENTATION,
	CMD_PALETTE,
	CMD_POWEROFF,
	CMD_REGION_EN,
	CMD_REGION_LOC,
	CMD_SAVE_BACKLIGHT,
	CMD_SAVE_OVL_EN,
	CMD_SAVE_PALETTE,
	CMD_SHUTDOWN,
	CMD_SHUTTER_INFO,
	CMD_SPOT_EN,
	CMD_SPOT_LOC,
	CMD_STREAM_EN,
	CMD_SYS_INFO,
	CMD_TAKE_PICTURE,
	CMD_UNITS,
	CMD_WIFI_INFO
} cmd_id_t;

// Total Count should always use the last entry
#define CMD_TOTAL_COUNT   ((uint32_t) CMD_WIFI_INFO + 1)


// Controller Activities (sent with CMD_CTRL_ACTIVITY)
enum cmd_ctrl_act_param
{
	CMD_CTRL_ACT_RESTORE = 0,
	CMD_CTRL_ACT_TINY1C_CAL_1,
	CMD_CTRL_ACT_TINY1C_CAL_2L,
	CMD_CTRL_ACT_TINY1C_CAL_2H,
	CMD_CTRL_ACT_SD_FORMAT
};


#endif /* CMD_LIST_H */
