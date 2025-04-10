/*
 * Originally part of Infiray "ASIC_2121W_RTOS_SDK_V1.0.0_release".
 * Modified by Dan Julio for use in iCam/iCamMini.
 */
#pragma once

#define		FALCON_CMD
#define I2C_TRANSFER_WAIT_TIME_MS  20000
/**
 * @brief Return error's type
 */
typedef enum{
	/// success
	IR_SUCCESS						= 0,
	/// parameters error
	IR_ERROR_PARAM					= -1,
	/// libiruvc's service context initialize failed
	IR_UVC_INIT_FAIL				= -2,
	/// get device list failed
	IR_GET_DEVICE_LIST_FAIL			= -3,
	/// find device via pid/vid/name failed
	IR_FIND_DEVICE_FAIL				= -4,
	/// find device via pid/vid/name failed
	IR_NOT_GET_DEVICE_INFO			= -5,
	/// open device fail
	IR_DEVICE_OPEN_FAIL				= -6,
	/// get device's descriptor failed
	IR_GET_DEVICE_DESCRIPTOR_FAIL	= -7,
	/// device already opened
	IR_DEVICE_OPENED				= -8,
	/// device doesn't provide a matching stream
	IR_GET_FORMAT_FAIL				= -9,
	/// user's callback fucntion is empty
	IR_USER_CALLBACK_EMPTY			= -10,
	/// device start streaming failed
	IR_START_STREAMING_FAIL			= -11,
	/// over time when getting frame
	IR_GET_FRAME_OVER_TIME			= -12,
	/// vdcmd not registered
	IR_VDCMD_NOT_REGISTER			= -13,
	/// vdcmd register failed
	IR_VDCMD_REGISTER_FAIL			= -14,
	/// control transter failed
	IR_CONTROL_TRANSFER_FAIL		= -15,
	/// checking vdcmd send failed
	IR_CHECK_DONE_FAIL				= -16,
	/// vdcmd's length is too long
	IR_VDCMD_TOO_LONG				= -17,
	/// reset device fail
	IR_RESET_DEVICE_FAIL			= -18,
	/// clear halt fail
	IR_CLEAR_HALT_FAIL				= -19,
	/// unsupported command
	IR_UNSUPPORTED_CMD				= -20,
	/// i2c transfer fail
	IR_I2C_TRANSFER_FAIL			= -21,
	/// i2c device open fail
	IR_I2C_DEVICE_OPEN_FAIL			= -22,
	/// i2c get register fail
	IR_I2C_GET_REGISTER_FAIL		= -23,
	/// i2c set register fail
	IR_I2C_SET_REGISTER_FAIL		= -24,
	/// memory allocation fail
	IR_MEM_ALLOC_FAIL				= -25
}ir_error_t;

/**
 * @brief Log message levels
 */
typedef enum{
	/// all debug&error infomation
	IR_LOG_DEBUG						= 0,
	/// all error infomation
	IR_LOG_ERROR						= 1,
	/// no print infomation(default)
	IR_LOG_NO_PRINT					= 2
}ir_log_level_t;


