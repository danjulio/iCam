/*
 * WiFi related utilities
 *
 * Contains functions to initialize and query the wifi interface.
 *
 * Copyright 2020-2024 Dan Julio
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
 *
 */
#ifndef WIFI_UTILITIES_H
#define WIFI_UTILITIES_H

#include <stdbool.h>
#include <stdint.h>



//
// WiFi Utilities Constants
//

// Maximum attempts to reconnect to an AP in client mode before starting to wait
#define WIFI_FAST_RECONNECT_ATTEMPTS  10



//
// WiFi Utilities API
//
bool wifi_init();
bool wifi_reinit();
bool wifi_en_mdns(bool en);
bool wifi_is_enabled();
bool wifi_is_connected();
void wifi_get_ipv4_addr(char* s);   // s must be large enough for "XXX.XXX.XXX.XXX" + null

#endif /* WIFI_UTILITIES_H */
