/*
 * iCam and iCamMini main file for ESP32.  Depending on build type calls the
 * the camera-specific main entry point.
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
#include "esp_system.h"
#ifdef CONFIG_BUILD_ICAM_MINI
	#include "icam_mini_main.h"
#else
	#include "icam_main.h"
#endif



void app_main(void)
{
#ifdef CONFIG_BUILD_ICAM_MINI
	icam_mini_app_main();
#else
	icam_app_main();
#endif
}
