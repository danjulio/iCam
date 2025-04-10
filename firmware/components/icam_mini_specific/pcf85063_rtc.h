/*
 * Simple NXP PCF85063 RTC driver
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
#ifndef PCF85063_DRIVER_H
#define PCF85063_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>



//
// API
//
bool pcf85063_init();
bool pcf85063_get_time(struct tm* te);
bool pcf85063_set_time(struct tm* te);

#endif /* PCF85063_DRIVER_H */