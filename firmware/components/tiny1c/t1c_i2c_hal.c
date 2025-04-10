/*
 * Hardware abstraction layer for Tiny1C CCI Interface over I2C.
 *
 * Copyright 2023-2024 Dan Julio
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "t1c_i2c_hal.h"
#include "data_rw.h"
#include "i2cs.h"
#include "system_config.h"


// Static buffer sized for maximum size (from read)
#define I2C_BUF_LEN I2C_VD_BUFFER_DATA_LEN


static uint8_t i2c_buf[I2C_BUF_LEN];


void HAL_Delay(int n)
{
	vTaskDelay(pdMS_TO_TICKS(n));
}


HAL_StatusTypeDef HAL_I2C_Init()
{
	// I2C bus initialized at startup by sys_utilities
	return HAL_OK;
}


HAL_StatusTypeDef HAL_I2C_Mem_Write(uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
	esp_err_t ret;
	
	// Copy Register Address + data to our buffer
	i2c_buf[0] = MemAddress >> 8;
	i2c_buf[1] = MemAddress & 0xFF;
	for (int i=0; i<Size; i++) {
		i2c_buf[i+2] = *pData++;
	}
	
	// Get the bus
	i2c_sensor_lock();
	
	// Write
	ret = i2c_sensor_write_slave((uint8_t) DevAddress >> 1, i2c_buf, (size_t) Size + 2);
	
	// Release the bus
	i2c_sensor_unlock();
	
	return((ret == ESP_OK) ? HAL_OK : HAL_ERROR);
}


HAL_StatusTypeDef HAL_I2C_Mem_Read(uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
	esp_err_t ret;
	
	// Copy Register address to our buffer
	i2c_buf[0] = MemAddress >> 8;
	i2c_buf[1] = MemAddress & 0xFF;
	
	// Get the bus
	i2c_sensor_lock();
	
	// Write the register address
	ret = i2c_sensor_write_slave((uint8_t) DevAddress >> 1, i2c_buf, 2);
	
	// Read data
	if (ret == ESP_OK) {
		ret = i2c_sensor_read_slave((uint8_t) DevAddress >> 1, i2c_buf, (size_t) Size);
	}
	
	// Release the bus
	i2c_sensor_unlock();
	
	// Copy the data back to the user's buffer
	for (int i=0; i<Size; i++) {
		*pData++ = i2c_buf[i];
	}
	
	return((ret == ESP_OK) ? HAL_OK : HAL_ERROR);
}
