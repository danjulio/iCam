/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 *
 * @file      driver_aht20.c
 * @brief     driver aht20 source file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2022-10-31
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2022/10/31  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#include "driver_aht20.h"

/**
 * @brief chip information definition
 */
#define CHIP_NAME                 "ASAIR AHT20"        /**< chip name */
#define MANUFACTURER_NAME         "ASAIR"              /**< manufacturer name */
#define SUPPLY_VOLTAGE_MIN        2.2f                 /**< chip min supply voltage */
#define SUPPLY_VOLTAGE_MAX        5.5f                 /**< chip max supply voltage */
#define MAX_CURRENT               0.98f                /**< chip max current */
#define TEMPERATURE_MIN           -40.0f               /**< chip min operating temperature */
#define TEMPERATURE_MAX           85.0f                /**< chip max operating temperature */
#define DRIVER_VERSION            1000                 /**< driver version */

/**
 * @brief chip address definition
 */
#define AHT20_ADDRESS             0x70        /**< iic device address */

/**
 * @brief      read bytes
 * @param[in]  *handle points to an aht20 handle structure
 * @param[out] *data points to a data buffer
 * @param[in]  len is the data length
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
static uint8_t a_aht20_iic_read(aht20_handle_t *handle, uint8_t *data, uint16_t len)
{
    if (handle->iic_read_cmd(AHT20_ADDRESS, data, len) != 0)        /* read the register */
    {
        return 1;                                                   /* return error */
    }
    else
    {
        return 0;                                                   /* success return 0 */
    }
}

/**
 * @brief     write bytes
 * @param[in] *handle points to an aht20 handle structure
 * @param[in] *data points to a data buffer
 * @param[in] len is the data length
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
static uint8_t a_aht20_iic_write(aht20_handle_t *handle, uint8_t *data, uint16_t len)
{
    if (handle->iic_write_cmd(AHT20_ADDRESS, data, len) != 0)        /* write the register */
    {
        return 1;                                                    /* return error */
    }
    else
    {
        return 0;                                                    /* success return 0 */
    }
}

/**
 * @brief     calculate the crc
 * @param[in] *data points to a data buffer
 * @param[in] len is the data length
 * @return    crc
 * @note      none
 */
static uint8_t a_aht20_calc_crc(uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint8_t byte;
    uint8_t crc = 0xFF;
    
    for (byte = 0; byte < len; byte++)          /* len times */
    {
        crc ^= data[byte];                      /* xor byte */
        for (i = 8; i > 0; --i)                 /* one byte */
        {
            if ((crc & 0x80) != 0)              /* if high*/
            {
                crc = (crc << 1) ^ 0x31;        /* xor 0x31 */
            }
            else
            {
                crc = crc << 1;                 /* skip */
            }
        }
    }
    
    return crc;                                 /* return crc */
}

/**
 * @brief     reset the register
 * @param[in] *handle points to an aht20 handle structure
 * @param[in] addr is the reset register
 * @return    status code
 *            - 0 success
 *            - 1 reset failed
 * @note      none
 */
static uint8_t a_aht20_jh_reset_reg(aht20_handle_t *handle, uint8_t addr)
{
    uint8_t buf[3];
    uint8_t regs[3];
    
    buf[0] = addr;                                     /* set the addr */
    buf[1] = 0x00;                                     /* set 0x00 */
    buf[2] = 0x00;                                     /* set 0x00 */
    if (a_aht20_iic_write(handle, buf, 3) != 0)        /* write the command */
    {
        return 1;                                      /* return error */
    }
    handle->delay_ms(5);                               /* delay 5ms */
    if (a_aht20_iic_read(handle, regs, 3) != 0)        /* read regs */
    {
        return 1;                                      /* return error */
    }
    handle->delay_ms(10);                              /* delay 10ms */
    buf[0] = 0xB0 | addr;                              /* set addr */
    buf[1] = regs[1];                                  /* set regs[1] */
    buf[2] = regs[2];                                  /* set regs[2] */
    if (a_aht20_iic_write(handle, buf, 3) != 0)        /* write the data */
    {
        return 1;                                      /* return error */
    }
    
    return 0;                                          /* success return 0 */
}

/**
 * @brief     initialize the chip
 * @param[in] *handle points to an aht20 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 iic failed
 *            - 2 handle is NULL
 *            - 3 linked functions is NULL
 *            - 4 read status failed
 *            - 5 reset reg failed
 * @note      none
 */
uint8_t aht20_init(aht20_handle_t *handle)
{
    uint8_t status;
    
    if (handle == NULL)                                                /* check handle */
    {
        return 2;                                                      /* return error */
    }
    if (handle->debug_print == NULL)                                   /* check debug_print */
    {
        return 3;                                                      /* return error */
    }
    if (handle->iic_init == NULL)                                      /* check iic_init */
    {
        handle->debug_print("aht20: iic_init is null.\n");             /* iic_init is null */
        
        return 3;                                                      /* return error */
    }
    if (handle->iic_deinit == NULL)                                    /* check iic_deinit */
    {
        handle->debug_print("aht20: iic_deinit is null.\n");           /* iic_deinit is null */
        
        return 3;                                                      /* return error */
    }
    if (handle->iic_read_cmd == NULL)                                  /* check iic_read_cmd */
    {
        handle->debug_print("aht20: iic_read_cmd is null.\n");         /* iic_read_cmd is null */
        
        return 3;                                                      /* return error */
    }
    if (handle->iic_write_cmd == NULL)                                 /* check iic_write_cmd */
    {
        handle->debug_print("aht20: iic_write_cmd is null.\n");        /* iic_write_cmd is null */
        
        return 3;                                                      /* return error */
    }
    if (handle->delay_ms == NULL)                                      /* check delay_ms */
    {
        handle->debug_print("aht20: delay_ms is null.\n");             /* delay_ms is null */
        
        return 3;                                                      /* return error */
    }
    
    if (handle->iic_init() != 0)                                       /* iic init */
    {
        handle->debug_print("aht20: iic init failed.\n");              /* iic init failed */
        
        return 1;                                                      /* return error */
    }
    handle->delay_ms(500);                                             /* wait for 500 ms */
    if (a_aht20_iic_read(handle, &status, 1) != 0)                     /* read the status */
    {
        handle->debug_print("aht20: read status failed.\n");           /* read status failed */
        (void)handle->iic_deinit();                                    /* close the iic */
        
        return 4;                                                      /* return error */
    }
    if ((status & 0x18) != 0x18)                                       /* check the status */
    {
        if (a_aht20_jh_reset_reg(handle, 0x1B) != 0)                   /* reset the 0x1B */
        {
            handle->debug_print("aht20: reset reg failed.\n");         /* reset reg failed */
            (void)handle->iic_deinit();                                /* close the iic */
            
            return 5;                                                  /* return error */
        }
        if (a_aht20_jh_reset_reg(handle, 0x1C) != 0)                   /* reset the 0x1C */
        {
            handle->debug_print("aht20: reset reg failed.\n");         /* reset reg failed */
            (void)handle->iic_deinit();                                /* close the iic */
            
            return 5;                                                  /* return error */
        }
        if (a_aht20_jh_reset_reg(handle, 0x1E) != 0)                   /* reset the 0x1E */
        {
            handle->debug_print("aht20: reset reg failed.\n");         /* reset reg failed */
            (void)handle->iic_deinit();                                /* close the iic */
            
            return 5;                                                  /* return error */
        }
    }
    handle->delay_ms(10);                                              /* delay 10ms */
    handle->inited = 1;                                                /* flag finish initialization */
    
    return 0;                                                          /* success return 0 */
}

/**
 * @brief     close the chip
 * @param[in] *handle points to an aht20 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 deinit failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t aht20_deinit(aht20_handle_t *handle)
{
    if (handle == NULL)                                            /* check handle */
    {
        return 2;                                                  /* return error */
    }
    if (handle->inited != 1)                                       /* check handle initialization */
    {
        return 3;                                                  /* return error */
    }
    
    if (handle->iic_deinit() != 0)                                 /* iic deinit */
    {
        handle->debug_print("aht20: iic deinit failed.\n");        /* iic deinit failed */
        
        return 1;                                                  /* return error */
    }
    handle->inited = 0;                                            /* set closed flag */
    
    return 0;                                                      /* success return 0 */
}

/**
 * @brief      read the temperature and humidity data
 * @param[in]  *handle points to an aht20 handle structure
 * @param[out] *temperature_raw points to a raw temperature buffer
 * @param[out] *temperature_s points to a converted temperature buffer
 * @param[out] *humidity_raw points to a raw humidity buffer
 * @param[out] *humidity_s points to a converted humidity buffer
 * @return     status code
 *             - 0 success
 *             - 1 read temperature humidity failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 data is not ready
 *             - 5 crc is error
 * @note       none
 */
uint8_t aht20_read_temperature_humidity(aht20_handle_t *handle, uint32_t *temperature_raw, float *temperature_s,
                                        uint32_t *humidity_raw, uint8_t *humidity_s)
{
    uint8_t status;
    uint8_t buf[7];
    
    if (handle == NULL)                                               /* check handle */
    {
        return 2;                                                     /* return error */
    }
    if (handle->inited != 1)                                          /* check handle initialization */
    {
        return 3;                                                     /* return error */
    }
    
    buf[0] = 0xAC;                                                    /* set the addr */
    buf[1] = 0x33;                                                    /* set 0x33 */
    buf[2] = 0x00;                                                    /* set 0x00 */
    if (a_aht20_iic_write(handle, buf, 3) != 0)                       /* write the command */
    {
        handle->debug_print("aht20: sent command failed.\n");         /* sent command failed */
        
        return 1;                                                     /* return error */
    }
    handle->delay_ms(85);                                             /* delay 85ms */
    if (a_aht20_iic_read(handle, &status, 1) != 0)                    /* read the status */
    {
        handle->debug_print("aht20: read status failed.\n");          /* read status failed */
        
        return 1;                                                     /* return error */
    }
    if ((status & 0x80) == 0)                                         /* check the status */
    {
        if (a_aht20_iic_read(handle, buf, 7) != 0)                    /* read data */
        {
            handle->debug_print("aht20: read data failed.\n");        /* read data failed */
            
            return 1;                                                 /* return error */
        }
        if (a_aht20_calc_crc(buf, 6) != buf[6])                       /* check the crc */
        {
            handle->debug_print("aht20: crc is error.\n");            /* crc is error */
            
            return 5;                                                 /* return error */
        }
        
        *humidity_raw = (((uint32_t)buf[1]) << 16) |
                        (((uint32_t)buf[2]) << 8) |
                        (((uint32_t)buf[3]) << 0);                    /* set the humidity */
        *humidity_raw = (*humidity_raw) >> 4;                         /* right shift 4 */
        *humidity_s = (uint8_t)((float)(*humidity_raw)
                                / 1048576.0f * 100.0f);               /* convert the humidity */
        *temperature_raw = (((uint32_t)buf[3]) << 16) |
                           (((uint32_t)buf[4]) << 8) |
                           (((uint32_t)buf[5]) << 0);                 /* set the temperature */
        *temperature_raw = (*temperature_raw) & 0xFFFFF;              /* cut the temperature part */
        *temperature_s = (float)(*temperature_raw) 
                                 / 1048576.0f * 200.0f
                                 - 50.0f;                             /* right shift 4 */
        
        return 0;                                                     /* success return 0 */
    }
    else
    {
        handle->debug_print("aht20: data is not ready.\n");           /* data is not ready */
        
        return 4;                                                     /* return error */
    }
}

/**
 * @brief      read the temperature
 * @param[in]  *handle points to an aht20 handle structure
 * @param[out] *temperature_raw points to a raw temperature buffer
 * @param[out] *temperature_s points to a converted temperature buffer
 * @return     status code
 *             - 0 success
 *             - 1 read temperature failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 data is not ready
 *             - 5 crc is error
 * @note       none
 */
uint8_t aht20_read_temperature(aht20_handle_t *handle, uint32_t *temperature_raw, float *temperature_s)
{
    uint8_t status;
    uint8_t buf[7];
    
    if (handle == NULL)                                               /* check handle */
    {
        return 2;                                                     /* return error */
    }
    if (handle->inited != 1)                                          /* check handle initialization */
    {
        return 3;                                                     /* return error */
    }
    
    buf[0] = 0xAC;                                                    /* set the addr */
    buf[1] = 0x33;                                                    /* set 0x33 */
    buf[2] = 0x00;                                                    /* set 0x00 */
    if (a_aht20_iic_write(handle, buf, 3) != 0)                       /* write the command */
    {
        handle->debug_print("aht20: sent command failed.\n");         /* sent command failed */
        
        return 1;                                                     /* return error */
    }
    handle->delay_ms(85);                                             /* delay 85ms */
    if (a_aht20_iic_read(handle, &status, 1) != 0)                    /* read the status */
    {
        handle->debug_print("aht20: read status failed.\n");          /* read status failed */
        
        return 1;                                                     /* return error */
    }
    if ((status & 0x80) == 0)                                         /* check the status */
    {
        if (a_aht20_iic_read(handle, buf, 7) != 0)                    /* read data */
        {
            handle->debug_print("aht20: read data failed.\n");        /* read data failed */
            
            return 1;                                                 /* return error */
        }
        if (a_aht20_calc_crc(buf, 6) != buf[6])                       /* check the crc */
        {
            handle->debug_print("aht20: crc is error.\n");            /* crc is error */
            
            return 5;                                                 /* return error */
        }
        
        *temperature_raw = (((uint32_t)buf[3]) << 16) |
                           (((uint32_t)buf[4]) << 8) |
                           (((uint32_t)buf[5]) << 0);                 /* set the temperature */
        *temperature_raw = (*temperature_raw) & 0xFFFFF;              /* cut the temperature part */
        *temperature_s = (float)(*temperature_raw) 
                                 / 1048576.0f * 200.0f
                                 - 50.0f;                             /* right shift 4 */
        
        return 0;                                                     /* success return 0 */
    }
    else
    {
        handle->debug_print("aht20: data is not ready.\n");           /* data is not ready */
        
        return 4;                                                     /* return error */
    }
}

/**
 * @brief      read the humidity data
 * @param[in]  *handle points to an aht20 handle structure
 * @param[out] *humidity_raw points to a raw humidity buffer
 * @param[out] *humidity_s points to a converted humidity buffer
 * @return     status code
 *             - 0 success
 *             - 1 read humidity failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 data is not ready
 *             - 5 crc is error
 * @note       none
 */
uint8_t aht20_read_humidity(aht20_handle_t *handle, uint32_t *humidity_raw, uint8_t *humidity_s)
{
    uint8_t status;
    uint8_t buf[7];
    
    if (handle == NULL)                                               /* check handle */
    {
        return 2;                                                     /* return error */
    }
    if (handle->inited != 1)                                          /* check handle initialization */
    {
        return 3;                                                     /* return error */
    }
    
    buf[0] = 0xAC;                                                    /* set the addr */
    buf[1] = 0x33;                                                    /* set 0x33 */
    buf[2] = 0x00;                                                    /* set 0x00 */
    if (a_aht20_iic_write(handle, buf, 3) != 0)                       /* write the command */
    {
        handle->debug_print("aht20: sent command failed.\n");         /* sent command failed */
        
        return 1;                                                     /* return error */
    }
    handle->delay_ms(85);                                             /* delay 85ms */
    if (a_aht20_iic_read(handle, &status, 1) != 0)                    /* read the status */
    {
        handle->debug_print("aht20: read status failed.\n");          /* read status failed */
        
        return 1;                                                     /* return error */
    }
    if ((status & 0x80) == 0)                                         /* check the status */
    {
        if (a_aht20_iic_read(handle, buf, 7) != 0)                    /* read data */
        {
            handle->debug_print("aht20: read data failed.\n");        /* read data failed */
            
            return 1;                                                 /* return error */
        }
        if (a_aht20_calc_crc(buf, 6) != buf[6])                       /* check the crc */
        {
            handle->debug_print("aht20: crc is error.\n");            /* crc is error */
            
            return 5;                                                 /* return error */
        }
        
        *humidity_raw = (((uint32_t)buf[1]) << 16) |
                        (((uint32_t)buf[2]) << 8) |
                        (((uint32_t)buf[3]) << 0);                    /* set the humidity */
        *humidity_raw = (*humidity_raw) >> 4;                         /* right shift 4 */
        *humidity_s = (uint8_t)((float)(*humidity_raw)
                                / 1048576.0f * 100.0f);               /* convert the humidity */
        
        return 0;                                                     /* success return 0 */
    }
    else
    {
        handle->debug_print("aht20: data is not ready.\n");           /* data is not ready */
        
        return 4;                                                     /* return error */
    }
}

/**
 * @brief     set the chip register
 * @param[in] *handle points to an aht20 handle structure
 * @param[in] *buf points to a data buffer
 * @param[in] len is the data buffer length
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t aht20_set_reg(aht20_handle_t *handle, uint8_t *buf, uint16_t len)
{
    if (handle == NULL)                                  /* check handle */
    {
        return 2;                                        /* return error */
    }
    if (handle->inited != 1)                             /* check handle initialization */
    {
        return 3;                                        /* return error */
    } 
    
    if (a_aht20_iic_write(handle, buf, len) != 0)        /* write data */
    {
        return 1;                                        /* return error */
    }
    else
    {
        return 0;                                        /* success return 0 */
    }
}

/**
 * @brief      get the chip register
 * @param[in]  *handle points to an aht20 handle structure
 * @param[out] *buf points to a data buffer
 * @param[in]  len is the data buffer length
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t aht20_get_reg(aht20_handle_t *handle, uint8_t *buf, uint16_t len)
{
    if (handle == NULL)                                 /* check handle */
    {
        return 2;                                       /* return error */
    }
    if (handle->inited != 1)                            /* check handle initialization */
    {
        return 3;                                       /* return error */
    } 
    
    if (a_aht20_iic_read(handle, buf, len) != 0)        /* read data */
    {
        return 1;                                       /* return error */
    }
    else
    {
        return 0;                                       /* success return 0 */
    }
}
