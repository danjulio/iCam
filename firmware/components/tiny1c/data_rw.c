/*
 * Originally part of Infiray "ASIC_2121W_RTOS_SDK_V1.0.0_release".
 * Modified by Dan Julio for use in iCam/iCamMini.
 */
#include "data_rw.h"
#include "esp_log.h"
#include "t1c_i2c_hal.h"


static const char* TAG = "data_rw";

static uint16_t special_poll_delay_ms = 0;


ir_error_t i2c_init()
{
    if (HAL_I2C_Init() == HAL_OK) {
        return IR_SUCCESS;
    } else {
        return IR_I2C_DEVICE_OPEN_FAIL;
    }
}


ir_error_t i2c_data_read(uint16_t byI2CSlaveID, uint16_t wI2CRegAddr, uint16_t wLen, uint8_t* pbyData)
{
    HAL_StatusTypeDef rst;
    if ((pbyData == NULL) || (wLen == 0))
    {
        ESP_LOGE(TAG, "parameter error");
        return IR_ERROR_PARAM;
    }

    if (wLen > I2C_VD_BUFFER_DATA_LEN) // one transfer length fix to i2c vendor buffer limitation
    {
        ESP_LOGE(TAG, "I2C in transfer length out of limitation:%d", I2C_VD_BUFFER_DATA_LEN);
        return IR_ERROR_PARAM;
    }
    rst = HAL_I2C_Mem_Read(byI2CSlaveID, wI2CRegAddr, I2C_MEMADD_SIZE_16BIT, pbyData, wLen, I2C_TRANSFER_WAIT_TIME_MS);
    if (rst != HAL_OK)
    {
        ESP_LOGE(TAG, "I2C read command failed (error_code:%d)", rst);
        return IR_I2C_GET_REGISTER_FAIL;
    }
    else 
    {
        return IR_SUCCESS;
    }
}


static ir_error_t i2c_check_access_done(void)
{
	HAL_StatusTypeDef rst;
    uint8_t status = 0xFF;
    uint8_t error_type=0;
    uint16_t wWaitTime = I2C_TRANSFER_WAIT_TIME_MS;
    
    do
    {
        rst = i2c_data_read(I2C_SLAVE_ID, I2C_VD_BUFFER_STATUS, 1, &status);
        if (rst != HAL_OK) {
        	ESP_LOGE(TAG, "I2C Check status read failed: %d", rst);
        	goto i2c_check_access_done_err;
        }
        error_type=(status & VCMD_ERR_STS_BIT);
        switch(error_type)
        {
            case VCMD_ERR_STS_LEN_ERR:
                ESP_LOGE(TAG, "LEN_ERR");
            	goto i2c_check_access_done_err;
            case VCMD_ERR_STS_UNKNOWN_CMD_ERR:
                ESP_LOGE(TAG, "UNKNOWN_CMD_ERR");
                goto i2c_check_access_done_err;
            case VCMD_ERR_STS_HW_ERR:
                ESP_LOGE(TAG, "HW_ERR");
                goto i2c_check_access_done_err;
            case VCMD_ERR_STS_UNKNOWN_SUBCMD_ERR:
                ESP_LOGE(TAG, "UNKNOWN_SUBCMD_ERR");
                goto i2c_check_access_done_err;
            case VCMD_ERR_STS_PARAM_ERR:
                ESP_LOGE(TAG, "PARAM_ERR");
                goto i2c_check_access_done_err;
            default:
                break;
        }
        if ((status & (VCMD_RST_STS_BIT | VCMD_BUSY_STS_BIT)) == \
            (VCMD_BUSY_STS_IDLE | VCMD_RST_STS_PASS))
        {
            return IR_SUCCESS;
        }
        HAL_Delay(5);
        wWaitTime-=5;
    } while (wWaitTime);
    
i2c_check_access_done_err:
    ESP_LOGE(TAG, "check done fail!");
    return IR_CHECK_DONE_FAIL;
}


ir_error_t i2c_data_write(uint16_t byI2CSlaveID, uint16_t wI2CRegAddr, uint16_t wLen, uint8_t* pbyData)
{
    HAL_StatusTypeDef rst;
    int n;

    if (wLen > I2C_OUT_BUFFER_MAX)
    {
        ESP_LOGE(TAG, "parameter error");
        return IR_ERROR_PARAM;
    }
    rst= HAL_I2C_Mem_Write(byI2CSlaveID, wI2CRegAddr, I2C_MEMADD_SIZE_16BIT, pbyData, wLen, I2C_TRANSFER_WAIT_TIME_MS);
    if (rst != HAL_OK)
    {
        ESP_LOGE(TAG, "I2C write command failed (error_code:%d)", rst);
        return IR_I2C_SET_REGISTER_FAIL;
    }
        //when wI2CRegAddr bit[15] = 1,only transfer data to vdcmd buf,no need to call check_access_done
    if (wI2CRegAddr & I2C_VD_CHECK_ACCESS)
    {
        return IR_SUCCESS;
    }
    else
    {
    	if (special_poll_delay_ms != 0) {
    		n = special_poll_delay_ms / SPECIAL_POLL_DELAY_INC_MSEC;
    		while (n--) {    	
    			HAL_Delay(SPECIAL_POLL_DELAY_INC_MSEC);
    		
    			// Ignore failures but return a successful poll during delay period
    			if (i2c_check_access_done() == IR_SUCCESS) {
    				return IR_SUCCESS;
    			}
    		}
    		// Reset to zero after use (commands must explicitly set this)
    		special_poll_delay_ms = 0;
    	}
    	
    	// Return poll result immediately if no special poll delay or after poll delay expires
    	return i2c_check_access_done();
    }

}

ir_error_t i2c_data_write_no_wait(uint16_t byI2CSlaveID, uint16_t wI2CRegAddr, uint16_t wLen, uint8_t* pbyData)
{
    HAL_StatusTypeDef rst;

    if (wLen > I2C_OUT_BUFFER_MAX)
    {
        ESP_LOGE(TAG, "parameter error");
        return IR_ERROR_PARAM;
    }
    rst= HAL_I2C_Mem_Write(byI2CSlaveID, wI2CRegAddr, I2C_MEMADD_SIZE_16BIT, pbyData, wLen, I2C_TRANSFER_WAIT_TIME_MS);
    if (rst != HAL_OK)
    {
        ESP_LOGE(TAG, "I2C write command failed (error_code:%d)", rst);
        return IR_I2C_SET_REGISTER_FAIL;
    }
    
    return IR_SUCCESS;
}


void i2c_set_special_poll_delay(uint16_t delay_ms)
{
	special_poll_delay_ms = delay_ms;
}
