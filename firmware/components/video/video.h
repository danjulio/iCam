/*
ESP32 Composite Video Library
Copyright (C) 2022 aquaticus
Simplified, optimized and ported to IDF 5.3 by Dan Julio

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "sdkconfig.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Video modes.
 * 
 */
typedef enum _VIDEO_MODE
{
    VIDEO_MODE_PAL, ///< PAL, typically Europe 50 frames/second, max 625 scan lines. 14.75MHz or 7.375 MHz.
    VIDEO_MODE_PAL_BT601, ///< As \c VIDEO_MODE_PAL but using 13.5 or 6.75 MHz.
    // put mode PAL modes here

    VIDEO_MODE_NTSC, ///< NTSC, typically USA and Japan, 60 frames/second, max 525 scan lines. 12.273 or or 6.136 MHz.
    VIDEO_MODE_NTSC_BT601, ///< As \c VIDEO_MODE_NTSC but using 13.5 or 6.75 MHz.
    // put more NTSC modes here
} VIDEO_MODE;

typedef enum _VSYNC_PULSE_LENGTH
{
    VSYNC_PULSE_SHORT,
    VSYNC_PULSE_LONG
} VSYNC_PULSE_LENGTH;

typedef enum _DAC_FREQUENCY
{
    DAC_FREQ_PAL_14_75MHz=14750004, // 14.75 MHz PAL square pixels 640 horizontal
    DAC_FREQ_PAL_7_357MHz=7375002, // 7.375 MHz PAL square pixels 320 horizontal
    DAC_FREQ_NTSC_12_273MHz=12272720, //12.273 MHz NTSC 640 pixels
    DAC_FREQ_NTSC_6_136MHz=6136360, // 6.136 MHz NTSC 320 pixels
    DAC_FREQ_PAL_NTSC_13_5MHz=13500001, // 13.5 MHz BT.601 640 pixels
    DAC_FREQ_PAL_NTSC_6_75MHz=6750000 // 6.75 MHz BT.601 320 pixels
} DAC_FREQUENCY;

typedef void (*p_pixel_render_func)(void);

typedef struct _VIDEO_SIGNAL_PARAMS
{
    VIDEO_MODE video_mode;
    uint16_t width_pixels;
    uint16_t height_pixels;
    uint16_t offset_x_samples;
    uint16_t offset_y_lines;
    uint16_t hsync_samples;
    uint16_t vsync_long_samples;
    uint16_t vsync_short_samples;
    uint16_t samples_per_line;
    uint16_t front_porch_samples;
    uint16_t back_porch_samples;
    uint16_t number_of_lines;
    uint8_t  num_lines_per_dma_buf;
    uint32_t dac_frequency;
    uint8_t* frame_buffer;
    uint8_t  bits_per_pixel;
    uint32_t frame_buffer_size_bytes;
    uint8_t dac_level_blank;
    uint8_t dac_level_black;
    void (*pixel_render_func)(void);

} VIDEO_SIGNAL_PARAMS;


extern volatile VIDEO_SIGNAL_PARAMS g_video_signal;

bool video_init(uint16_t width, uint16_t height, uint8_t* fb, VIDEO_MODE mode);
void video_set_alt_fb(uint8_t* fb);
void video_get_mode_description(char* buffer, size_t buffer_size);
void video_stop();