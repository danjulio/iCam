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

#include "sdkconfig.h"
#include "video.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "soc/gpio_reg.h"
#include "soc/rtc.h"
#include "soc/soc.h"
#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/io_mux_reg.h"
#include "esp32/rom/gpio.h"
#include "esp32/rom/lldesc.h"
#include "driver/periph_ctrl.h"
#include "driver/dac.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <math.h>
#include <string.h>


//
// Constants
//

// PAL/NTSC
#define HSYNC_US 4.7
#define VSYNC_SHORT_US (HSYNC_US/2)

// Using full (approx 2.9V full-swing) DAC range
#define DAC_LEVEL_SYNC 0      // sync pulse level 0V
#define DAC_LEVEL_BLANK 73    // blank level 0.286V
#define DAC_LEVEL_BLACK 86    // black level 0.339V
#define DAC_LEVEL_P_BLANK 76  // blank level 0.3V
#define DAC_LEVEL_P_BLACK 76  // black level 0.3V
#define DAC_LEVEL_WHITE 255   // white level 1V

// PAL
#define PAL_LINE_DURATION_US 64
#define PAL_FRONT_PORCH_US 1.65
#define PAL_BACK_PORCH_US 5.7
#define PAL_TOTAL_LINES_COUNT 312
#define PAL_CONST_OFFSET_Y 11
#define PAL_CONST_OFFSET_X 0

// NTSC
#define NTSC_LINE_DURATION_US 63.55
#define NTSC_FRONT_PORCH_US 1.5
#define NTSC_BACK_PORCH_US 4.5
#define NTSC_TOTAL_LINES_COUNT 262
#define NTSC_CONST_OFFSET_Y 8
#define NTSC_CONST_OFFSET_X 0

#define US_FREQ_TO_SAMPLES(freq, time_us) (round((double)freq*time_us/1000000.0))
#define US_TO_SAMPLES(time_us) (round(((double)g_video_signal.dac_frequency*time_us/1000000.0)))
#define SAMPLES_TO_US(samples) (1000000.0 * (double)samples / (double)g_video_signal.dac_frequency)

#define DMA_BUFFER_UINT16 ((uint16_t*)((lldesc_t*)I2S0.out_eof_des_addr)->buf)
#define DMA_BUFFER_UINT8 ((uint8_t*)((lldesc_t*)I2S0.out_eof_des_addr)->buf)
#define DMA_BUFFER_UINT32 ((uint32_t*)((lldesc_t*)I2S0.out_eof_des_addr)->buf)



//
// Variables
//
static const char* TAG = "video";

static bool g_video_initialized = false;
static volatile int g_current_scan_line = 1;
static volatile uint32_t g_current_dma_buf_offset = 0;

static volatile uint16_t pixel_map_buf[256];

static intr_handle_t i2s_interrupt_handle;
static lldesc_t DRAM_ATTR dma_buffers[2] = {0};

DRAM_ATTR volatile VIDEO_SIGNAL_PARAMS g_video_signal;

static volatile uint8_t* new_fb;
static volatile bool new_fb_valid = false;

//
// Forward Declarations
//
static bool setup_video_signal(VIDEO_MODE mode, DAC_FREQUENCY dac_frequency, uint16_t width_pixels, uint16_t height_pixels, uint8_t* fb);
static bool set_dac_frequency();
static bool setup_video_dac();
static inline void pal_render_scan_line() __attribute__((always_inline));
static inline void ntsc_render_scan_line() __attribute__((always_inline));
static /*IRAM_ATTR*/ void i2s_interrupt();
static /*IRAM_ATTR*/ void signal_vertical_sync_line(VSYNC_PULSE_LENGTH first_pulse, VSYNC_PULSE_LENGTH second_pulse);
static /*IRAM_ATTR*/ inline void signal_blank_line();
static /*IRAM_ATTR*/ void signal_line_start();
static /*IRAM_ATTR*/ void render_pixels_grey_8bpp();






//
// API
//

/**
 * @brief Starts video generation and allocates all required resources.
 * 
 * To stop generating video call \a video_stop(). To change video mode just call
 * the function with new parameters.
 * 
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param mode PAL or NTSC mode
 * 
 * @see video_stop()
 */
bool video_init(uint16_t width, uint16_t height, uint8_t* fb, VIDEO_MODE mode)
{
	uint32_t max_low_freq_width;
	uint32_t max_high_freq_width;
		
	if (fb == NULL) {
		ESP_LOGE(TAG, "Frame buffer = null");
		return false;
	}
	
	if( g_video_initialized )
    {
        video_stop();
    }
    
	// Setup the video parameters
    DAC_FREQUENCY freq;
    switch(mode)
    {
        case VIDEO_MODE_PAL:
        	if (height > (PAL_TOTAL_LINES_COUNT - 8)) {
        		ESP_LOGE(TAG, "Height %u exceeds PAL limits", height);
        		return false;
        	}
        	max_low_freq_width = US_FREQ_TO_SAMPLES(DAC_FREQ_PAL_7_357MHz, PAL_LINE_DURATION_US - PAL_FRONT_PORCH_US - PAL_BACK_PORCH_US);
        	max_high_freq_width = US_FREQ_TO_SAMPLES(DAC_FREQ_PAL_14_75MHz, PAL_LINE_DURATION_US - PAL_FRONT_PORCH_US - PAL_BACK_PORCH_US);
        	if (width > max_high_freq_width) {
        		ESP_LOGE(TAG, "Width %u exceeds PAL limits", width);
        		return false;
        	} else if (width > max_low_freq_width) {
        		freq = DAC_FREQ_PAL_14_75MHz;
        	} else {
        		freq = DAC_FREQ_PAL_7_357MHz;
        	}
            break;

        case VIDEO_MODE_PAL_BT601:
        	if (height > (PAL_TOTAL_LINES_COUNT - 8)) {
        		ESP_LOGE(TAG, "Height %u exceeds PAL BT601 limits", height);
        		return false;
        	}
        	max_low_freq_width = US_FREQ_TO_SAMPLES(DAC_FREQ_PAL_NTSC_6_75MHz, PAL_LINE_DURATION_US - PAL_FRONT_PORCH_US - PAL_BACK_PORCH_US);
        	max_high_freq_width = US_FREQ_TO_SAMPLES(DAC_FREQ_PAL_NTSC_13_5MHz, PAL_LINE_DURATION_US - PAL_FRONT_PORCH_US - PAL_BACK_PORCH_US);
        	if (width > max_high_freq_width) {
        		ESP_LOGE(TAG, "Width %u exceeds PAL BT601 limits", width);
        		return false;
        	} else if (width > max_low_freq_width) {
        		freq = DAC_FREQ_PAL_NTSC_13_5MHz;
        	} else {
        		freq = DAC_FREQ_PAL_NTSC_6_75MHz;
        	}
            break;

        case VIDEO_MODE_NTSC:
        	if (height > (NTSC_TOTAL_LINES_COUNT - 10)) {
        		ESP_LOGE(TAG, "Height %u exceeds NTSC limits", height);
        		return false;
        	}
        	max_low_freq_width = US_FREQ_TO_SAMPLES(DAC_FREQ_NTSC_6_136MHz, NTSC_LINE_DURATION_US - NTSC_FRONT_PORCH_US - NTSC_BACK_PORCH_US);
        	max_high_freq_width = US_FREQ_TO_SAMPLES(DAC_FREQ_NTSC_12_273MHz, NTSC_LINE_DURATION_US - NTSC_FRONT_PORCH_US - NTSC_BACK_PORCH_US);
        	if (width > max_high_freq_width) {
        		ESP_LOGE(TAG, "Width %u exceeds NTSC limits", width);
        		return false;
        	} else if (width > max_low_freq_width) {
        		freq = DAC_FREQ_NTSC_12_273MHz;
        	} else {
        		freq = DAC_FREQ_NTSC_6_136MHz;
        	}
            break;

        case VIDEO_MODE_NTSC_BT601:
        	if (height > (NTSC_TOTAL_LINES_COUNT - 10)) {
        		ESP_LOGE(TAG, "Height %u exceeds NTSC BT601 limits", height);
        		return false;
        	}
        	max_low_freq_width = US_FREQ_TO_SAMPLES(DAC_FREQ_PAL_NTSC_6_75MHz, NTSC_LINE_DURATION_US - NTSC_FRONT_PORCH_US - NTSC_BACK_PORCH_US);
        	max_high_freq_width = US_FREQ_TO_SAMPLES(DAC_FREQ_PAL_NTSC_13_5MHz, NTSC_LINE_DURATION_US - NTSC_FRONT_PORCH_US - NTSC_BACK_PORCH_US);
        	if (width > max_high_freq_width) {
        		ESP_LOGE(TAG, "Width %u exceeds NTSC BT601 limits", width);
        		return false;
        	} else if (width > max_low_freq_width) {
        		freq = DAC_FREQ_PAL_NTSC_13_5MHz;
        	} else {
        		freq = DAC_FREQ_PAL_NTSC_6_75MHz;
        	}
            break;

        default:
            ESP_LOGE(TAG, "Illegal video mode - %d", (int) mode);
            return false;
            break;
    }

    if (setup_video_signal(mode, freq, width, height, fb)) {
	    ESP_LOGD(TAG, "Scan line duration: %d DAC samples (%.2fµs) (PAL:64µs, NTSC:63.55µs)", g_video_signal.samples_per_line, SAMPLES_TO_US(g_video_signal.samples_per_line));
	    ESP_LOGD(TAG, "HSYNC: %u samples (%.2fµs)", g_video_signal.hsync_samples,SAMPLES_TO_US(g_video_signal.hsync_samples));
	    ESP_LOGD(TAG, "VSYNC LONG: %u samples (%.2fµs)", g_video_signal.vsync_long_samples, SAMPLES_TO_US(g_video_signal.vsync_long_samples));
	    ESP_LOGD(TAG, "VSYNC SHORT: %d samples (%.2fµs)", g_video_signal.vsync_short_samples, SAMPLES_TO_US(g_video_signal.vsync_short_samples));
	
	    ESP_LOGD(TAG, "Offset X %u samples (%.2fµs)", g_video_signal.offset_x_samples, SAMPLES_TO_US(g_video_signal.offset_x_samples));
	    ESP_LOGD(TAG, "Offset Y %u lines", g_video_signal.offset_y_lines);
	    ESP_LOGD(TAG, "Front porch %u samples (%.2fµs)", g_video_signal.front_porch_samples, SAMPLES_TO_US(g_video_signal.front_porch_samples));
	    ESP_LOGD(TAG, "Back porch %u samples (%.2fµs)", g_video_signal.back_porch_samples, SAMPLES_TO_US(g_video_signal.back_porch_samples));
    } else {
    	return false;
    }

    // Create the 8bpp mapping array to the video DMA 16-bit data (maps 8-bit values to
    // the video range and puts the data in the top of 16-bit words)
    uint16_t p;
    for (int i=0; i<256; i++) {
    	p = round((float) i*(DAC_LEVEL_WHITE - g_video_signal.dac_level_black)/255.0) + g_video_signal.dac_level_black;
    	pixel_map_buf[i] = p << 8;
    }

	if (setup_video_dac()) {
    	ESP_LOGI(TAG,"DAC frequency: %lu Hz", (uint32_t)g_video_signal.dac_frequency);
    	ESP_LOGD(TAG,"DAC SYNC  level: %u", DAC_LEVEL_SYNC);
    	ESP_LOGD(TAG,"DAC BLANK level: %u", g_video_signal.dac_level_blank);
    	ESP_LOGD(TAG,"DAC BLACK level: %u", g_video_signal.dac_level_black);
    	ESP_LOGD(TAG,"DAC WHITE level: %u", DAC_LEVEL_WHITE);

    	g_video_initialized = true;
    } else {
    	return false;
    }
    
    return true;
}


void video_set_alt_fb(uint8_t* fb)
{
	new_fb = fb;
	new_fb_valid = true;
}



/**
 * @brief Get the mode description, e.g. "NTSC 320x200"
 * 
 * @param buffer pointer to buffer where to store description
 * @param buffer_size size of \a buffer. It should be minimum \b 14 bytes.
 */
void video_get_mode_description(char* buffer, size_t buffer_size)
{
    const char* mode_name;

    switch(g_video_signal.video_mode)
    {
        case VIDEO_MODE_PAL:
            mode_name = "PAL";
            break;

        case VIDEO_MODE_NTSC:
            mode_name = "NTSC";
            break;

        case VIDEO_MODE_PAL_BT601:
            mode_name = "PAL'";
            break;       

        case VIDEO_MODE_NTSC_BT601:
            mode_name = "NTSC'";
            break; 

        default:
            mode_name = "";
            break;
    }

    snprintf(buffer, buffer_size, "%s %ux%u", mode_name, g_video_signal.width_pixels, g_video_signal.height_pixels);
}


/**
    @brief Stops video generation and frees resources
*/ 
void video_stop()
{
	ESP_LOGD(TAG, "Video output stop");
    if( !g_video_initialized ) return;

    // disable interrupt
    ESP_LOGD(TAG, "Disable I²S interrupt");
    ESP_ERROR_CHECK(esp_intr_disable(i2s_interrupt_handle));
    ESP_ERROR_CHECK(esp_intr_free(i2s_interrupt_handle));

    // stop DAC
    I2S0.out_link.start = 0;

    //disable i2s DAC
    ESP_LOGD(TAG, "Disable DAC");
    dac_i2s_disable();
    dac_output_disable(DAC_CHAN_1);
    
    // free DMA buffers 
    const size_t DMA_BUFFER_COUNT = sizeof(dma_buffers)/sizeof(lldesc_t);
    for(size_t i=0;i<DMA_BUFFER_COUNT;i++)
    {
        if( dma_buffers[i].buf )
        {
            ESP_LOGD(TAG, "Free DMA buffers");
            heap_caps_free((uint8_t*)dma_buffers[i].buf);
            dma_buffers[i].buf=NULL;
        }
    }

    // disable I2S
    ESP_LOGD(TAG, "Disable I²S module");
    periph_module_disable(PERIPH_I2S0_MODULE);

    ESP_LOGI(TAG, "Video generation stopped.");

    g_video_initialized = false;
}



//
// Internal functions
//
static bool setup_video_signal(VIDEO_MODE mode, DAC_FREQUENCY dac_frequency, uint16_t width_pixels, uint16_t height_pixels, uint8_t* fb)
{
	g_video_signal.dac_frequency = (uint32_t)dac_frequency;

    if( mode == VIDEO_MODE_PAL || mode == VIDEO_MODE_PAL_BT601 )
    {
        g_video_signal.samples_per_line = US_TO_SAMPLES(PAL_LINE_DURATION_US);
        g_video_signal.front_porch_samples = US_TO_SAMPLES(PAL_FRONT_PORCH_US);
        g_video_signal.back_porch_samples = US_TO_SAMPLES(PAL_BACK_PORCH_US);
        g_video_signal.offset_x_samples = PAL_CONST_OFFSET_X;
        g_video_signal.offset_y_lines = PAL_CONST_OFFSET_Y + PAL_TOTAL_LINES_COUNT/2 - height_pixels/2;
        g_video_signal.number_of_lines = PAL_TOTAL_LINES_COUNT;
        g_video_signal.dac_level_blank = DAC_LEVEL_P_BLANK;
        g_video_signal.dac_level_black = DAC_LEVEL_P_BLACK;
    }
    else //NTSC
    {
        g_video_signal.samples_per_line = US_TO_SAMPLES(NTSC_LINE_DURATION_US);
        g_video_signal.front_porch_samples = US_TO_SAMPLES(NTSC_FRONT_PORCH_US);
        g_video_signal.back_porch_samples = US_TO_SAMPLES(NTSC_BACK_PORCH_US);
        g_video_signal.offset_x_samples = NTSC_CONST_OFFSET_X;
        g_video_signal.offset_y_lines = NTSC_CONST_OFFSET_Y + NTSC_TOTAL_LINES_COUNT/2 - height_pixels/2;
        g_video_signal.number_of_lines = NTSC_TOTAL_LINES_COUNT;
        g_video_signal.dac_level_blank = DAC_LEVEL_BLANK;
        g_video_signal.dac_level_black = DAC_LEVEL_BLACK;
    }
    
    g_video_signal.samples_per_line &=~1; //must be even
    g_video_signal.hsync_samples = US_TO_SAMPLES(HSYNC_US);
    g_video_signal.vsync_short_samples = US_TO_SAMPLES(VSYNC_SHORT_US);
    g_video_signal.vsync_long_samples = g_video_signal.samples_per_line/2 - g_video_signal.hsync_samples;

    g_video_signal.width_pixels = width_pixels;
    g_video_signal.height_pixels = height_pixels;
    g_video_signal.offset_x_samples +=
            g_video_signal.back_porch_samples + 
            g_video_signal.hsync_samples +

           (g_video_signal.samples_per_line-
            g_video_signal.front_porch_samples-
            g_video_signal.back_porch_samples-
            g_video_signal.hsync_samples)
            /2 -
            (width_pixels/2);
    
    // Find the maximum number of lines we can put in a DMA buffer
    size_t line_num_bytes = g_video_signal.samples_per_line*sizeof(uint16_t);
    g_video_signal.num_lines_per_dma_buf = (4092 / line_num_bytes);
    ESP_LOGD(TAG, "Bytes per line: %d, Lines per DMA buffer: %u", line_num_bytes, g_video_signal.num_lines_per_dma_buf);
    
    g_video_signal.video_mode = mode;
    g_video_signal.bits_per_pixel = 8;
    g_video_signal.pixel_render_func = render_pixels_grey_8bpp;
    g_video_signal.frame_buffer_size_bytes = width_pixels*height_pixels;
    g_video_signal.frame_buffer = fb;
    ESP_LOGD(TAG, "Bits per pixel: %u, %ux%u. FB size %lu bytes ", g_video_signal.bits_per_pixel, g_video_signal.width_pixels, g_video_signal.height_pixels, g_video_signal.frame_buffer_size_bytes);
    
	return true;
}


static bool set_dac_frequency()
{
    switch(g_video_signal.dac_frequency)
    {
        case DAC_FREQ_PAL_14_75MHz:
        	rtc_clk_apll_enable(true);
        	rtc_clk_apll_coeff_set(2, 0xCD, 0xCC, 0x07); //= 14.750004 MHz
            ESP_LOGI(TAG, "DAC clock configured to 14.75 MHz. PAL 640 pixels.");
            break;

        case DAC_FREQ_PAL_7_357MHz:
        	rtc_clk_apll_enable(true);
        	rtc_clk_apll_coeff_set(6, 0xCD, 0xCC, 0x07); //= 7.375002 MHz
            ESP_LOGI(TAG, "DAC clock configured to 7.35 MHz. PAL 320 pixels.");
            break;

        case DAC_FREQ_NTSC_12_273MHz: //=12272720
        	rtc_clk_apll_enable(true);
        	rtc_clk_apll_coeff_set(3, 209, 69, 8);
            ESP_LOGI(TAG, "DAC clock configured to 12.273 MHz. NTSC 640 pixels.");
            break;

        case DAC_FREQ_NTSC_6_136MHz: //=6.136360
        	rtc_clk_apll_enable(true);
        	rtc_clk_apll_coeff_set(8, 209, 69, 8);
            ESP_LOGI(TAG, "DAC clock configured to 6.136 MHz. NTSC 320 pixels.");
            break;

        case DAC_FREQ_PAL_NTSC_13_5MHz: //=13500001
        	rtc_clk_apll_enable(true);
        	rtc_clk_apll_coeff_set(7, 05, 76, 20);
            ESP_LOGI(TAG, "DAC clock configured to 13.5 MHz. BT.601 PAL/NTSC 640 pixels.");
            break;

        case DAC_FREQ_PAL_NTSC_6_75MHz: // =6.750000
        	rtc_clk_apll_enable(true);
        	rtc_clk_apll_coeff_set(16, 205, 76, 20);
            ESP_LOGI(TAG, "DAC clock configured to 6.75 MHz. BT.601 PAL/NTSC 320 pixels.");
            break;

        default:
            ESP_LOGE(TAG, "Not supported DAC frequency");
            return false;
            break;
    }
    
    return true;
}


static bool setup_video_dac()
{
	esp_err_t ret;
	
	ESP_LOGD(TAG, "DAC setup");

    const size_t dma_buffer_size_bytes = g_video_signal.num_lines_per_dma_buf * g_video_signal.samples_per_line * sizeof(uint16_t);
    ESP_LOGD(TAG, "Computed DMA buffer size %u for %u lines", dma_buffer_size_bytes, g_video_signal.num_lines_per_dma_buf);

    periph_module_enable(PERIPH_I2S0_MODULE);
    if ((ret = esp_intr_alloc(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, i2s_interrupt, NULL, &i2s_interrupt_handle)) != ESP_OK) {
    	ESP_LOGE(TAG, "Interrupt allocation failed - %d", ret);
    	return false;
    }
    ESP_LOGD(TAG, "I²S interrupt configured");

    // reset conf
    I2S0.conf.val = 1;
    I2S0.conf.val = 0;
    I2S0.conf.tx_right_first = 1;
    I2S0.conf.tx_mono = 1;

    I2S0.conf2.lcd_en = 1;
    I2S0.fifo_conf.tx_fifo_mod_force_en = 1;
    I2S0.sample_rate_conf.tx_bits_mod = 16;     // DAC uses MSB 8 bits of 16
    I2S0.conf_chan.tx_chan_mod = 1;             // 16-bit single channel data

    I2S0.clkm_conf.clkm_div_num = 1;            // I2S clock divider's integral value.
    I2S0.clkm_conf.clkm_div_b = 0;              // Fractional clock divider's numerator value.
    I2S0.clkm_conf.clkm_div_a = 1;              // Fractional clock divider's denominator value
    I2S0.sample_rate_conf.tx_bck_div_num = 1;
    I2S0.clkm_conf.clka_en = 1;                 // use clk_apll clock
    I2S0.fifo_conf.tx_fifo_mod = 1;             // 16-bit single channel data

	const size_t DMA_BUFFER_COUNT = sizeof(dma_buffers)/sizeof(lldesc_t);
    for (size_t n=0; n<DMA_BUFFER_COUNT; n++)
	{
        ESP_LOGD(TAG, "Allocating DMA buffer: %u bytes", dma_buffer_size_bytes);
        dma_buffers[n].buf = (uint8_t*)heap_caps_calloc(dma_buffer_size_bytes, sizeof(uint8_t), MALLOC_CAP_DMA);
		assert(dma_buffers[n].buf != NULL);
        dma_buffers[n].owner = 1;
        dma_buffers[n].eof = 1;
        dma_buffers[n].length = dma_buffer_size_bytes;
        dma_buffers[n].size = dma_buffer_size_bytes;
        dma_buffers[n].empty = (uint32_t)(n==DMA_BUFFER_COUNT-1? &dma_buffers[0] : &dma_buffers[n+1]);
    }
    I2S0.out_link.addr = (uint32_t)&dma_buffers[0];
    ESP_LOGI(TAG, "DMA buffers configured. Buffers: %u, Size: %u bytes each", DMA_BUFFER_COUNT, dma_buffer_size_bytes);

    if (!set_dac_frequency()) {
    	return false;
    }

	if ((ret = dac_output_enable(DAC_CHAN_1)) != ESP_OK) {
		ESP_LOGE(TAG, "DAC Output enabled failed - %d", ret);
		return false;
	}
    ESP_LOGI(TAG, "DAC output on GPIO26 (DAC_CHAN_1)");

	if ((ret = dac_i2s_enable()) != ESP_OK) {
		ESP_LOGI(TAG, "DAC I2S Enable failed - %d", ret);
		return false;
	}
    ESP_LOGD(TAG, "DAC I²S enabled");

    // start transmission
    I2S0.conf.tx_start = 1;
    I2S0.int_clr.val = UINT32_MAX;
    I2S0.int_ena.out_eof = 1;
    I2S0.out_link.start = 1;

    if ((ret = esp_intr_enable(i2s_interrupt_handle)) != ESP_OK) {
    	ESP_LOGE(TAG, "Enable I2S Interrupt failed - %d", ret);
    	return false;
    }
    ESP_LOGD(TAG, "I²S interrupt enabled");

	return true;
}


static inline void pal_render_scan_line()
{
    if( g_current_scan_line <= 2) // lines 1,2
    {
        signal_vertical_sync_line(VSYNC_PULSE_LONG, VSYNC_PULSE_LONG);
    }
    else if( g_current_scan_line == 3) // line 3
    {
        signal_vertical_sync_line(VSYNC_PULSE_LONG, VSYNC_PULSE_SHORT);
    }
    else if( g_current_scan_line <= 5) // lines 4,5
    {
        signal_vertical_sync_line(VSYNC_PULSE_SHORT, VSYNC_PULSE_SHORT);
    }
    else if( g_current_scan_line < g_video_signal.offset_y_lines )
    {
        signal_blank_line();
    }
    else if (g_current_scan_line < g_video_signal.offset_y_lines+g_video_signal.height_pixels)
    {
        signal_line_start();
        g_video_signal.pixel_render_func();
    }
    else if( g_current_scan_line < g_video_signal.number_of_lines - 2 ) // line 310
    {
        signal_blank_line();
    }
    else if (g_current_scan_line <= g_video_signal.number_of_lines) // lines 310-312
    {
        signal_vertical_sync_line(VSYNC_PULSE_SHORT, VSYNC_PULSE_SHORT);
    }

    if( g_current_scan_line >= PAL_TOTAL_LINES_COUNT ) // line 312
    {
        g_current_scan_line = 1;
        if (new_fb_valid) {
        	new_fb_valid = false;
        	g_video_signal.frame_buffer = (uint8_t *) new_fb;
        }
    }
    else
    {
    	g_current_scan_line += 1;
    }
}


static inline void ntsc_render_scan_line(void)
{
    if( g_current_scan_line <= 3) // lines 1,2,3
    {
        signal_vertical_sync_line(VSYNC_PULSE_SHORT,VSYNC_PULSE_SHORT);
    }
    else if( g_current_scan_line <= 6 ) //line 4,5,6
    {
        signal_vertical_sync_line(VSYNC_PULSE_LONG,VSYNC_PULSE_LONG);
    }
    else if( g_current_scan_line <= 9) // lines 7,8,9
    {
        signal_vertical_sync_line(VSYNC_PULSE_SHORT,VSYNC_PULSE_SHORT);
    }
    else if( g_current_scan_line < g_video_signal.offset_y_lines )
    {
        signal_blank_line();
    }
    else if (g_current_scan_line < g_video_signal.offset_y_lines+g_video_signal.height_pixels)
    {
        signal_line_start();
        g_video_signal.pixel_render_func();
    }
    else if( g_current_scan_line < g_video_signal.number_of_lines  )
    {
        signal_blank_line();
    }

    if( g_current_scan_line >= g_video_signal.number_of_lines )
    {
        g_current_scan_line = 1;
        if (new_fb_valid) {
        	new_fb_valid = false;
        	g_video_signal.frame_buffer = (uint8_t *) new_fb;
        }
    }
    else
    {
    	g_current_scan_line += 1;
    }
}


static IRAM_ATTR void i2s_interrupt()
{
	int num_lines;
	
	if (I2S0.int_st.out_eof)
	{
        num_lines = g_video_signal.num_lines_per_dma_buf;
        g_current_dma_buf_offset = 0;

        if( g_video_signal.video_mode >= VIDEO_MODE_NTSC )
        	while (num_lines--) {
            	ntsc_render_scan_line();
            	g_current_dma_buf_offset += g_video_signal.samples_per_line * sizeof(uint16_t);
            }
        else
        	while (num_lines--) {
            	pal_render_scan_line();
            	g_current_dma_buf_offset += g_video_signal.samples_per_line * sizeof(uint16_t);
            }
	}

	// reset the interrupt
    I2S0.int_clr.val = I2S0.int_st.val;
}


static IRAM_ATTR void signal_vertical_sync_line(const VSYNC_PULSE_LENGTH first_pulse, const VSYNC_PULSE_LENGTH second_pulse)
{
	// Compute vsync byte lengths that are an even multiple of 4 bytes (they be close enough to the calculated hsync period).
    // We do this so vysnc always ends on a 4 byte boundary to avoid a condition due to the ESP32 little endianness the first
	// 16-bit word of the subsequent portion is output first causing a one pixel-clock glitch between portions.
    const int first_half_bytes = g_video_signal.samples_per_line & 0xFFFFFFFC;
    const int second_half_bytes = (g_video_signal.samples_per_line*sizeof(uint16_t)) - first_half_bytes;
    
    int first_pulse_width_bytes =   first_pulse == VSYNC_PULSE_LONG ? g_video_signal.vsync_long_samples*2 : g_video_signal.vsync_short_samples*2;
    first_pulse_width_bytes &= 0xFFFFFFFC;
    int second_pulse_width_bytes = second_pulse == VSYNC_PULSE_LONG ? g_video_signal.vsync_long_samples*2 : g_video_signal.vsync_short_samples*2;
    second_pulse_width_bytes &= 0xFFFFFFFC;

    memset(DMA_BUFFER_UINT8+g_current_dma_buf_offset, DAC_LEVEL_SYNC, first_pulse_width_bytes);
    memset(DMA_BUFFER_UINT8+g_current_dma_buf_offset+first_pulse_width_bytes, g_video_signal.dac_level_blank, first_half_bytes - first_pulse_width_bytes);

    memset(DMA_BUFFER_UINT8+g_current_dma_buf_offset+first_half_bytes, DAC_LEVEL_SYNC, second_pulse_width_bytes);
    memset(DMA_BUFFER_UINT8+g_current_dma_buf_offset+first_half_bytes+second_pulse_width_bytes, g_video_signal.dac_level_blank, second_half_bytes - second_pulse_width_bytes);
}


static IRAM_ATTR void signal_blank_line()
{
	// Compute an hsync byte length that is an even multiple of 4 bytes (it will be close enough to the calculated hsync period).
	// We do this so hysnc always ends on a 4 byte boundary to avoid a condition due to the ESP32 little endianness the first
	// 16-bit word of the subsequent portion is output first causing a one pixel-clock glitch between portions.
	const size_t hsync_byte_len = (g_video_signal.hsync_samples*sizeof(uint16_t)) & 0xFFFFFFFC;
    memset(DMA_BUFFER_UINT8+g_current_dma_buf_offset, DAC_LEVEL_SYNC, hsync_byte_len);
    memset(DMA_BUFFER_UINT8+g_current_dma_buf_offset+hsync_byte_len, g_video_signal.dac_level_blank, (g_video_signal.samples_per_line*sizeof(uint16_t)) - hsync_byte_len);
}


static IRAM_ATTR void signal_line_start()
{
	// Sync
	size_t offset_byte_len = (g_video_signal.hsync_samples*sizeof(uint16_t)) & 0xFFFFFFFC;
	memset(DMA_BUFFER_UINT8+g_current_dma_buf_offset, DAC_LEVEL_SYNC, offset_byte_len);
	
	// Blank before pixels
	memset(DMA_BUFFER_UINT8+g_current_dma_buf_offset+offset_byte_len, g_video_signal.dac_level_blank, (g_video_signal.offset_x_samples*sizeof(uint16_t)) - offset_byte_len);
	
	// Blank after pixels
	offset_byte_len += g_video_signal.width_pixels*sizeof(uint16_t);
	memset(DMA_BUFFER_UINT8+g_current_dma_buf_offset+offset_byte_len, g_video_signal.dac_level_blank, (g_video_signal.samples_per_line*sizeof(uint16_t)) - offset_byte_len);
}


static IRAM_ATTR void render_pixels_grey_8bpp()
{
    uint32_t* p = DMA_BUFFER_UINT32 + g_current_dma_buf_offset/4 + g_video_signal.offset_x_samples/2;
    uint8_t* s = g_video_signal.frame_buffer + (g_current_scan_line-g_video_signal.offset_y_lines)*g_video_signal.width_pixels;
    size_t len = g_video_signal.width_pixels;
    uint32_t d;
	
    while (len)
    {
    	// ESP32 little-endian requires us to swap 16-bit words into 32-bit word
    	// Compiler doesn't like us doing this in one line of code so we use 'd'
    	d = pixel_map_buf[*s++] << 16;
    	*p++ = d | (pixel_map_buf[*s++]);
    	len -= 2;
    }

}
