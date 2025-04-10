/*
 * Originally part of Infiray "ASIC_2121W_RTOS_SDK_V1.0.0_release".
 * Modified by Dan Julio for use in iCam/iCamMini.
 */
#ifndef DATA_RW_
#define DATA_RW_

#include <stdint.h>
#include <stdio.h>
#include "all_config.h"


#define I2C_SLAVE_ID			0x78

#define I2C_VD_BUFFER_RW			0x1D00
#define I2C_VD_BUFFER_HLD			0x9D00
#define I2C_VD_CHECK_ACCESS			0x8000
#define I2C_VD_BUFFER_DATA_LEN		256
#define I2C_OUT_BUFFER_MAX          64
//#define I2C_OUT_BUFFER_MAX			64 // IN buffer set equal to I2C_VD_BUFFER_DATA_LEN(256)


// [7:2] are error bits, [1] is result bit, and [0] is busy bit.
#define I2C_VD_BUFFER_STATUS			0x0200
#define VCMD_BUSY_STS_BIT				0x01
#define VCMD_RST_STS_BIT				0x02
#define VCMD_ERR_STS_BIT				0xFC

#define VCMD_BUSY_STS_IDLE				0x00
#define VCMD_BUSY_STS_BUSY				0x01

#define VCMD_RST_STS_PASS				0x00
#define VCMD_RST_STS_FAIL				0x02

#define VCMD_ERR_STS_SUCCESS				0x00
#define VCMD_ERR_STS_LEN_ERR				0x04
#define VCMD_ERR_STS_UNKNOWN_CMD_ERR		0x08
#define VCMD_ERR_STS_HW_ERR					0x0C
#define VCMD_ERR_STS_UNKNOWN_SUBCMD_ERR		0x10
#define VCMD_ERR_STS_PARAM_ERR				0x14


#define USB_CTL_TRANSFER_TIMEROUT  1000 // unit is 1ms
#define USB_SRING_BUF_MAX			255

#define SPECIAL_POLL_DELAY_INC_MSEC 100


typedef struct
{
	uint8_t		byCmdType;
	uint8_t		bySubCmd;
	uint8_t		byPara;
	uint8_t		byAddr_h;
	uint8_t		byAddr_l;
	uint8_t		byAddr_ll;
	uint8_t		byLen_h;
	uint8_t		byLen_l;
}vdcmd_std_header_t;

typedef struct
{
	uint8_t		byCmd;
	uint8_t		byLen;
	uint8_t		byAddr_h;
	uint8_t		byAddr_l;
}vdcmd_short_header_t;

typedef struct
{
	uint8_t byCmdType;
	uint8_t bySubCmd;
	uint8_t byParam_h;
	uint8_t byParam_l;
	uint8_t byAddr1_hh;
	uint8_t byAddr1_h;
	uint8_t byAddr1_l;
	uint8_t byAddr1_ll;

	uint8_t byAddr2_hh;
	uint8_t byAddr2_h;
	uint8_t byAddr2_l;
	uint8_t byAddr2_ll;
	uint8_t byLen_hh;
	uint8_t byLen_h;
	uint8_t byLen_l;
	uint8_t byLen_ll;
}vdcmd_long_header_t;


ir_error_t i2c_init();
ir_error_t i2c_data_read(uint16_t byI2CSlaveID, uint16_t wI2CRegAddr, uint16_t wLen, uint8_t* pbyData);
ir_error_t i2c_data_write(uint16_t byI2CSlaveID, uint16_t wI2CRegAddr, uint16_t wLen, uint8_t* pbyData);
ir_error_t i2c_data_write_no_wait(uint16_t byI2CSlaveID, uint16_t wI2CRegAddr, uint16_t wLen, uint8_t* pbyData);
void i2c_set_special_poll_delay(uint16_t delay_ms);
#endif
