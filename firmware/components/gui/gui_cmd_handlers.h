/*
 * Command handlers updating GUI state in the web browser (iCamMini) or on gCore (iCam).
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
#ifndef GUI_CMD_HANDLERS_H
#define GUI_CMD_HANDLERS_H

#include "cmd_utilities.h"
#include <stdbool.h>
#include <stdint.h>



//
// API
//
void cmd_handler_set_critical_batt(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_image(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_msg_on(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_msg_off(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_set_timelapse_status(cmd_data_t data_type, uint32_t len, uint8_t* data);

void cmd_handler_rsp_ambient_correct(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_backlight(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_batt_info(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_brightness(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_card_present(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_ctrl_activity(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_emissivity(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_file_catalog(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_file_image(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_gain(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_min_max_en(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_palette(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_region_enable(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_save_ovl_en(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_shutter(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_spot_enable(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_sys_info(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_time(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_units(cmd_data_t data_type, uint32_t len, uint8_t* data);
void cmd_handler_rsp_wifi(cmd_data_t data_type, uint32_t len, uint8_t* data);

#endif /* GUI_CMD_HANDLERS_H */
