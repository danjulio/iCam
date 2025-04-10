/*
 * Time related utilities
 *
 * Contains functions to interface the RTC to the system timekeeping
 * capabilities and provide application access to the system time.
 *
 * Copyright 2020-2021, 2023 Dan Julio
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
 *
 */
#include "esp_system.h"
#include "time_utilities.h"
#include "esp_log.h"
#include <stdlib.h>
#include <sys/time.h>

#ifdef CONFIG_BUILD_ICAM_MINI
	#include "pcf85063_rtc.h"
#else
	#include "gcore.h"
#endif


//
// Constants
//
// Minimum epoch time (12:00:00 AM Jan 1 2000)
#define MIN_EPOCH_TIME 946684800


//
// Time Utilities date related strings (related to tmElements)
//
static const char* day_strings[] = {
	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat"
};

static const char* mon_strings[] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};



//
// Time Utilities Variables
//
static const char* TAG = "time_utilities";


//
// Forward declarations for internal functions
//
void _rtc_read_time(tmElements_t* tm);
bool _rtc_write_time(tmElements_t* tm);
 
//
// Time Utilities API
//

/**
 * Initialize system time from the RTC
 */
void time_init()
{
	char buf[32];
	struct timeval tv;
	tmElements_t te;
	uint32_t secs;
	
	// Set the system time from the RTC
#ifdef CONFIG_BUILD_ICAM_MINI
	if (pcf85063_init()) {
		(void) pcf85063_get_time(&te);
		secs = mktime(&te);
	} else {
		secs = MIN_EPOCH_TIME;
	}
#else
	if (!gcore_get_time_secs(&secs)) {
		ESP_LOGE(TAG, "gCore RTC didn't respond");
		secs = MIN_EPOCH_TIME;
	}
#endif
	
	// Set ESP32 system clock
	tv.tv_sec = secs;
	tv.tv_usec = 0;
	settimeofday((const struct timeval *) &tv, NULL);
	
	// Diagnostic display of time
	time_get(&te);
	time_get_disp_string(&te, buf);
	ESP_LOGI(TAG, "Set time: %s", buf);
}


/**
 * Set the system time and update the RTC
 */
void time_set(tmElements_t* te)
{
	char buf[32];
	struct timeval tv;
	time_t secs;
	
	// Set the system time
	secs = mktime(te);
	tv.tv_sec = secs;
	tv.tv_usec = 0;
	settimeofday((const struct timeval *) &tv, NULL);
	
	// Then attempt to set the RTC
	if (_rtc_write_time(te)) {
		time_get_disp_string(te, buf);
		ESP_LOGI(TAG, "Set RTC time: %s", buf);
	} else {
		ESP_LOGE(TAG, "Update RTC failed");
	}
}



/**
 * Get the system time
 */
void time_get(tmElements_t* te)
{
	time_t now;
	struct timeval tv;
	
	// Get the time and convert into our simplified tmElements format
	(void) gettimeofday(&tv, NULL);
	
	now = tv.tv_sec;
    localtime_r(&now, te);  // Get the unix formatted timeinfo
    mktime(te);             // Fill in the DOW and DOY fields
}


/**
 * Return true if the system time (in seconds) has changed from the last time
 * this function returned true. Each calling task must maintain its own prev_time
 * variable (it can initialize it to 0).  Set te to NULL if you don't need the time.
 */
bool time_changed(tmElements_t* te, time_t* prev_time)
{
	time_t now;
	struct tm timeinfo;
	
	// Get the time and check if it is different
	time(&now);
	if (now != *prev_time) {
		*prev_time = now;
		
		if (te != NULL) {
			// convert time into our simplified tmElements format
    		localtime_r(&now, &timeinfo);  // Get the unix formatted timeinfo
    		mktime(&timeinfo);             // Fill in the DOW and DOY fields
    	}
    	
    	return true;
    } else {
    	return false;
    }
}


/**
 * Load buf with a time & date string for display.
 *
 *   "DOW MON DAY, YEAR HH:MM:SS"
 *
 * buf must be at least 26 bytes long (to include null termination).
 */
void time_get_disp_string(tmElements_t* te, char* buf)
{
	// Validate te to prevent illegal accesses to the constant string buffers
	if (te->tm_wday > 6) te->tm_wday = 0;
	if (te->tm_mon > 11) te->tm_mon = 0;
	
	// Build up the string
	sprintf(buf,"%s %s %2d, %4d %2d:%02d:%02d", 
		day_strings[te->tm_wday],
		mon_strings[te->tm_mon],
		te->tm_mday,
		te->tm_year + 1900,
		te->tm_hour,
		te->tm_min,
		te->tm_sec);
}



//
// Internal functions
//
void _rtc_read_time(tmElements_t* tm)
{
	uint32_t secs = 0;
	time_t tv;
	tmElements_t* te;
		
#ifdef CONFIG_BUILD_ICAM_MINI
	if (!pcf85063_get_time(tm)) {
		secs = MIN_EPOCH_TIME;
		tv = (time_t) secs;
		te = localtime(&tv);
		*tm = *te;
	}
#else	
	if (!gcore_get_time_secs(&secs)) {
		secs = MIN_EPOCH_TIME;
	}
	tv = (time_t) secs;
	te = localtime(&tv);
	*tm = *te;
#endif
}


bool _rtc_write_time(tmElements_t* tm)
{
#ifdef CONFIG_BUILD_ICAM_MINI
	return (pcf85063_set_time(tm));
#else
	time_t t;
	
	t = mktime(tm);
	
	return (gcore_set_time_secs((uint32_t) t));
#endif
}
