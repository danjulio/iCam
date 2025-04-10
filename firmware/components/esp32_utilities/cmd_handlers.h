/*
 * Command handlers updating or retrieving application state
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
#ifndef CMD_HANDLERS_H
#define CMD_HANLDERS_H

#include "cmd_utilities.h"
#include <stdbool.h>
#include <stdint.h>



//
// API
//
void cmd_handler_get_ambient_correct(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_backlight(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_batt_level(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_brightness(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_card_present(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_emissivity(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_file_catalog(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_file_image(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_gain(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_min_max_enable(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_palette(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_region_enable(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_save_ovl_en(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_shutter(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_spot_enable(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_sys_info(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_time(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_units(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_get_wifi(cmd_data_t data_type, uint32_t len, uint8_t* data);

void cmd_handler_set_ambient_correct(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_backlight(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_brightness(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_ctrl_activity(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_emissivity(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_ffc(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_file_delete(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_gain(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_min_max_enable(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_palette(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_poweroff(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_save_backlight(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_save_ovl_en(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_orientation(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_save_palette(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_region_enable(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_region_location(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_shutter(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_spot_enable(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_spot_location(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_stream_enable(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_take_picture(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_time(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_timelapse_cfg(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_units(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_wifi(cmd_data_t data_type, uint32_t len, uint8_t* data);

bool cmd_handler_stream_enabled();
bool cmd_handler_take_picture_notification();

#endif /* CMD_HANDLERS_H */
