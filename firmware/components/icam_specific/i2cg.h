/*
 * I2C Module
 *
 * Provides gCore I2C Access routines for other modules/tasks.  Provides a locking mechanism
 * since the underlying ESP IDF routines are not thread safe.
 *
 * Copyright 2020-2022 Dan Julio
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
#ifndef I2CG_H
#define I2CG_H

#include <stdint.h>
#include "esp_system.h"



//
// I2C API
//
esp_err_t i2c_gcore_init(int scl_pin, int sda_pin);
void i2c_gcore_lock();
void i2c_gcore_unlock();
esp_err_t i2c_gcore_read_slave(uint8_t addr7, uint8_t *data_rd, size_t size);
esp_err_t i2c_gcore_write_slave(uint8_t addr7, uint8_t *data_wr, size_t size);


#endif /* I2CG_H */