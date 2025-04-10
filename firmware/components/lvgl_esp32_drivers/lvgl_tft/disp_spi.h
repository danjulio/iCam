/**
 * @file disp_spi.h
 *
 */

#ifndef DISP_SPI_H
#define DISP_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <stdbool.h>
#include <driver/spi_master.h>
#include "esp_system.h"
#include "system_config.h"



/*********************
 *      DEFINES
 *********************/
 
// SPI Bus when using disp_spi_init()
#define DISP_SPI_HOST LCD_SPI_HOST
#define DISP_SPI_DMA  LCD_DMA_NUM

// Buffer size - sets maximum update region (and can use a lot of memory!)
#define DISP_BUF_SIZE (8*LV_HOR_RES_MAX)
 
// Display-specific GPIO
#define DISP_SPI_MOSI BRD_LCD_MOSI_IO
#define DISP_SPI_CLK  BRD_LCD_SCK_IO
#define DISP_SPI_CS   BRD_LCD_CSN_IO

// Display SPI frequency
#define DISP_SPI_HZ   LCD_SPI_FREQ_HZ


/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void disp_spi_init(void);
void disp_spi_add_device(spi_host_device_t host);
void disp_spi_add_device_config(spi_host_device_t host, spi_device_interface_config_t *devcfg);
void disp_spi_send_data(uint8_t * data, uint16_t length);
void disp_spi_send_colors(uint8_t * data, uint16_t length);
bool disp_spi_is_busy(void);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*DISP_SPI_H*/
