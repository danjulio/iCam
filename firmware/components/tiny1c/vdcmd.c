/*
 * Originally part of Infiray "ASIC_2121W_RTOS_SDK_V1.0.0_release".
 * Modified by Dan Julio for use in iCam/iCamMini.
 */
#include "esp_log.h"
#include "vdcmd.h"

static const char* TAG = "vdcmd";

vdcmd_rw_handle_t vdcmd_read_handle = { 0 };
vdcmd_rw_handle_t vdcmd_write_handle = { 0 };

ir_error_t vdcmd_rw_handle_register(enum product_types product_type, enum driver_types driver_type)
{
    switch (driver_type)
    {
    case I2C_VDCMD:
        if (i2c_init() == IR_SUCCESS) {
            vdcmd_read_handle.handle = i2c_data_read;
            vdcmd_read_handle.delay = i2c_set_special_poll_delay;
            vdcmd_read_handle.slave_id = I2C_SLAVE_ID;

            vdcmd_write_handle.handle = i2c_data_write;
            vdcmd_write_handle.delay = i2c_set_special_poll_delay;
            vdcmd_write_handle.slave_id = I2C_SLAVE_ID;
        } else {
            return IR_I2C_DEVICE_OPEN_FAIL;
        }
        break;
    case UART_VDCMD:
        return IR_VDCMD_REGISTER_FAIL;
    default:
        return IR_VDCMD_REGISTER_FAIL;
    }
    return IR_SUCCESS;
}

ir_error_t short_cmd_read(vdcmd_short_header_t* ptVdCmdShortHeader, uint8_t byLen, uint8_t* pbyData)
{
    uint16_t byMaxBufferLen = 64;
    uint16_t wTmpLen;
    ir_error_t rst;
    if (ptVdCmdShortHeader == NULL)
    {
        ESP_LOGE(TAG, "short_cmd_read: parameter error");
        return IR_ERROR_PARAM;
    }
    
    while (byLen)
    {
        wTmpLen = (byLen >= (byMaxBufferLen - 1)) ? (byMaxBufferLen - 1) : byLen;

        if ((vdcmd_write_handle.handle == NULL) || (vdcmd_read_handle.handle == NULL))
        {
            return IR_VDCMD_NOT_REGISTER;
        }

        rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, I2C_VD_BUFFER_RW, \
            sizeof(vdcmd_short_header_t),(uint8_t*)ptVdCmdShortHeader);
        if (rst != IR_SUCCESS)
        {
            ESP_LOGE(TAG, "short_cmd_read: send write cmd failed");
            return rst;
        }

        rst = vdcmd_read_handle.handle(vdcmd_read_handle.slave_id, I2C_VD_BUFFER_RW + \
            sizeof(vdcmd_short_header_t), wTmpLen, pbyData);
        if (rst!=IR_SUCCESS)
        {
            ESP_LOGE(TAG, "short_cmd_read: read buffer failed!");
            return rst;
        }

        byLen -= wTmpLen;
        pbyData += wTmpLen;
    }

    
    return IR_SUCCESS;
}

ir_error_t short_cmd_write(vdcmd_short_header_t* ptVdCmdShortHeader, uint8_t byLen, uint8_t* pbyData)
{
    ir_error_t rst;
    if ((byLen == 0) || (pbyData == NULL))
    {
        if (vdcmd_write_handle.handle == NULL)
        {
            return IR_VDCMD_NOT_REGISTER;
        }

        // no data stage, just send command info
        rst =  vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, I2C_VD_BUFFER_RW, \
            sizeof(vdcmd_std_header_t), (uint8_t*)ptVdCmdShortHeader);
        if (rst != IR_SUCCESS)
        {
            ESP_LOGE(TAG, "short_cmd_write: send write cmd failed");            
        }
        return rst;
    }

    rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, I2C_VD_BUFFER_RW, \
        sizeof(vdcmd_short_header_t), (uint8_t*)ptVdCmdShortHeader);
    if (rst!=IR_SUCCESS)
    {
        ESP_LOGE(TAG, "short_cmd_write: send write cmd failed");
        
        return rst;
    }

    if (byLen <= 4)
    {
        rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, \
            I2C_VD_BUFFER_RW + sizeof(vdcmd_short_header_t), \
            byLen, pbyData);
        if (rst!=IR_SUCCESS)
        {
            ESP_LOGE(TAG, "short_cmd_write: send write cmd failed");
            return rst;
        }
    }
    else
    {
        rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, \
            I2C_VD_BUFFER_HLD + sizeof(vdcmd_short_header_t), \
            byLen - 4, pbyData);
        if (rst!=IR_SUCCESS)
        {
            ESP_LOGE(TAG, "short_cmd_write: write buffer failed");
            return rst;
        }

        rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, \
            I2C_VD_BUFFER_RW + sizeof(vdcmd_short_header_t) - 4, \
            4, pbyData + byLen - 4);
        if (rst!=IR_SUCCESS)
        {
            ESP_LOGE(TAG, "short_cmd_write: send write cmd failed");
            return rst;
        }
    }
    
    return IR_SUCCESS;
}

ir_error_t standard_cmd_read(vdcmd_std_header_t* ptVdCmdHeader, uint16_t wLen, uint8_t* pbyData)
{
    uint16_t byMaxBufferLen = 256;
    uint32_t dwAddr;
    uint16_t wTmpLen;
    ir_error_t rst;
    dwAddr = ((uint32_t)ptVdCmdHeader->byPara << 24) + ((uint32_t)ptVdCmdHeader->byAddr_h << 16) + \
        ((uint32_t)ptVdCmdHeader->byAddr_l << 8) + ptVdCmdHeader->byAddr_ll;
    if (ptVdCmdHeader == NULL)
    {
        ESP_LOGE(TAG, "standard_cmd_read: parameter error");
        return IR_ERROR_PARAM;
    }

    
    while (wLen)
    {
        if (wLen >= byMaxBufferLen)
        {
            wTmpLen = byMaxBufferLen;
        }
        else
        {
            wTmpLen = wLen;
        }
        ptVdCmdHeader->byPara = (uint8_t)(dwAddr >> 24);
        ptVdCmdHeader->byAddr_h = (uint8_t)(dwAddr >> 16);
        ptVdCmdHeader->byAddr_l = (uint8_t)(dwAddr >> 8);
        ptVdCmdHeader->byAddr_ll = (uint8_t)dwAddr;

        ptVdCmdHeader->byLen_h = (uint8_t)(wTmpLen >> 8);
        ptVdCmdHeader->byLen_l = (uint8_t)wTmpLen;
        // 1) write spi cmd header
        if ((vdcmd_write_handle.handle == NULL) || (vdcmd_read_handle.handle == NULL))
        {
            return IR_VDCMD_NOT_REGISTER;
        }

        rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, I2C_VD_BUFFER_RW, \
                                       sizeof(vdcmd_std_header_t), (uint8_t*)ptVdCmdHeader);
        if (rst != IR_SUCCESS)
        {
            ESP_LOGE(TAG, "standard_cmd_read: send write cmd failed");
            return rst;
        }

        rst = vdcmd_read_handle.handle(vdcmd_read_handle.slave_id, I2C_VD_BUFFER_RW + \
                                      sizeof(vdcmd_std_header_t), wTmpLen, pbyData);
        if (rst != IR_SUCCESS)
        {
            ESP_LOGE(TAG, "standard_cmd_read: read buffer failed!");
            return rst;
        }
        dwAddr += wTmpLen;
        wLen -= wTmpLen;
        pbyData += wTmpLen;
    }
    
    return IR_SUCCESS;
}

ir_error_t standard_cmd_write(vdcmd_std_header_t* ptVdCmdHeader, uint16_t wLen, uint8_t* pbyData)
{
    uint16_t byMaxBufferLen = I2C_VD_BUFFER_DATA_LEN;
    uint32_t dwDataOffset = 0;
    uint8_t byUsbOneTransferLen;
    uint16_t wI2COneTransferLen= I2C_VD_BUFFER_DATA_LEN;
    uint16_t wTmpLen;
    uint32_t dwAddr;
    ir_error_t rst;

    dwAddr = ((uint32_t)ptVdCmdHeader->byPara << 24) + ((uint32_t)ptVdCmdHeader->byAddr_h << 16) \
        + ((uint32_t)ptVdCmdHeader->byAddr_l << 8) + ptVdCmdHeader->byAddr_ll;
    do
    {
        if ((wLen == 0) || (pbyData == NULL))
        {
            if ((vdcmd_write_handle.handle == NULL) || (vdcmd_read_handle.handle == NULL))
            {
                return IR_VDCMD_NOT_REGISTER;
            }
            // no data stage, just send command info
            rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, I2C_VD_BUFFER_RW, \
                sizeof(vdcmd_std_header_t), (uint8_t*)ptVdCmdHeader);
            if (rst < 0)
            {
                ESP_LOGE(TAG, "standard_cmd_write: send write cmd failed");
                return rst;
            }
            return rst;
        }
        if (wLen >= byMaxBufferLen)
        {
            wTmpLen = byMaxBufferLen;
        }
        else
        {
            wTmpLen = wLen;
        }
        wI2COneTransferLen = wTmpLen;
        ptVdCmdHeader->byPara = (uint8_t)(dwAddr >> 24);
        ptVdCmdHeader->byAddr_h = (uint8_t)(dwAddr >> 16);
        ptVdCmdHeader->byAddr_l = (uint8_t)(dwAddr >> 8);
        ptVdCmdHeader->byAddr_ll = (uint8_t)dwAddr;

        ptVdCmdHeader->byLen_h = (uint8_t)(wI2COneTransferLen >> 8);
        ptVdCmdHeader->byLen_l = (uint8_t)wI2COneTransferLen;

        rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, I2C_VD_BUFFER_HLD, \
            sizeof(vdcmd_std_header_t), (uint8_t*)ptVdCmdHeader);
        if (rst != IR_SUCCESS)
        {
            ESP_LOGE(TAG, "standard_cmd_write: send write cmd failed");
            return rst;
        }
        dwDataOffset = 0;
        while (wTmpLen > 0)
        {
            if (wTmpLen > I2C_OUT_BUFFER_MAX)
            {
                byUsbOneTransferLen = I2C_OUT_BUFFER_MAX;
                rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, \
                    I2C_VD_BUFFER_HLD + dwDataOffset + sizeof(vdcmd_std_header_t), I2C_OUT_BUFFER_MAX, pbyData);
                if (rst != IR_SUCCESS)
                {
                    ESP_LOGE(TAG, "standard_cmd_write: write buffer failed");
                    return rst;
                }
            }
            else  //wLen in 1-64
            {
                byUsbOneTransferLen = (uint8_t)wTmpLen;
                if (wTmpLen <= 8)
                {
                    rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, \
                        I2C_VD_BUFFER_RW+ dwDataOffset + sizeof(vdcmd_std_header_t), \
                        wTmpLen, pbyData);
                    if (rst != IR_SUCCESS)
                    {
                        ESP_LOGE(TAG, "standard_cmd_write: send write cmd failed");
                        return rst;
                    }
                }
                else
                {
                    rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, \
                        I2C_VD_BUFFER_HLD + dwDataOffset + sizeof(vdcmd_std_header_t), \
                        wTmpLen - 8, pbyData);
                    if (rst < 0)
                    {
                        ESP_LOGE(TAG, "standard_cmd_write: write buffer failed");
                        return rst;
                    }
                    rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, \
                        I2C_VD_BUFFER_RW + dwDataOffset + sizeof(vdcmd_std_header_t)+wTmpLen - 8, \
                        8, pbyData + wTmpLen - 8);
                    if (rst < 0)
                    {
                        ESP_LOGE(TAG, "standard_cmd_write: send write cmd failed");
                        return rst;
                    }
                }
            }
            wTmpLen -= byUsbOneTransferLen;
            pbyData += byUsbOneTransferLen;
            dwDataOffset += byUsbOneTransferLen;
        }
        wLen -= wI2COneTransferLen;
        dwAddr += wI2COneTransferLen;
    } while (wLen > 0);
    
    return IR_SUCCESS;
}

ir_error_t long_cmd_read(vdcmd_long_header_t* ptVdCmdHeader, uint16_t wParam, uint32_t dwAddr1,\
                  uint32_t dwAddr2, uint32_t dwLen, uint8_t data[])
{
    ir_error_t rst;

    if (dwLen > (256 + 8 - sizeof(vdcmd_long_header_t)))
    {
        ESP_LOGE(TAG, "long_cmd_read: data length out of the limitation");
        return IR_VDCMD_TOO_LONG;	//the data stage length is limit to (256+8-16) bytes for i2c long cmd read
    }

    ptVdCmdHeader->byParam_h = (uint8_t)(wParam >> 8);
    ptVdCmdHeader->byParam_l = (uint8_t)(wParam);

    if (dwLen != 0)
    {
        ptVdCmdHeader->byLen_hh = (uint8_t)(dwLen >> 24);
        ptVdCmdHeader->byLen_h = (uint8_t)(dwLen >> 16);
        ptVdCmdHeader->byLen_l = (uint8_t)(dwLen >> 8);
        ptVdCmdHeader->byLen_ll = (uint8_t)dwLen;
    }

    ptVdCmdHeader->byAddr1_hh = (uint8_t)(dwAddr1 >> 24);
    ptVdCmdHeader->byAddr1_h = (uint8_t)(dwAddr1 >> 16);
    ptVdCmdHeader->byAddr1_l = (uint8_t)(dwAddr1 >> 8);
    ptVdCmdHeader->byAddr1_ll = (uint8_t)dwAddr1;

    ptVdCmdHeader->byAddr2_hh = (uint8_t)(dwAddr2 >> 24);
    ptVdCmdHeader->byAddr2_h = (uint8_t)(dwAddr2 >> 16);
    ptVdCmdHeader->byAddr2_l = (uint8_t)(dwAddr2 >> 8);
    ptVdCmdHeader->byAddr2_ll = (uint8_t)dwAddr2;

    if ((vdcmd_write_handle.handle == NULL) || (vdcmd_read_handle.handle == NULL))
    {
        return IR_VDCMD_NOT_REGISTER;
    }

    rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, \
        I2C_VD_BUFFER_HLD, 8, (uint8_t*)ptVdCmdHeader);
    if (rst != IR_SUCCESS)
    {
        ESP_LOGE(TAG, "long_cmd_read: write buffer failed!");
        return rst;
    }

    rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id, \
        I2C_VD_BUFFER_RW + 8, 8, ((uint8_t*)ptVdCmdHeader) + 8);
    if (rst != IR_SUCCESS)
    {
        ESP_LOGE(TAG, "long_cmd_read: send write cmd failed!");
        return rst;
    }

    rst = vdcmd_read_handle.handle(vdcmd_read_handle.slave_id, \
        I2C_VD_BUFFER_RW + sizeof(vdcmd_long_header_t), (uint16_t)dwLen, data);
    if (rst != IR_SUCCESS)
    {
        ESP_LOGE(TAG, "long_cmd_read: read buffer failed!");
        
        return rst;
    }
    return IR_SUCCESS;
}

ir_error_t long_cmd_write(vdcmd_long_header_t* ptVdCmdHeader, uint16_t wParam, uint32_t dwAddr1,\
                   uint32_t dwAddr2, uint32_t dwLen)
{
    ir_error_t rst;

    ptVdCmdHeader->byParam_h = (uint8_t)(wParam >> 8);
    ptVdCmdHeader->byParam_l = (uint8_t)(wParam);

    if (dwLen != 0)
    {
        ptVdCmdHeader->byLen_hh = (uint8_t)(dwLen >> 24);
        ptVdCmdHeader->byLen_h = (uint8_t)(dwLen >> 16);
        ptVdCmdHeader->byLen_l = (uint8_t)(dwLen >> 8);
        ptVdCmdHeader->byLen_ll = (uint8_t)dwLen;
    }

    ptVdCmdHeader->byAddr1_hh = (uint8_t)(dwAddr1 >> 24);
    ptVdCmdHeader->byAddr1_h = (uint8_t)(dwAddr1 >> 16);
    ptVdCmdHeader->byAddr1_l = (uint8_t)(dwAddr1 >> 8);
    ptVdCmdHeader->byAddr1_ll = (uint8_t)dwAddr1;

    ptVdCmdHeader->byAddr2_hh = (uint8_t)(dwAddr2 >> 24);
    ptVdCmdHeader->byAddr2_h = (uint8_t)(dwAddr2 >> 16);
    ptVdCmdHeader->byAddr2_l = (uint8_t)(dwAddr2 >> 8);
    ptVdCmdHeader->byAddr2_ll = (uint8_t)dwAddr2;

    if ((vdcmd_write_handle.handle == NULL) || (vdcmd_read_handle.handle == NULL))
    {
        return IR_VDCMD_NOT_REGISTER;
    }

    rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id,\
                                   I2C_VD_BUFFER_HLD, 8, (uint8_t*)ptVdCmdHeader);
    if (rst != IR_SUCCESS)
    {
        ESP_LOGE(TAG, "long_cmd_write 1 failed!");
        return rst;
    }
    rst = vdcmd_write_handle.handle(vdcmd_write_handle.slave_id,\
                                   I2C_VD_BUFFER_RW + 8, 8, ((uint8_t*)ptVdCmdHeader) + 8);
    if (rst != IR_SUCCESS)
    {
        ESP_LOGE(TAG, "long_cmd_write 2 failed!");
        return rst;
    }
    return IR_SUCCESS;
}


void vdcmd_set_special_poll_delay(uint16_t delay_ms)
{
	if (vdcmd_write_handle.delay != NULL) {
		vdcmd_write_handle.delay(delay_ms);
	}
}

