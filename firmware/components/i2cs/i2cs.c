/*
 * I2C Module
 *
 * Provides I2C Sensor Access routines for other modules/tasks.  Provides a locking mechanism
 * since the underlying ESP IDF routines are not thread safe.
 *
 * Copyright 2020-024 Dan Julio
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
#include "system_config.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i2cs.h"


//
// I2C variables
//
static i2c_master_bus_handle_t bus_handle;
static SemaphoreHandle_t i2c_mutex;



//
// I2C API
//

/**
 * i2c master initialization
 */
esp_err_t i2c_sensor_init(int scl_pin, int sda_pin)
{
	i2c_master_bus_config_t i2c_mst_config;

	// Create a mutex for thread safety
    i2c_mutex = xSemaphoreCreateMutex();
    
    // Configure the I2C bus
    i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port = I2C_SENSOR_NUM;
    i2c_mst_config.scl_io_num = scl_pin;
    i2c_mst_config.sda_io_num = sda_pin;
    i2c_mst_config.glitch_ignore_cnt = 7;
    i2c_mst_config.intr_priority = 1;
    i2c_mst_config.trans_queue_depth = 0;
    i2c_mst_config.flags.enable_internal_pullup = true;
    
	return i2c_new_master_bus(&i2c_mst_config, &bus_handle);
}


/**
 * i2c master lock
 */
void i2c_sensor_lock()
{
	xSemaphoreTake(i2c_mutex, portMAX_DELAY);
}


/**
 * i2c master unlock
 */
void i2c_sensor_unlock()
{
	xSemaphoreGive(i2c_mutex);
}


/**
 * Read esp-i2c-slave
 *
 * _______________________________________________________________________________________
 * | start | slave_addr + rd_bit +ack | read n-1 bytes + ack | read 1 byte + nack | stop |
 * --------|--------------------------|----------------------|--------------------|------|
 *
 */
esp_err_t i2c_sensor_read_slave(uint8_t addr7, uint8_t *data_rd, size_t size)
{
	i2c_device_config_t dev_cfg;
	i2c_master_dev_handle_t dev_handle;
	
    if (size == 0) {
        return ESP_OK;
    }
    
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = (uint16_t) addr7;
    dev_cfg.scl_speed_hz = I2C_SENSOR_FREQ_HZ;
    
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
    	return ret;
    }
    
    ret = i2c_master_receive(dev_handle, data_rd, size, 1000);
    
    i2c_master_bus_rm_device(dev_handle);
    
    return ret;
}


/**
 * Write esp-i2c-slave
 *
 * ___________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write n bytes + ack  | stop |
 * --------|---------------------------|----------------------|------|
 *
 */
esp_err_t i2c_sensor_write_slave(uint8_t addr7, uint8_t *data_wr, size_t size)
{
    i2c_device_config_t dev_cfg;
	i2c_master_dev_handle_t dev_handle;
	
	dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = (uint16_t) addr7;
    dev_cfg.scl_speed_hz = I2C_SENSOR_FREQ_HZ;
    
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
    	return ret;
    }
    
    ret = i2c_master_transmit(dev_handle, data_wr, size, 1000);
    
    i2c_master_bus_rm_device(dev_handle);
    
    return ret;
}
