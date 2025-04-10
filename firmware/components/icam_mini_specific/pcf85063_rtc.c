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
#include "esp_system.h"
#include "esp_log.h"
#include "i2cs.h"
#include "pcf85063_rtc.h"


//
// Constants
//

// 7-bit I2C address
#define PCF85063_I2C_ADDRESS 0x51

// Register offsets
#define REG_CTRL1         0x00
#define REG_CTRL2         0x01
#define REG_OFFSET        0x02
#define REG_OFFSET        0x02
#define REG_RAM           0x03
#define REG_SEC           0x04
#define REG_MIN           0x05
#define REG_HOUR          0x06
#define REG_DAY_MONTH     0x07
#define REG_DAY_WEEK      0x08
#define REG_MON           0x09
#define REG_YEAR          0x0A

// Register Control_1 bit masks
#define CTRL1_CAP_SEL_7   0x00
#define CTRL1_CAP_SEL_12  0x01
#define CTRL1_24_HOUR     0x00
#define CTRL1_12_HOUR     0x02
#define CTRL1_CIE_DIS     0x00
#define CTRL1_CIE_EN      0x04
#define CTRL1_SR_TRIG     0x10
#define CTRL1_STOP_MASK   0x20
#define CTRL1_MODE_NORM   0x00
#define CTRL1_MODE_TEST   0x80

// Special software reset value for Control_1
#define CTRL1_SOFT_RESET  0x58

// Register Control_2 bit masks
#define CTRL2_COF_MASK    0x07
#define CTRL2_COF_32KHZ   0x00
#define CTRL2_COF_16KHZ   0x01
#define CTRL2_COF_8KHZ    0x02
#define CTRL2_COF_4KHZ    0x03
#define CTRL2_COF_2KHZ    0x04
#define CTRL2_COF_1KHZ    0x05
#define CTRL2_COF_1HZ     0x06
#define CTRL2_COF_NONE    0x07
#define CTRL2_TF_DIS      0x00
#define CTRL2_TF_EN       0x08
#define CTRL2_HMI_DIS     0x00
#define CTRL2_HMI_EN      0x10
#define CTRL2_MI_DIS      0x00
#define CTRL2_MI_EN       0x20

// Register Offset masks
#define OFFSET_VAL_MASK   0x7F
#define OFFSET_MODE_2HR   0x00
#define OFFSET_MODE_4MIN  0x80

// Register Seconds masks (0-59)
#define SEC_UNIT_BCD_MASK 0x0F
#define SEC_TENS_BCD_MASK 0x70
#define SEC_OS_VALID      0x00
#define SEC_OS_INVALID    0x80

// Register Minutes masks (0-59)
#define MIN_UNIT_BCD_MASK 0x0F
#define MIN_TENS_BCD_MASK 0x70

// Register Hours masks (1-12 or 0-23)
#define HR_12_UNIT_BCD_MASK  0x0F
#define HR_12_TENS_BCD_MASK  0x10
#define HR_12A_M_MASK        0x00
#define HR_12_PM_MASK        0x20
#define HR_24_UNIT_BCD_MASK  0x0F
#define HR_24_TENS_BCD_MASK  0x30

// Register Days masks (1-31)
#define DAY_UNIT_BCD_MASK 0x0F
#define DAY_TENS_BCD_MASK 0x30

// Register Weekday masks (0-6 -> Sunday - Saturday)
#define WEEKDAY_MASK      0x07

// Register Months masks (1-12)
#define MON_UNIT_BCD_MASK 0x0F
#define MON_TENS_BCD_MASK 0x10

// Register Year masks (start at 2000)
#define YR_UNIT_BCD_MASK  0x0F
#define YR_TENS_BCD_MASK  0xF0


// Burst I2C length
#define BURST_I2C_LEN     7


//
// Variables
//
static const char* TAG = "pcf85063_rtc";

static bool rtc_present = false;



//
// Forward declarations for internal functions
//
static bool _read_reg(uint8_t reg, uint8_t* val);
static bool _read_reg_array(uint8_t reg, int len, uint8_t* buf);
static bool _write_reg(uint8_t reg, uint8_t val);
static bool _write_reg_array(uint8_t reg, int len, uint8_t* buf);
static uint8_t _dec_2_bcd(int val);
static int _bcd_to_dec(uint8_t val);



//
// API
//
bool pcf85063_init()
{
	uint8_t reg_val;
	
	// First probe to see if we can find the chip
    // Read the clock integrity bit in the seconds register to determine if we're up
	// and running or starting from power-on or clock issue
	if (!_read_reg(REG_SEC, &reg_val)) {
		ESP_LOGI(TAG, "Did not find RTC");
		return false;
	}
	
	ESP_LOGI(TAG, "Found RTC");
	
	if ((reg_val & SEC_OS_INVALID) == SEC_OS_INVALID) {
		// Perform a software reset - default values are good for us
		ESP_LOGI(TAG, "Reset RTC");
		if (!_write_reg(REG_CTRL1, CTRL1_SOFT_RESET)) {
			ESP_LOGE(TAG, "Reset RTC failed");
			return false;
		}
		
		// The perform the recommended STOP and START
		reg_val = CTRL1_CAP_SEL_7 | CTRL1_24_HOUR | CTRL1_CIE_DIS | CTRL1_STOP_MASK | CTRL1_MODE_NORM;
		if (!_write_reg(REG_CTRL1, reg_val)) {
			ESP_LOGE(TAG, "Stop RTC failed");
			return false;
		}
		reg_val &= ~CTRL1_STOP_MASK;
		if (!_write_reg(REG_CTRL1, reg_val)) {
			ESP_LOGE(TAG, "Start RTC failed");
			return false;
		}
	}
	
	rtc_present = true;
	
	return rtc_present;
}


bool pcf85063_get_time(struct tm* te)
{
	uint8_t time_buf[BURST_I2C_LEN];
	
	if (!rtc_present) return false;
	
	if (!_read_reg_array(REG_SEC, BURST_I2C_LEN, time_buf)) {
		return false;
	}
	
	// Convert time formats
	te->tm_sec = _bcd_to_dec(time_buf[REG_SEC - REG_SEC] & (SEC_TENS_BCD_MASK | SEC_UNIT_BCD_MASK));
	te->tm_min = _bcd_to_dec(time_buf[REG_MIN - REG_SEC] & (MIN_TENS_BCD_MASK | MIN_UNIT_BCD_MASK));
	te->tm_hour = _bcd_to_dec(time_buf[REG_HOUR - REG_SEC] & (HR_24_TENS_BCD_MASK | HR_24_UNIT_BCD_MASK));
	te->tm_mday = _bcd_to_dec(time_buf[REG_DAY_MONTH - REG_SEC] & (DAY_TENS_BCD_MASK | DAY_UNIT_BCD_MASK));
	
	// tm_mon is 0-11,  we are 1-12
	te->tm_mon = _bcd_to_dec(time_buf[REG_MON - REG_SEC] & (MON_TENS_BCD_MASK | MON_UNIT_BCD_MASK)) - 1;
	
	// tm_year starts at 1900, we at 2000
	te->tm_year = _bcd_to_dec(time_buf[REG_YEAR - REG_SEC] & (YR_TENS_BCD_MASK | YR_UNIT_BCD_MASK)) + 100;
	
	te->tm_wday = _bcd_to_dec(time_buf[REG_DAY_WEEK - REG_SEC] & (WEEKDAY_MASK));
	
	return true;
}


bool pcf85063_set_time(struct tm* te)
{
	uint8_t reg_val;
	uint8_t time_buf[BURST_I2C_LEN];
	
	if (!rtc_present) return false;
	
	// Set the STOP bit in Control_1
	if (!_read_reg(REG_CTRL1, &reg_val)) {
		return false;
	}
	reg_val |= CTRL1_STOP_MASK;
	if (!_write_reg(REG_CTRL1, reg_val)) {
		return false;
	}
	
	// Convert time formats
	time_buf[REG_SEC - REG_SEC]       = _dec_2_bcd(te->tm_sec);
	time_buf[REG_MIN - REG_SEC]       = _dec_2_bcd(te->tm_min);
	time_buf[REG_HOUR - REG_SEC]      = _dec_2_bcd(te->tm_hour);
	time_buf[REG_DAY_MONTH - REG_SEC] = _dec_2_bcd(te->tm_mday);
	time_buf[REG_MON - REG_SEC]       = _dec_2_bcd(te->tm_mon + 1);
	time_buf[REG_YEAR - REG_SEC]      = _dec_2_bcd(te->tm_year - 100);
	time_buf[REG_DAY_WEEK - REG_SEC]  = _dec_2_bcd(te->tm_wday);
	
	if (!_write_reg_array(REG_SEC, BURST_I2C_LEN, time_buf)) {
		return false;
	}
	
	// Restart
	reg_val &= ~CTRL1_STOP_MASK;
	if (!_write_reg(REG_CTRL1, reg_val)) {
		return false;
	}

	return true;
}



//
// Internal functions
//
static bool _read_reg(uint8_t reg, uint8_t* val)
{
	esp_err_t ret;
	
	i2c_sensor_lock();
	ret = i2c_sensor_write_slave(PCF85063_I2C_ADDRESS, &reg, 1);
	if (ret == ESP_OK) {
		ret = i2c_sensor_read_slave(PCF85063_I2C_ADDRESS, val, 1);
	}
	i2c_sensor_unlock();
	
	return (ret == ESP_OK);
}


static bool _read_reg_array(uint8_t reg, int len, uint8_t* buf)
{
	esp_err_t ret;
	
	i2c_sensor_lock();
	ret = i2c_sensor_write_slave(PCF85063_I2C_ADDRESS, &reg, 1);
	if (ret == ESP_OK) {
		ret = i2c_sensor_read_slave(PCF85063_I2C_ADDRESS, buf, (size_t) len);
	}
	i2c_sensor_unlock();
	
	return (ret == ESP_OK);
}


static bool _write_reg(uint8_t reg, uint8_t val)
{
	esp_err_t ret;
	uint8_t data[2];
	
	data[0] = reg;
	data[1] = val;
	
	i2c_sensor_lock();
	ret = i2c_sensor_write_slave(PCF85063_I2C_ADDRESS, data, 2);
	i2c_sensor_unlock();
	
	return (ret == ESP_OK);
}


static bool _write_reg_array(uint8_t reg, int len, uint8_t* buf)
{
	esp_err_t ret;
	uint8_t data[BURST_I2C_LEN+1];
	
	data[0] = reg;
	for (int i=0; i<len; i++) {
		data[i+1] = buf[i];
	}
	
	i2c_sensor_lock();
	ret = i2c_sensor_write_slave(PCF85063_I2C_ADDRESS, data, (size_t) len+1);
	i2c_sensor_unlock();
	
	return (ret == ESP_OK);
}


static uint8_t _dec_2_bcd(int val) {
    return (uint8_t) (((val / 10 * 16) + (val % 10)));
}


static int _bcd_to_dec(uint8_t val) {
    return (int) (((val / 16 * 10) + (val % 16)));
}
