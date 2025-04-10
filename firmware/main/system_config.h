/*
 * System Configuration File
 *
 * Contains hardware-specific configuration and system configurable items.
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
 */
#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "esp_system.h"



// ======================================================================================
// System debug
//

// Undefine to include the system monitoring task (included only for debugging/tuning)
//#define INCLUDE_SYS_MON



// ======================================================================================
// System hardware definitions
//

#ifdef CONFIG_BUILD_ICAM_MINI

//
// iCamMini IO Pins
//
#define BRD_OUT_MODE_IO        4

#define BRD_PWR_HOLD_IO        2

#define BRD_BATT_SNS_IO       37

#define BRD_BTN1_IO           35
#define BRD_BTN2_IO           38

#define BRD_GREEN_LED_IO      32
#define BRD_RED_LED_IO        33

#define BRD_SD_CSN_IO         19
#define BRD_SD_SCK_IO          8
#define BRD_SD_MOSI_IO         5
#define BRD_SD_MISO_IO         7
#define BRD_SD_SNS_IO         36

#define BRD_T1C_CSN_IO        15
#define BRD_T1C_SCK_IO        14
#define BRD_T1C_MOSI_IO       13
#define BRD_T1C_MISO_IO       12

#define BRD_SENSOR_RSTN_IO    20

#define BRD_I2C_SENSOR_SDA_IO 21
#define BRD_I2C_SENSOR_SCL_IO 22

#define BRD_AUX_TX_IO         25
#define BRD_AUX_RX_IO         39
#define BRD_AUX_RTS_IO        27
#define BRD_AUX_CTS_IO        34

#define BRD_VID_OUT_IO        26

#define BRD_DIAG_IO           0

// Unused but need to be defined
#define BRD_LCD_SCK_IO        -1
#define BRD_LCD_CSN_IO        -1
#define BRD_LCD_DC_IO         -1
#define BRD_LCD_MOSI_IO       -1

#define LCD_SPI_HOST    VSPI_HOST
#define LCD_DMA_NUM     1
#define LCD_SPI_FREQ_HZ 80000000


//
// iCamMini Hardware Configuration
//

// I2C
#define I2C_SENSOR_NUM     1
#define I2C_SENSOR_FREQ_HZ 400000

// SPI
//   Tiny1C uses HSPI
#define T1C_SPI_HOST    HSPI_HOST
#define T1C_DMA_NUM     1
#define T1C_SPI_FREQ_HZ 26670000

//   SD Card uses VSPI
#define SD_SPI_HOST     VSPI_HOST
#define SD_DMA_NUM      2
#define SD_SPI_FREQ_HZ  20000000

// Battery Monitor ADC input
#define BATT_SNS_SCALE_FACTOR 2

// ESP32 ADC Attenuation value to support maximum ADC input voltage
#define BATT_ADC_ATTEN        ADC_ATTEN_DB_12

// Approximate DC offset added to account for loss across switching transistors between
// battery and sense input (measured on prototype unit)
#define BATT_OFFSET_MV        20

// Filesystem utilities configuration
//  Using SPI
#define FILE_USE_SPI_IF
#define FILE_SDSPI_HOST         SD_SPI_HOST
#define FILE_SDSPI_DMA          SD_DMA_NUM
#define FILE_SDSPI_CS           BRD_SD_CSN_IO
#define FILE_SDSPI_CLK          BRD_SD_SCK_IO
#define FILE_SDSPI_MOSI         BRD_SD_MOSI_IO
#define FILE_SDSPI_MISO         BRD_SD_MISO_IO
#define FILE_SD_CLK_HZ          SD_SPI_FREQ_HZ

#else /* CONFIG_BUILD_ICAM_MINI */

//
// iCam IO Pins
//

#define BRD_LCD_SCK_IO        18
#define BRD_LCD_CSN_IO        5
#define BRD_LCD_DC_IO         27
#define BRD_LCD_MOSI_IO       23

#define BRD_T1C_CSN_IO        26
#define BRD_T1C_SCK_IO        25
#define BRD_T1C_MOSI_IO       0
#define BRD_T1C_MISO_IO       34

#define BRD_SENSOR_RSTN_IO    19

#define BRD_I2C_GCORE_SDA_IO  21
#define BRD_I2C_GCORE_SCL_IO  22
#define BRD_I2C_SENSOR_SDA_IO 33
#define BRD_I2C_SENSOR_SCL_IO 32

#define BRD_BTN1_IO           35
#define BRD_BTN2_IO           36


//
// iCam Hardware Configuration
//

// I2C Bus allocation
//  I2C Bus 1 - gCore on-board (EFM8 and FT6236 touchscreen controller)
//  I2C Bus 2 - Sensor bus (Tiny1C, distance, temp/humidity)
#define I2C_GCORE_NUM      0
#define I2C_GCORE_FREQ_HZ  100000
#define I2C_SENSOR_NUM     1
#define I2C_SENSOR_FREQ_HZ 400000

// SPI
//   LCD uses VSPI (no MISO)
//   Tiny1C uses HSPI
#define LCD_SPI_HOST    VSPI_HOST
#define LCD_DMA_NUM     1
#define LCD_SPI_FREQ_HZ 80000000
#define T1C_SPI_HOST    HSPI_HOST
#define T1C_DMA_NUM     2
#define T1C_SPI_FREQ_HZ 26670000

// Filesystem utilities configuration
//   Using SDMMC interface
#define FILE_SD_CLK_HZ  20000000

#endif /* CONFIG_BUILD_ICAM_MINI */



// ======================================================================================
// System configuration
//

// Battery state-of-charge curve for a pair of 2600 mAh 3.65 (3.7) volt batteries from
// BatterySpace (Powerizer 3.65V/5.2Ah) based on load testing with ~470 mA load
// and estimated 30 minute runtime before shutoff at 3.5V.
// (??? redo for something like 1000-1200 mAh battery)
//
#define BATT_75_THRESHOLD_MV    3890
#define BATT_50_THRESHOLD_MV    3700
#define BATT_25_THRESHOLD_MV    3580
#define BATT_0_THRESHOLD_MV     3525
#define BATT_CRIT_THRESHOLD_MV  3500

// Low battery indication interval
#define LOW_BATT_INDICATION_SEC 30

// Critical Battery Shutdown timeout
#define CRIT_BATTERY_OFF_SEC    30

// Filesystem Information Structure buffer (catalog)
//   Holds records for directories (36 bytes/each) and files in those directories
//   (32 bytes/each).  This buffer should be sized larger than the most files and
//   directories ever expected to be seen by the system.
#define FILE_INFO_BUFFER_LEN   (1024 * 512)

// Maximum number of image files per sub-directory
#define FILE_MAX_FILES_PER_DIR 100

// Maximum number of image directories
#define FILE_MAX_DIRS          100

// Maximum number of names stored in a comma separated catalog listing
// This number should be the larger of FILES_PER_DIR or DIRS
#define FILE_MAX_CATALOG_NAMES 100

#endif // SYSTEM_CONFIG_H