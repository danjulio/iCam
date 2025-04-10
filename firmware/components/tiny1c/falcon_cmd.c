/*
 * Originally part of Infiray "ASIC_2121W_RTOS_SDK_V1.0.0_release".
 * Modified by Dan Julio for use in iCam/iCamMini.
 */
#include "esp_log.h"
#include "falcon_cmd.h"
#include "t1c_i2c_hal.h"
#include <stdio.h>
#include <stdint.h>
#include "vdcmd.h"

#if defined(FALCON_CMD)

static const char* TAG = "falcon_cmd";


typedef struct {
    uint8_t bySdpcDeadPixEn;     //盲元功能使能
    uint8_t bySdpRpEn;           //新增盲元写入盲元表使能
    uint8_t bySdpcDeadPixType;   //盲元类型（勿改）
    uint8_t bySdpcInterpolationTh; //盲元替换时的插值阈值
    uint8_t bySdpcMedianFilterEn;  //中值滤波使能
}SdpcParma_t;


typedef struct {
    uint8_t byBpEn;          //新增单点盲元检测使能
    uint8_t byBpOptEn;		//新增连续盲元检测使能
    int16_t swBpMaxTshd;    //新增单点盲元最大值的算法裕度
    int16_t swBpMinTshd;    //新增单点盲元最小值的算法裕度
    int16_t swBpMaxOptTshd; //新增连续盲元最大值的算法裕度
    int16_t swBpMinOptTshd; //新增连续盲元最小值的算法裕度
    int8_t sbyBpRpMaxOffset; //新增单点盲元最大值的算法偏移
    int8_t sbyBpRpMinOffset; //新增单点盲元最小值的算法偏移
    int8_t sbyBpRpMaxOptOffset;//新增连续盲元最大值的算法偏移
    int8_t sbyBpRpMinOptOffset;//新增连续盲元最小值的算法偏移
}DdpcParma_t;


enum isp_param_module {
    CDC = 0,
    HBC = 1,
    SDPC = 2,
    VBC = 3,
    TNR = 4,
    DUMMY = 5,
    RMVCOVER=6,
    TPD=7,
    SNR_GAUSS=8,
    SNR_BP=9,
    DDPC=10,
    FOCUS=11,
    AGC=12,
    DDE=13,
    GAMMA=14,
    MIRROR_AND_FLIP=15,
    KBC=16,
    OOC=17
};




//register vendor command by selected type
ir_error_t vdcmd_init_by_type(enum vdcmd_driver_type type)
{
    ir_error_t rst;
    rst = vdcmd_rw_handle_register(FALCON_PRODUCT, type);
    return rst;
}


//restore default config
ir_error_t restore_default_cfg(enum prop_default_cfg default_cfg_type)
{
    vdcmd_std_header_t ptVdCmdHeader = { 0 };

    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
    ptVdCmdHeader.bySubCmd = SUBCMD_SPI_DEFAULT_CFG_RESTORE;
    ptVdCmdHeader.byPara = default_cfg_type;
    
    // Set a special poll delay because the Tiny1C may not respond to I2C for a while
    vdcmd_set_special_poll_delay(5000);

    return standard_cmd_write(&ptVdCmdHeader, 0, NULL);
}

//load property page's defaut parameters
ir_error_t load_prop_default_params(enum prop_default_params default_param)
{
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_PROP_PAGE;
    ptVdCmdHeader.bySubCmd = SUBCMD_PROP_LOAD_DEFAULT_PARAM;
    wParam = default_param;

    return long_cmd_write(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 0);
}


//set property page's auto shutter parameters
ir_error_t set_prop_auto_shutter_params(enum prop_auto_shutter_params shutter_param, uint16_t value)
{
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_PROP_PAGE;
    ptVdCmdHeader.bySubCmd = SUBCMD_PROP_AUTO_SHUTTER_PARAM_SET;
    wParam = shutter_param;
    dwAddr1 = (uint32_t)value;


    if (((shutter_param == SHUTTER_PROP_MIN_INTERVAL) && (value < SHUTTER_PROP_MIN_INTERVAL_MIN))\
        || ((shutter_param == SHUTTER_PROP_MIN_INTERVAL) && (value > SHUTTER_PROP_MIN_INTERVAL_MAX)))
    {
        ESP_LOGE(TAG, "parameter MIN_INTERVAL out of range");
        return IR_ERROR_PARAM;
    }

    if (((shutter_param == SHUTTER_PROP_MAX_INTERVAL) && (value < SHUTTER_PROP_MAX_INTERVAL_MIN))\
        || ((shutter_param == SHUTTER_PROP_MAX_INTERVAL) && (value > SHUTTER_PROP_MAX_INTERVAL_MAX)))
    {
        ESP_LOGE(TAG, "parameter MAX_INTERVAL out of range");
        return IR_ERROR_PARAM;
    }

    if ((shutter_param == SHUTTER_PROP_ANY_INTERVAL) && (value > SHUTTER_PROP_ANY_INTERVAL_MAX))
    {
        ESP_LOGE(TAG, "parameter ANY_INTERVAL out of range");
        return IR_ERROR_PARAM;
    }

    if ((shutter_param == SHUTTER_PREVIEW_START_1ST_DELAY) && (value > SHUTTER_PROP_PREVIEW_START_1ST_DELAY_MAX))
    {
        ESP_LOGE(TAG, "parameter PREVIEW_START_1ST_DELAY out of range");
        return IR_ERROR_PARAM;
    }

    if ((shutter_param == SHUTTER_PREVIEW_START_2ND_DELAY) && (value > SHUTTER_PROP_PREVIEW_START_2ND_DELAY_MAX))
    {
        ESP_LOGE(TAG, "parameter PREVIEW_START_2ND_DELAY out of range");
        return IR_ERROR_PARAM;
    }

    if ((shutter_param == SHUTTER_CHANGE_GAIN_1ST_DELAY) && (value > SHUTTER_PROP_CHANGE_GAIN_1ST_DELAY_MAX))
    {
        ESP_LOGE(TAG, "parameter CHANGE_GAIN_1ST_DELAY out of range");
        return IR_ERROR_PARAM;
    }

    if ((shutter_param == SHUTTER_CHANGE_GAIN_2ND_DELAY) && (value > SHUTTER_PROP_CHANGE_GAIN_2ND_DELAY_MAX))
    {
        ESP_LOGE(TAG, "parameter CHANGE_GAIN_2ND_DELAY out of range");
        return IR_ERROR_PARAM;
    }

    
    return long_cmd_write(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 0);
}


//get property page's auto shutter parameters
ir_error_t get_prop_auto_shutter_params(enum prop_auto_shutter_params shutter_param, uint16_t* value)
{
    ir_error_t rst;
    uint8_t data[2] = { 0 };
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_PROP_PAGE;
    ptVdCmdHeader.bySubCmd = SUBCMD_PROP_AUTO_SHUTTER_PARAM_GET;
    wParam = shutter_param;

    rst = long_cmd_read(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 2, data);
    *value = ((uint16_t)data[0] << 8) + data[1];
    return rst;
}


//set property page's image parameters
ir_error_t set_prop_image_params(enum prop_image_params image_param, uint16_t value)
{
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_PROP_PAGE;
    ptVdCmdHeader.bySubCmd = SUBCMD_PROP_IMAGE_PARAM_SET;
    wParam = image_param;
    dwAddr1 = (uint32_t)value;

    if ((image_param == IMAGE_PROP_LEVEL_TNR) && (value > IMG_PROP_TNR_MAX))
    {
        ESP_LOGE(TAG, "parameter TNR out of range");
        return IR_ERROR_PARAM;
    }

    if ((image_param == IMAGE_PROP_LEVEL_SNR) && (value > IMG_PROP_SNR_MAX))
    {
        ESP_LOGE(TAG, "parameter SNR out of range");
        return IR_ERROR_PARAM;
    }

    if ((image_param == IMAGE_PROP_LEVEL_DDE) && (value > IMG_PROP_DDE_MAX))
    {
        ESP_LOGE(TAG, "parameter DDE out of range");
        return IR_ERROR_PARAM;
    }

    if ((image_param == IMAGE_PROP_LEVEL_BRIGHTNESS) && (value > IMG_PROP_BRIGHTNESS_MAX))
    {
        ESP_LOGE(TAG, "parameter BRIGHTNESS out of range");
        return IR_ERROR_PARAM;
    }

    if ((image_param == IMAGE_PROP_LEVEL_CONTRAST) && (value > IMG_PROP_CONTRAST_MAX))
    {
        ESP_LOGE(TAG, "parameter CONTRAST out of range");
        return IR_ERROR_PARAM;
    }

    if ((image_param == IMAGE_PROP_MODE_AGC) && (value > IMG_PROP_AGC_MAX))
    {
        ESP_LOGE(TAG, "parameter AGC out of range");
        return IR_ERROR_PARAM;
    }

    if ((image_param == IMAGE_PROP_LEVEL_MAX_GAIN) && (value > IMG_PROP_AGC_MAXGAIN_MAX))
    {
        ESP_LOGE(TAG, "parameter MAX_GAIN out of range");
        return IR_ERROR_PARAM;
    }

    if ((image_param == IMAGE_PROP_LEVEL_BOS) && (value > IMG_PROP_AGC_BOS_MAX))
    {
        ESP_LOGE(TAG, "parameter BOS out of range");
        return IR_ERROR_PARAM;
    }

    if ((image_param == IMAGE_PROP_SEL_MIRROR_FLIP) && (value > IMG_PROP_MIRROR_FLIP_MAX))
    {
        ESP_LOGE(TAG, "parameter MIRROR_FLIP out of range");
        return IR_ERROR_PARAM;
    }
    return long_cmd_write(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 0);
}


//get property page's image parameters
ir_error_t get_prop_image_params(enum prop_image_params image_param, uint16_t* value)
{
    ir_error_t rst;
    uint8_t data[2] = { 0 };
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_PROP_PAGE;
    ptVdCmdHeader.bySubCmd = SUBCMD_PROP_IMAGE_PARAM_GET;
    wParam = image_param;

    rst = long_cmd_read(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 2, data);
    *value = ((uint16_t)data[0] << 8) + data[1];

    return rst;
}

//set property page's tpd parameters
ir_error_t set_prop_tpd_params(enum prop_tpd_params tpd_param, uint16_t value)
{
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_PROP_PAGE;
    ptVdCmdHeader.bySubCmd = SUBCMD_PROP_TPD_PARAM_SET;
    wParam = tpd_param;
    dwAddr1 = (uint32_t)value;
    if ((tpd_param == TPD_PROP_DISTANCE) && (value > TPD_PARAM_DIST_MAX))
    {
        ESP_LOGE(TAG, "parameter DISTANCE out of range");
        return IR_ERROR_PARAM;
    }

    if ((tpd_param == TPD_PROP_TU) && ((value < TPD_PARAM_TU_MIN) || (value > TPD_PARAM_TU_MAX)))
    {
        ESP_LOGE(TAG, "parameter TU out of range");
        return IR_ERROR_PARAM;
    }

    if ((tpd_param == TPD_PROP_TA)&&((value < TPD_PARAM_TA_MIN) || (value > TPD_PARAM_TA_MAX)))
    {
        ESP_LOGE(TAG, "parameter TA out of range");
        return IR_ERROR_PARAM;
    }

    if ((tpd_param == TPD_PROP_EMS) && ((value < TPD_PARAM_EMS_MIN) || (value > TPD_PARAM_EMS_MAX)))
    {
        ESP_LOGE(TAG, "parameter EMS out of range");
        return IR_ERROR_PARAM;
    }

    if ((tpd_param == TPD_PROP_TAU) && ((value < TPD_PARAM_TAU_MIN) || (value > TPD_PARAM_TAU_MAX)))
    {
        ESP_LOGE(TAG, "parameter TAU out of range");
        return IR_ERROR_PARAM;
    }

    return long_cmd_write(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 0);
}


//get property page's tpd parameters
ir_error_t get_prop_tpd_params(enum prop_tpd_params tpd_param, uint16_t* value)
{
    ir_error_t rst;
    uint8_t data[2] = { 0 };
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    if (tpd_param == TPD_PROP_DIST_COR_FACT)
    {
        *value = 0;
        ESP_LOGE(TAG, "error parameter");
        return IR_ERROR_PARAM;
    }
    else if ((tpd_param == TPD_PROP_ENV_CORRECT_SEL) || (tpd_param == TPD_PROP_DIST_CORRECT_SEL) || \
        (tpd_param == TPD_PROP_TEMP_MAP_SEL))
    {
        *value = 1;
        ESP_LOGE(TAG, "error parameter");
        return IR_ERROR_PARAM;
    }
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_PROP_PAGE;
    ptVdCmdHeader.bySubCmd = SUBCMD_PROP_TPD_PARAM_GET;
    wParam = tpd_param;

    rst = long_cmd_read(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 2, data);
    *value = ((uint16_t)data[0] << 8) + data[1];
    return rst;
}


//get device info
ir_error_t get_device_info(enum device_id_types id_type, uint8_t* id_content)
{
    uint16_t wTmpLen = 64;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };

    if (id_content == NULL)
    {
        ESP_LOGE(TAG, "parameter error");
        return IR_ERROR_PARAM;
    }

    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SYS;
    ptVdCmdHeader.bySubCmd = SUBCMD_SYS_DEV_INFO_GET;
    ptVdCmdHeader.byPara = id_type;

    return standard_cmd_read(&ptVdCmdHeader, wTmpLen, id_content);
}



//switch to y16 mode
ir_error_t y16_preview_start(enum preview_path path, enum y16_isp_stream_src_types src_type)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_Y16_MODE;
    ptVdCmdHeader.bySubCmd = SUBCMD_Y16_MODE_PREVIEW_START;
    ptVdCmdHeader.byPara = path;
    ptVdCmdHeader.byAddr_ll = src_type;
    
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

//quit y16 mode
ir_error_t y16_preview_stop(enum preview_path path)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_Y16_MODE;
    ptVdCmdHeader.bySubCmd = SUBCMD_Y16_MODE_PREVIEW_STOP;
    ptVdCmdHeader.byPara = path;

    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

//start preview via command
ir_error_t preview_start(PreviewStartParam_t* preview_start_param)
{
    uint8_t data[8] = { 0 };
    uint16_t wTmpLen = 8;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_PREVIEW;
    ptVdCmdHeader.bySubCmd = SUBCMD_PREVIEW_START;
    data[0] = preview_start_param->path;
    if (preview_start_param->source == 0)
    {
        data[1] = 0;
    }
    else if (preview_start_param->source == 1)
    {
        data[1] = 0x80;
    }
    data[2] = preview_start_param->width >> 8; 
    data[3] = preview_start_param->width&0xFF;
    data[4] = preview_start_param->height >> 8;
    data[5] = preview_start_param->height & 0xFF;
    data[6] = preview_start_param->fps;
    data[7] = preview_start_param->mode;
    
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, data);
}

//stop preview via command
ir_error_t preview_stop(enum preview_path path)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_PREVIEW;
    ptVdCmdHeader.bySubCmd = SUBCMD_PREVIEW_STOP;
    ptVdCmdHeader.byPara = path;

    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

// ooc or b update
ir_error_t ooc_b_update(enum ooc_b_update_types update_type)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_FW_ISP;
    ptVdCmdHeader.bySubCmd = SUBCMD_FW_ISP_OOC_B_UPDATE;
    ptVdCmdHeader.byPara = update_type;
	
    // Set a special poll delay because the Tiny1C may not respond to I2C for a while
    vdcmd_set_special_poll_delay(3000);
    
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

ir_error_t shutter_sta_get(uint8_t* shutter_en_sta, uint8_t* shutter_sta)
{
    ir_error_t rst;
    uint8_t data[2] = { 0 };
    uint16_t wTmpLen = 2;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SHUTTER;
    ptVdCmdHeader.bySubCmd = SUBCMD_SHUTTER_STA_GET;
    rst = standard_cmd_read(&ptVdCmdHeader, wTmpLen, data);
    *shutter_en_sta = data[0];
    *shutter_sta = data[1];
    return rst;
}

ir_error_t shutter_sta_set(enum shutter_sta_types sta_type)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SHUTTER;
    ptVdCmdHeader.bySubCmd = SUBCMD_SHUTTER_STA_SET;
    ptVdCmdHeader.byPara = sta_type;

    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

ir_error_t shutter_manual_switch(enum shutter_manual_types manual_type)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SHUTTER;
    ptVdCmdHeader.bySubCmd = SUBCMD_SHUTTER_SWITCH;
    ptVdCmdHeader.byPara = manual_type;

    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}


//get current shutter's vtemp
ir_error_t shutter_vtemp_get(uint16_t* cur_vtemp)
{
    int rst = 0;
    uint8_t data[2] = { 0 };
    uint16_t wTmpLen = 2;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };

    if (cur_vtemp == NULL)
    {
        ESP_LOGE(TAG, "parameter error");
        return IR_ERROR_PARAM;
    }

    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SHUTTER;
    ptVdCmdHeader.bySubCmd = SUBCMD_SHUTTER_VTEMP_GET;

    rst = standard_cmd_read(&ptVdCmdHeader, wTmpLen, data);
    *cur_vtemp = ((uint16_t)data[0] << 8) + data[1];
    return rst;
}

//get current IR sensor's vtemp
ir_error_t cur_vtemp_get(uint16_t* cur_vtemp)
{
    ir_error_t rst;
    uint8_t data[2] = { 0 };
    uint16_t wTmpLen = 2;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };

    if (cur_vtemp == NULL)
    {
        ESP_LOGE(TAG, "parameter error");
        return IR_ERROR_PARAM;
    }

    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_FW_ISP;
    ptVdCmdHeader.bySubCmd = SUBCMD_FW_ISP_GET_CUR_VTEMP;

    rst = standard_cmd_read(&ptVdCmdHeader, wTmpLen, data);
    *cur_vtemp = ((uint16_t)data[0] << 8) + data[1];
    return rst;
}


//remove cover status switch
ir_error_t rmcover_sts_switch(enum rmcover_types en_sts)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_FW_ISP;
    if (en_sts == RMCOVER_DIS)
    {
        ptVdCmdHeader.bySubCmd = SUBCMD_FW_ISP_RMCOVER_DISABLE;
    }
    else if(en_sts == RMCOVER_EN)
    {
        ptVdCmdHeader.bySubCmd = SUBCMD_FW_ISP_RMCOVER_ENABLE;
    }
    else
    {
        ESP_LOGE(TAG, "parameter:en_sts error");
        return IR_ERROR_PARAM;
    }

    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

//auto calculate the remove cover by firmware
ir_error_t rmcover_auto_calc(uint8_t zoom_scale)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };

    if ((zoom_scale != 1) && (zoom_scale != 2) && (zoom_scale != 4))
    {
        ESP_LOGE(TAG, "parameter:zoom_scale error");
        return IR_ERROR_PARAM;
    }
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_FW_ISP;
    ptVdCmdHeader.bySubCmd = SUBCMD_FW_ISP_AUTO_CAL_RMCOVER;
    ptVdCmdHeader.byPara = zoom_scale;

    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}


// get pseudo color's index
ir_error_t pseudo_color_get(enum preview_path path, uint8_t* color_type)
{
    uint16_t wTmpLen = 1;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };

    if (color_type == NULL)
    {
        ESP_LOGE(TAG, "parameter error");
        return IR_ERROR_PARAM;
    }

    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_ISP_PARAM;
    ptVdCmdHeader.bySubCmd = SUBCMD_PSEUD_COLOR_GET;
    ptVdCmdHeader.byPara = path;

    return standard_cmd_read(&ptVdCmdHeader, wTmpLen, color_type);
}

// set pseudo color's index
ir_error_t pseudo_color_set(enum preview_path path, enum pseudo_color_types color_type)
{
    uint16_t wTmpLen = 1;
    uint8_t data = color_type;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_ISP_PARAM;
    ptVdCmdHeader.bySubCmd = SUBCMD_PSEUD_COLOR_SET;
    ptVdCmdHeader.byPara = path;
    if ((color_type < PSEUDO_COLOR_MODE_1) || (color_type > PSEUDO_COLOR_MODE_11))
    {
        ESP_LOGE(TAG, "parameter error");
        return IR_ERROR_PARAM;
    }

    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, &data);
}


//recalculate the kt_bt by 1 point's temperature
ir_error_t tpd_ktbt_recal_1point(uint16_t temp)
{
    uint8_t data[2] = { 0 };
    uint16_t wTmpLen = 2;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_TPD;
    ptVdCmdHeader.bySubCmd = SUBCMD_TPD_KTBT_RECAL_1_POINT;
    
    // Set a special poll delay because the Tiny1C may not respond to I2C for a while
    vdcmd_set_special_poll_delay(25000);

    data[0] = temp >> 8;
    data[1] = temp & 0xFF;
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, data);
}

//recalculate the kt_bt by 2 points' temperature
ir_error_t tpd_ktbt_recal_2point(enum tpd_ktbt_recal_point_idx point_idx, uint16_t temp)
{
    uint8_t data[2] = { 0 };
    uint16_t wTmpLen = 2;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_TPD;
    ptVdCmdHeader.bySubCmd = SUBCMD_TPD_KTBT_RECAL_2_POINT;
    if (point_idx == TPD_KTBT_RECAL_P1)
    {
        ptVdCmdHeader.byPara = 0x00;
    }
    else if(point_idx == TPD_KTBT_RECAL_P2)
    {
        ptVdCmdHeader.byPara = 0x01;
    }
    else
    {
        ESP_LOGE(TAG, "error parameter");
        return IR_ERROR_PARAM;
    }
    
    // Set a special poll delay because the Tiny1C may not respond to I2C for a while
    vdcmd_set_special_poll_delay(25000);

    data[0] = temp >> 8;
    data[1] = temp & 0xFF;
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, data);
}

//long command
ir_error_t tpd_get_max_temp(uint16_t* max_temp)
{
    ir_error_t rst;
    uint8_t data[2] = { 0 };
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_TPD;
    ptVdCmdHeader.bySubCmd = SUBCMD_TPD_GET_MAX_TEMP;

    rst = long_cmd_read(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 2, data);
    *max_temp = ((uint16_t)data[0] << 8) + data[1];
    return rst;
}


ir_error_t tpd_get_min_temp(uint16_t* min_temp)
{
    ir_error_t rst;
    uint8_t data[2] = { 0 };
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_TPD;
    ptVdCmdHeader.bySubCmd = SUBCMD_TPD_GET_MIN_TEMP;

    rst = long_cmd_read(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 2, data);
    *min_temp = ((uint16_t)data[0] << 8) + data[1];
    return rst;
}


//get the max and min temp info of the whole frame
ir_error_t tpd_get_max_min_temp_info(MaxMinTempInfo_t* max_min_temp_info)
{
    ir_error_t rst;
    uint8_t data[12] = { 0 };
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_TPD;
    ptVdCmdHeader.bySubCmd = SUBCMD_TPD_GET_MAX_MIN_TEMP_INFO;

    rst = long_cmd_read(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 12, data);
    max_min_temp_info->max_temp = ((uint16_t)data[0] << 8) + data[1];
    max_min_temp_info->min_temp = ((uint16_t)data[2] << 8) + data[3];
    max_min_temp_info->max_temp_point.x = ((uint16_t)data[4] << 8) + data[5];
    max_min_temp_info->max_temp_point.y = ((uint16_t)data[6] << 8) + data[7];
    max_min_temp_info->min_temp_point.x = ((uint16_t)data[8] << 8) + data[9];
    max_min_temp_info->min_temp_point.y = ((uint16_t)data[10] << 8) + data[11];
    return rst;
}

ir_error_t tpd_get_point_temp_info(IrPoint_t point_pos, uint16_t* point_temp_value)
{
    ir_error_t rst;
    uint8_t data[2] = { 0 };
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_TPD;
    ptVdCmdHeader.bySubCmd = SUBCMD_TPD_GET_POINT_TEMP;
    dwAddr1 = (((uint32_t)point_pos.x) << 16) | point_pos.y;
    rst = long_cmd_read(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 2, data);
    *point_temp_value = ((uint16_t)data[0] << 8) + data[1];
    return rst;
}


//get temp info of the line
ir_error_t tpd_get_line_temp_info(IrLine_t line_pos, TpdLineRectTempInfo_t* line_temp_info)
{
    ir_error_t rst;
    uint8_t data[14] = { 0 };
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_TPD;
    ptVdCmdHeader.bySubCmd = SUBCMD_TPD_GET_LINE_TEMP;
    dwAddr1 = (((uint32_t)line_pos.start_point.x) << 16) | line_pos.start_point.y;
    dwAddr2 = (((uint32_t)line_pos.end_point.x) << 16) | line_pos.end_point.y;

    rst = long_cmd_read(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 14, data);
    line_temp_info->temp_info_value.ave_temp = ((uint16_t)data[0] << 8) + data[1];
    line_temp_info->temp_info_value.max_temp = ((uint16_t)data[2] << 8) + data[3];
    line_temp_info->temp_info_value.min_temp = ((uint16_t)data[4] << 8) + data[5];
    line_temp_info->max_temp_point.x = ((uint16_t)data[6] << 8) + data[7];
    line_temp_info->max_temp_point.y = ((uint16_t)data[8] << 8) + data[9];
    line_temp_info->min_temp_point.x = ((uint16_t)data[10] << 8) + data[11];
    line_temp_info->min_temp_point.y = ((uint16_t)data[12] << 8) + data[13];
    return rst;
}


//get temp info of the rect
ir_error_t tpd_get_rect_temp_info(IrRect_t rect_pos, TpdLineRectTempInfo_t* rect_temp_info)
{
    ir_error_t rst;
    uint8_t data[14] = { 0 };
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    uint32_t dwAddr2 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_TPD;
    ptVdCmdHeader.bySubCmd = SUBCMD_TPD_GET_RECT_TEMP;
    dwAddr1 = (((uint32_t)rect_pos.start_point.x) << 16) | rect_pos.start_point.y;
    dwAddr2 = (((uint32_t)rect_pos.end_point.x) << 16) | rect_pos.end_point.y;

    rst = long_cmd_read(&ptVdCmdHeader, wParam, dwAddr1, dwAddr2, 14, data);
    rect_temp_info->temp_info_value.ave_temp = ((uint16_t)data[0] << 8) + data[1];
    rect_temp_info->temp_info_value.max_temp = ((uint16_t)data[2] << 8) + data[3];
    rect_temp_info->temp_info_value.min_temp = ((uint16_t)data[4] << 8) + data[5];
    rect_temp_info->max_temp_point.x = ((uint16_t)data[6] << 8) + data[7];
    rect_temp_info->max_temp_point.y = ((uint16_t)data[8] << 8) + data[9];
    rect_temp_info->min_temp_point.x = ((uint16_t)data[10] << 8) + data[11];
    rect_temp_info->min_temp_point.y = ((uint16_t)data[12] << 8) + data[13];
    return rst;
}

ir_error_t zoom_center_up(enum preview_path path, enum zoom_scale_step scale_step)
{
    uint16_t wParam = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_ZOOM;
    ptVdCmdHeader.bySubCmd = SUBCMD_ZOOM_CENTER_UP;
    wParam = (path << 8) | (scale_step);

    return long_cmd_write(&ptVdCmdHeader, wParam, 0, 0, 0);
}

ir_error_t zoom_center_down(enum preview_path path, enum zoom_scale_step scale_step)
{
    uint16_t wParam = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_ZOOM;
    ptVdCmdHeader.bySubCmd = SUBCMD_ZOOM_CENTER_DOWN;
    wParam = (path << 8) | (scale_step);

    return long_cmd_write(&ptVdCmdHeader, wParam, 0, 0, 0);
}

ir_error_t zoom_position_up(enum preview_path path, enum zoom_scale_step scale_step, IrPoint_t position)
{
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_ZOOM;
    ptVdCmdHeader.bySubCmd = SUBCMD_ZOOM_POSITION_UP;
    wParam = (path << 8) | (scale_step);
    dwAddr1 = (((uint32_t)position.x) << 16) | (position.y);

    return long_cmd_write(&ptVdCmdHeader, wParam, dwAddr1, 0, 0);
}

ir_error_t zoom_position_down(enum preview_path path, enum zoom_scale_step scale_step, IrPoint_t position)
{
    uint16_t wParam = 0;
    uint32_t dwAddr1 = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_ZOOM;
    ptVdCmdHeader.bySubCmd = SUBCMD_ZOOM_POSITION_DOWN;
    wParam = (path << 8) | (scale_step);
    dwAddr1 = (((uint32_t)position.x) << 16) | (position.y);

    return long_cmd_write(&ptVdCmdHeader, wParam, dwAddr1, 0, 0);
}

//add dead pixel point
ir_error_t dpc_add_point(IrPoint_t position)
{
    uint16_t wParam = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_DPC;
    ptVdCmdHeader.bySubCmd = SUBCMD_DPC_ADD_DP;

    return long_cmd_write(&ptVdCmdHeader, wParam, position.x, position.y, 0);
}

//remove dead pixel point
ir_error_t dpc_remove_point(IrPoint_t position)
{
    uint16_t wParam = 0;
    vdcmd_long_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_LONG_TYPE_DPC;
    ptVdCmdHeader.bySubCmd = SUBCMD_DPC_REMOVE_DP;

    return long_cmd_write(&ptVdCmdHeader, wParam, position.x, position.y, 0);
}



ir_error_t dpc_auto_calibration(uint16_t wait_time)
{
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    SdpcParma_t sdpc_param = { 0 };
    DdpcParma_t ddpc_apram = { 0 };
    ir_error_t ret;
    enum isp_param_module isp_module = SDPC;
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_ISP_PARAM;
    ptVdCmdHeader.bySubCmd = SUBCMD_ISP_PARAM_GET;
    ptVdCmdHeader.byPara = isp_module;
    ret = standard_cmd_read(&ptVdCmdHeader, sizeof(sdpc_param), (uint8_t*)&sdpc_param);
    if(ret!= IR_SUCCESS)
    {
        ESP_LOGE(TAG, "read sdpc param failed:%d", ret);
        return ret;
    }

    ptVdCmdHeader.bySubCmd = SUBCMD_ISP_PARAM_SET;
    sdpc_param.bySdpRpEn = 1;
    ret=standard_cmd_write(&ptVdCmdHeader, sizeof(sdpc_param), (uint8_t*)&sdpc_param);
    if (ret != IR_SUCCESS)
    {
        ESP_LOGE(TAG, "write sdpc param failed:%d", ret);
        return ret;
    }


    isp_module = DDPC;
    ptVdCmdHeader.bySubCmd = SUBCMD_ISP_PARAM_GET;
    ptVdCmdHeader.byPara = isp_module;
    ret=standard_cmd_read(&ptVdCmdHeader, sizeof(ddpc_apram), (uint8_t*)&ddpc_apram);
    if (ret != IR_SUCCESS)
    {
        ESP_LOGE(TAG, "read ddpc param failed:%d", ret);
        return ret;
    }

    ptVdCmdHeader.bySubCmd = SUBCMD_ISP_PARAM_SET;
    ddpc_apram.byBpEn = 1;
    ret=standard_cmd_write(&ptVdCmdHeader, sizeof(ddpc_apram), (uint8_t*)&ddpc_apram);
    if (ret != IR_SUCCESS)
    {
        ESP_LOGE(TAG, "write ddpc param failed:%d", ret);
        return ret;
    }
    HAL_Delay(1000*wait_time);

    isp_module = SDPC;
    ptVdCmdHeader.bySubCmd = SUBCMD_ISP_PARAM_SET;
    ptVdCmdHeader.byPara = isp_module;
    sdpc_param.bySdpRpEn = 0;
    ret=standard_cmd_write(&ptVdCmdHeader, sizeof(sdpc_param), (uint8_t*)&sdpc_param);
    if (ret != IR_SUCCESS)
    {
        ESP_LOGE(TAG, "write sdpc param failed:%d", ret);
        return ret;
    }

    isp_module = DDPC;
    ptVdCmdHeader.bySubCmd = SUBCMD_ISP_PARAM_SET;
    ptVdCmdHeader.byPara = isp_module;
    ddpc_apram.byBpEn = 0;
    ret=standard_cmd_write(&ptVdCmdHeader, sizeof(ddpc_apram), (uint8_t*)&ddpc_apram);
    if (ret != IR_SUCCESS)
    {
        ESP_LOGE(TAG, "write ddpc param failed:%d", ret);
        return ret;
    }
    return IR_SUCCESS;
}


ir_error_t xdata_read(uint16_t addr, uint8_t bylen, uint8_t* pbyData)
{
    vdcmd_short_header_t ptVdCmdShortHeader = { 0 };
    ptVdCmdShortHeader.byCmd = CMDTYPE_SHORT_TYPE_XMEM_READ;
    ptVdCmdShortHeader.byLen = bylen;
    ptVdCmdShortHeader.byAddr_h = (uint8_t)(addr >> 8);
    ptVdCmdShortHeader.byAddr_l = (uint8_t)addr;
    return short_cmd_read(&ptVdCmdShortHeader, bylen, pbyData);
}

ir_error_t xdata_write(uint16_t addr, uint8_t bylen, uint8_t* pbyData)
{
    uint8_t byTmpLen = bylen;
    vdcmd_short_header_t ptVdCmdShortHeader = { 0 };
    ptVdCmdShortHeader.byCmd = CMDTYPE_SHORT_TYPE_XMEM_WRITE;
    ptVdCmdShortHeader.byLen = bylen;
    ptVdCmdShortHeader.byAddr_h = (uint8_t)(addr >> 8);
    ptVdCmdShortHeader.byAddr_l = (uint8_t)addr;
    return short_cmd_write(&ptVdCmdShortHeader, byTmpLen, pbyData);
}

ir_error_t spi_read(uint32_t addr, uint16_t wlen, uint8_t* pbyData)
{
    uint16_t wTmpLen = wlen;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
    ptVdCmdHeader.bySubCmd = SUBCMD_SPI_READ;
    ptVdCmdHeader.byPara = (uint8_t)(addr >> 24);
    ptVdCmdHeader.byAddr_h = (uint8_t)(addr >> 16);
    ptVdCmdHeader.byAddr_l = (uint8_t)(addr >> 8);
    ptVdCmdHeader.byAddr_ll = (uint8_t)addr;
    return standard_cmd_read(&ptVdCmdHeader, wTmpLen, pbyData);
}

ir_error_t spi_write(uint32_t addr, uint16_t wlen, uint8_t* pbyData)
{
    uint16_t wTmpLen = wlen;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
    ptVdCmdHeader.bySubCmd = SUBCMD_SPI_WRITE;
    ptVdCmdHeader.byPara = (uint8_t)(addr >> 24);
    ptVdCmdHeader.byAddr_h = (uint8_t)(addr >> 16);
    ptVdCmdHeader.byAddr_l = (uint8_t)(addr >> 8);
    ptVdCmdHeader.byAddr_ll = (uint8_t)addr;
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, pbyData);
}

ir_error_t spi_erase_sector(uint32_t addr, uint16_t sector_cnt)
{
    ir_error_t rst = IR_SUCCESS;
    uint16_t wTmpLen = 0;
    uint16_t one_sector_size = 4096;
    uint16_t once_sector_cnt = 8;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    uint32_t tmp_addr = addr;
    uint16_t i = 0;
    while (sector_cnt>0)
    {
        if (sector_cnt >= once_sector_cnt)
        {
            i = once_sector_cnt;
        }
        else
        {
            i = sector_cnt;
        }

        ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
        ptVdCmdHeader.bySubCmd = SUBCMD_SPI_ERASE;
        ptVdCmdHeader.byPara = (uint8_t)(tmp_addr >> 24);
        ptVdCmdHeader.byAddr_h = (uint8_t)(tmp_addr >> 16);
        ptVdCmdHeader.byAddr_l = (uint8_t)(tmp_addr >> 8);
        ptVdCmdHeader.byAddr_ll = (uint8_t)tmp_addr;
        ptVdCmdHeader.byLen_h = (uint8_t)(i >> 8);
        ptVdCmdHeader.byLen_l = (uint8_t)(i);
        //the wTmpLen must be 0, not i.Because the erase sector needs lens of the header > 256.
        rst = standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
        if (rst < 0)
        {
            return rst;
        }

        tmp_addr += (uint32_t)i * (uint32_t)one_sector_size;
        sector_cnt -= i;
    }
    return rst;
}

ir_error_t spi_erase_fw(void)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
    ptVdCmdHeader.bySubCmd = SUBCMD_SPI_ERASE_FW;
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

ir_error_t spi_write_status(uint8_t status_reg_type, uint8_t status_val)
{
    uint16_t wTmpLen = 1;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    uint8_t  pbyData = status_val;
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
    ptVdCmdHeader.bySubCmd = SUBCMD_SPI_WRSR;
    ptVdCmdHeader.byPara = status_reg_type;
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, &pbyData);
}

ir_error_t spi_read_status(uint8_t status_reg_type, uint8_t* pstatus_val)
{
    uint16_t wTmpLen = 1;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
    ptVdCmdHeader.bySubCmd = SUBCMD_SPI_RDSR;
    ptVdCmdHeader.byPara = status_reg_type;
    return standard_cmd_read(&ptVdCmdHeader, wTmpLen, pstatus_val);
}

ir_error_t spi_write_tag(void)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
    ptVdCmdHeader.bySubCmd = SUBCMD_SPI_WRITE_TAG;
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

ir_error_t spi_read_id(uint8_t* pbyID)
{
    uint16_t wTmpLen = 6;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
    ptVdCmdHeader.bySubCmd = SUBCMD_SPI_READ_ID;
    return standard_cmd_read(&ptVdCmdHeader, wTmpLen, pbyID);
}

ir_error_t spi_config_save(enum prop_spi_cfg_module spi_module)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
    ptVdCmdHeader.bySubCmd = SUBCMD_SPI_CFG_SAVE;
    ptVdCmdHeader.byPara = spi_module;
    
    // Set a special poll delay because the Tiny1C may not respond to I2C for a while
    vdcmd_set_special_poll_delay(5000);
    
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

ir_error_t spi_config_load(enum prop_spi_cfg_module spi_module)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SPI;
    ptVdCmdHeader.bySubCmd = SUBCMD_SPI_CFG_LOAD;
    ptVdCmdHeader.byPara = spi_module;
    if (spi_module == SPI_MOD_CFG_DEAD_PIX) 
    {
        ESP_LOGE(TAG, "error parameter");
        return IR_ERROR_PARAM;
    }
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

ir_error_t sys_reset_to_update_fw(void)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SYS;
    ptVdCmdHeader.bySubCmd = SUBCMD_SYS_RST_TO_UPDATEFW;
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

ir_error_t sys_reset_to_rom(void)
{
    uint16_t wTmpLen = 0;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_SYS;
    ptVdCmdHeader.bySubCmd = SUBCMD_SYS_RST_TO_ROM;
    return standard_cmd_write(&ptVdCmdHeader, wTmpLen, NULL);
}

ir_error_t i2c_slave_read(uint16_t addr, uint8_t bylen, uint8_t* pbyData)
{
    vdcmd_short_header_t ptVdCmdShortHeader = { 0 };
    ptVdCmdShortHeader.byCmd = CMDTYPE_SHORT_TYPE_XMEM_READ;
    ptVdCmdShortHeader.byLen = bylen;
    ptVdCmdShortHeader.byAddr_h = (uint8_t)(addr >> 8);
    ptVdCmdShortHeader.byAddr_l = (uint8_t)(addr);
    return short_cmd_read(&ptVdCmdShortHeader, bylen, pbyData);
}

ir_error_t ir_read_id(uint8_t* sensor_id)
{
    uint16_t wTmpLen = 10;
    vdcmd_std_header_t ptVdCmdHeader = { 0 };
    ptVdCmdHeader.byCmdType = CMDTYPE_STANDARD_TYPE_IR;
    ptVdCmdHeader.bySubCmd = SUBCMD_IR_READ_ID;
    return standard_cmd_read(&ptVdCmdHeader, wTmpLen, sensor_id);
}


#endif
