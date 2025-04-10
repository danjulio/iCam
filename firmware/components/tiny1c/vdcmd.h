/*
 * Originally part of Infiray "ASIC_2121W_RTOS_SDK_V1.0.0_release".
 * Modified by Dan Julio for use in iCam/iCamMini.
 */
#ifndef VDCMD_
#define VDCMD_

#include <stdint.h>
#include <stddef.h>
#include "all_config.h"
#include "data_rw.h"

enum product_types
{
    FALCON_PRODUCT = 0,
    TINY1B_PRODUCT
};

enum driver_types
{
    USB_VDCMD = 0,
    I2C_USB_VDCMD,
    I2C_VDCMD,
    UART_VDCMD
};



#define TEMP_POINT_CNT_MAX 11

#define TPD_PARAM_DIST_MAX 32767
#define TPD_PARAM_TU_MIN 230
#define TPD_PARAM_TU_MAX 900
#define TPD_PARAM_TA_MIN 230
#define TPD_PARAM_TA_MAX 900
#define TPD_PARAM_EMS_MIN 1
#define TPD_PARAM_EMS_MAX 128
#define TPD_PARAM_TAU_MIN 1
#define TPD_PARAM_TAU_MAX 128

#define IMG_PROP_TNR_MAX 3
#define IMG_PROP_SNR_MAX 3
#define IMG_PROP_DDE_MAX 6
#define IMG_PROP_BRIGHTNESS_MAX 255
#define IMG_PROP_CONTRAST_MAX 255
#define IMG_PROP_AGC_MAX 5
#define IMG_PROP_AGC_MAXGAIN_MAX 255
#define IMG_PROP_AGC_BOS_MAX 255
#define IMG_PROP_MIRROR_FLIP_MAX 3

#define SHUTTER_PROP_MIN_INTERVAL_MIN 5
#define SHUTTER_PROP_MIN_INTERVAL_MAX 655
#define SHUTTER_PROP_MAX_INTERVAL_MIN 5
#define SHUTTER_PROP_MAX_INTERVAL_MAX 655
#define SHUTTER_PROP_TEMP_THR_OOC_MAX 65535
#define SHUTTER_PROP_TEMP_THR_B_MAX 65535
#define SHUTTER_PROP_ANY_INTERVAL_MAX 655
#define SHUTTER_PROP_THR_HIGH_GAIN_MAX 65535
#define SHUTTER_PROP_THR_LOW_GAIN_MAX 65535
#define SHUTTER_PROP_PREVIEW_START_1ST_DELAY_MAX 655
#define SHUTTER_PROP_PREVIEW_START_2ND_DELAY_MAX 655
#define SHUTTER_PROP_CHANGE_GAIN_1ST_DELAY_MAX 655
#define SHUTTER_PROP_CHANGE_GAIN_2ND_DELAY_MAX 655




#define VCMD_STANDARD_TYPE			        0x00
#define VCMD_SHORT_TYPE				        0x80
#define VCMD_LONG_TYPE				        0x10


//byCmdType

#define CMDTYPE_SHORT_TYPE_XMEM_READ        0x80
#define CMDTYPE_SHORT_TYPE_XMEM_WRITE       0x81
#define CMDTYPE_SHORT_TYPE_CCI_ADDR_SET     0x82

#define CMDTYPE_STANDARD_TYPE_SPI           0x01
#define CMDTYPE_STANDARD_TYPE_I2C           0x02
#define CMDTYPE_STANDARD_TYPE_DDR           0x03
#define CMDTYPE_STANDARD_TYPE_SYS           0x05
#define CMDTYPE_STANDARD_TYPE_CAPTURE       0x06
#define CMDTYPE_STANDARD_TYPE_OPEN_PATH     0x07
#define CMDTYPE_STANDARD_TYPE_ISP_DATA      0x08
#define CMDTYPE_STANDARD_TYPE_ISP_PARAM     0x09
#define CMDTYPE_STANDARD_TYPE_Y16_MODE      0x0A
#define CMDTYPE_STANDARD_TYPE_IR            0x0B
#define CMDTYPE_STANDARD_TYPE_SHUTTER       0x0C
#define CMDTYPE_STANDARD_TYPE_FW_ISP        0x0D
#define CMDTYPE_STANDARD_TYPE_TPD           0x0E
#define CMDTYPE_STANDARD_TYPE_PREVIEW       0x0F

#define CMDTYPE_LONG_TYPE_SPI               0x10
#define CMDTYPE_LONG_TYPE_TPD               0x11
#define CMDTYPE_LONG_TYPE_ZOOM              0x12
#define CMDTYPE_LONG_TYPE_DPC               0x13
#define CMDTYPE_LONG_TYPE_PROP_PAGE         0x14


//bySubCmd
//standard cmd
#define SUBCMD_SPI_READ_ID                  0x81
#define SUBCMD_SPI_READ                     0x82
#define SUBCMD_SPI_WRITE                    0xC2
#define SUBCMD_SPI_RDSR                     0x86
#define SUBCMD_SPI_WRSR                     0xC6
#define SUBCMD_SPI_ERASE_FW                 0x07
#define SUBCMD_SPI_ERASE                    0x08
#define SUBCMD_SPI_WRITE_TAG                0x09
#define SUBCMD_SPI_GET_CRC                  0x8A
#define SUBCMD_SPI_CFG_SAVE                 0x0B
#define SUBCMD_SPI_CFG_LOAD                 0x0C
#define SUBCMD_SPI_DEFAULT_CFG_SAVE         0x0D
#define SUBCMD_SPI_DEFAULT_CFG_RESTORE      0x0E

#define SUBCMD_SYS_DEV_INFO_GET             0x84
#define SUBCMD_SYS_DEV_INFO_SET             0xC4
#define SUBCMD_SYS_RST_TO_UPDATEFW          0x07
#define SUBCMD_SYS_RST_TO_ROM               0x08

#define SUBCMD_START_CAPTURE                0x01
#define SUBCMD_GET_CAPTURE_DATA             0x02
#define SUBCMD_EXIT_CAPTURE                 0x03

#define SUBCMD_ISP_PARAM_GET                0x81
#define SUBCMD_ISP_PARAM_SET                0xC1
#define SUBCMD_PSEUD_COLOR_GET              0x84
#define SUBCMD_PSEUD_COLOR_SET              0xC4

#define SUBCMD_Y16_MODE_PREVIEW_START       0x01
#define SUBCMD_Y16_MODE_PREVIEW_STOP        0x02

#define SUBCMD_IR_READ_ID                   0x82

#define SUBCMD_SHUTTER_STA_SET              0x41
#define SUBCMD_SHUTTER_SWITCH               0x42
#define SUBCMD_SHUTTER_STA_GET              0x83
#define SUBCMD_SHUTTER_VTEMP_GET            0x89

#define SUBCMD_FW_ISP_OOC_B_UPDATE          0xC1
#define SUBCMD_FW_ISP_RMCOVER_DISABLE       0x05
#define SUBCMD_FW_ISP_GET_CUR_TEMP_AREA     0x8A
#define SUBCMD_FW_ISP_GET_CUR_VTEMP         0x8B
#define SUBCMD_FW_ISP_JK_CAL                0x0C
#define SUBCMD_FW_ISP_RMCOVER_ENABLE        0x0D
#define SUBCMD_FW_ISP_AUTO_CAL_RMCOVER      0x0E

#define SUBCMD_TPD_E_SET                    0x01
#define SUBCMD_TPD_E_GET                    0x81
#define SUBCMD_TPD_D_SET                    0x02
#define SUBCMD_TPD_D_GET                    0x82
#define SUBCMD_TPD_US_SET                   0x03
#define SUBCMD_TPD_US_GET                   0x83
#define SUBCMD_TPD_KTBT_RECAL_1_POINT       0xC4
#define SUBCMD_TPD_KTBT_RECAL_2_POINT       0xC5
#define SUBCMD_TPD_GAIN_SET                 0x06

#define SUBCMD_PREVIEW_START                0xC1
#define SUBCMD_PREVIEW_STOP                 0x02
#define SUBCMD_PREVIEW_VIDEOMODE_SET        0x03
#define SUBCMD_PREVIEW_VIDEOMODE_GET        0x83

//long cmd
#define TEMP_POINT_CNT_MAX 11
#define TPD_PARAM_DIST_MAX 32767

#define SUBCMD_TPD_GET_MAX_TEMP             0x81
#define SUBCMD_TPD_GET_MIN_TEMP             0x82
#define SUBCMD_TPD_GET_MAX_MIN_TEMP_INFO    0x83
#define SUBCMD_TPD_SET_MAX_MIN_TEMP_THD     0x04
#define SUBCMD_TPD_GET_POINT_TEMP           0x86
#define SUBCMD_TPD_SET_POINT_TEMP_THD       0x07
#define SUBCMD_TPD_GET_LINE_TEMP            0x89
#define SUBCMD_TPD_SET_LINE_TEMP_THD        0x0A
#define SUBCMD_TPD_GET_RECT_TEMP            0x8C
#define SUBCMD_TPD_SET_RECT_TEMP_THD        0x0D
#define SUBCMD_TPD_SHOW_POINT_TEMP          0x0F
#define SUBCMD_TPD_HIDE_POINT_TEMP          0x10
#define SUBCMD_TPD_SHOW_LINE_TEMP           0x11
#define SUBCMD_TPD_HIDE_LINE_TEMP           0x12
#define SUBCMD_TPD_SHOW_RECT_TEMP           0x13
#define SUBCMD_TPD_HIDE_RECT_TEMP           0x14
#define SUBCMD_TPD_SHOW_FRAME_TEMP          0x15
#define SUBCMD_TPD_HIDE_FRAME_TEMP          0x16

#define SUBCMD_ZOOM_CENTER_UP               0x01
#define SUBCMD_ZOOM_CENTER_DOWN             0x02
#define SUBCMD_ZOOM_POSITION_UP             0x03
#define SUBCMD_ZOOM_POSITION_DOWN           0x04

#define SUBCMD_DPC_ADD_DP                   0x01
#define SUBCMD_DPC_REMOVE_DP                0x02

#define SUBCMD_PROP_LOAD_DEFAULT_PARAM         0x01
#define SUBCMD_PROP_AUTO_SHUTTER_PARAM_SET     0xC2
#define SUBCMD_PROP_AUTO_SHUTTER_PARAM_GET     0x82
#define SUBCMD_PROP_IMAGE_PARAM_SET            0xC3
#define SUBCMD_PROP_IMAGE_PARAM_GET            0x83
#define SUBCMD_PROP_OVER_EXPOSURE_SET          0xC4
#define SUBCMD_PROP_OVER_EXPOSURE_GET          0x84
#define SUBCMD_PROP_TPD_PARAM_SET              0xC5
#define SUBCMD_PROP_TPD_PARAM_GET              0x85


//typedef int (*handle_func)(uint32_t param,...);
typedef ir_error_t (*handle_func)(uint16_t byI2CSlaveID, uint16_t wI2CRegAddr, uint16_t wLen, uint8_t* pbyData);
typedef void (*delay_func)(uint16_t delay_ms);

typedef struct {
    handle_func handle;
    delay_func delay;
    uint16_t slave_id;
}vdcmd_rw_handle_t;

ir_error_t vdcmd_rw_handle_register(enum product_types product_type, enum driver_types driver_type);

ir_error_t short_cmd_read(vdcmd_short_header_t* ptVdCmdShortHeader, uint8_t byLen, uint8_t* pbyData);

ir_error_t short_cmd_write(vdcmd_short_header_t* ptVdCmdShortHeader, uint8_t byLen, uint8_t* pbyData);

ir_error_t standard_cmd_read(vdcmd_std_header_t* ptVdCmdHeader, uint16_t wLen, uint8_t* pbyData);

ir_error_t standard_cmd_write(vdcmd_std_header_t* ptVdCmdHeader, uint16_t wLen, uint8_t* pbyData);

ir_error_t long_cmd_read(vdcmd_long_header_t* ptVdCmdHeader, uint16_t wParam, uint32_t dwAddr1, \
    uint32_t dwAddr2, uint32_t dwLen, uint8_t data[]);

ir_error_t long_cmd_write(vdcmd_long_header_t* ptVdCmdHeader, uint16_t wParam, uint32_t dwAddr1, \
    uint32_t dwAddr2, uint32_t dwLen);

void vdcmd_set_special_poll_delay(uint16_t delay_ms);
#endif
