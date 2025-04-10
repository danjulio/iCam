/*
 * Originally part of Infiray "ASIC_2121W_RTOS_SDK_V1.0.0_release".
 * Modified by Dan Julio for use in iCam/iCamMini.
 */
#ifndef FALCON_CMD_
#define FALCON_CMD_

#include <stdint.h>
#include "all_config.h"

/**
 * @file
 *
 * @brief Header file for command of Falcon chips 
 *
 */
#pragma pack (1)

 /**
  * @brief Restore defaut config types
  */
enum vdcmd_driver_type
{
    /// send command by USB
    VDCMD_USB_VDCMD = 0,
    /// send command by I2C_USB
    VDCMD_I2C_USB_VDCMD,
    /// send command by I2C(linux with i2c port only)
    VDCMD_I2C_VDCMD,
    /// send command by UART
    VDCMD_UART_VDCMD
};


 /**
  * @brief Restore defaut config types
  */
enum prop_default_cfg
{
    /// all config
    DEF_CFG_ALL = 0,
    /// tpd config
    DEF_CFG_TPD = 2,
    /// prop page config
    DEF_CFG_PROP_PAGE =3 ,
    /// user config
    DEF_CFG_USER_CFG = 4
};


 /**
  * @brief Property page's defaut parameters types
  */
enum prop_default_params
{
    /// shutter parameters
    PROP_SEL_SHUTTER = 0,
    /// image parameters
    PROP_SEL_IMAGE,
    /// overexposure parameters
    PROP_SEL_OVEREXPOSURE,
    /// tpd parameters
    PROP_SEL_TPD
};


/**
 * @brief Spi config modules
 */
enum prop_spi_cfg_module
{
    /// all modules
    SPI_MOD_CFG_ALL = 0x00,
    /// dead pixel
    SPI_MOD_CFG_DEAD_PIX = 0x05,
    /// property page
    SPI_MOD_CFG_PROPERTY_PAGE = 0x0C,
};


/**
 * @brief Property page's auto shutter parameters types
 */
enum prop_auto_shutter_params
{
    /// Auto shutter update switch. 0:OFF, 1:ON
    SHUTTER_PROP_SWITCH = 0,

    /// Auto shutter minimun interval,to prevent updates too frequently.
    /// unit:1s, range:5-655
    SHUTTER_PROP_MIN_INTERVAL = 1,

    /// Auto shutter maximun interval,shutter updates automatically at this time.
    /// unit:1s, range:5-655
    SHUTTER_PROP_MAX_INTERVAL = 2, 

    /// Vtemp's threshold to trigger B update in auto shutter.Do B update according to 
    /// the temperature difference between latest update.
    /// unit:cnt(36cnt~=1c), range:0-65535
    SHUTTER_PROP_TEMP_THRESHOLD_B = 4,

    /// Shutter anti-fall protection switch. 0:OFF, 1:ON
    SHUTTER_PROP_PROTECT_SWITCH = 5,

    /// Manual shutter to auto shutter(or manual shutter)'s minimun interval,
    /// to prevent updates too frequently.
    /// unit:1s, range:0-655
    SHUTTER_PROP_ANY_INTERVAL = 6,

    /// Shutter anti-fall protection threshold in high gain mode.Range:0-65535.
    SHUTTER_PROTECT_THR_HIGH_GAIN = 7,

    /// Shutter anti-fall protection threshold in low gain mode.Range:0-65535.
    SHUTTER_PROTECT_THR_LOW_GAIN = 8,

    /// Calling shutter's fisrt delay time after preview start.
    /// unit:1s, range:0-655
    SHUTTER_PREVIEW_START_1ST_DELAY = 9,

    /// Calling shutter's delay time after fisrt delay.
    /// unit:1s, range:0-655
    SHUTTER_PREVIEW_START_2ND_DELAY = 10,

    /// Calling shutter's fisrt delay time after change gain.
    /// unit:1s, range:0-655
    SHUTTER_CHANGE_GAIN_1ST_DELAY = 11,

    /// Calling shutter's fisrt delay time after fisrt delay.
    /// unit:1s, range:0-655
    SHUTTER_CHANGE_GAIN_2ND_DELAY = 12

};


enum prop_image_params
{
    /// TNR(Temporal Noise Reduction) level.Range:0-3.
    IMAGE_PROP_LEVEL_TNR = 0,

    /// SNR(Spatial Noise Reduction) level.Range:0-3.
    IMAGE_PROP_LEVEL_SNR = 1,

    /// DDE(digital detail enhance) level.Range:0-6.
    IMAGE_PROP_LEVEL_DDE = 2,

    /// Brightness level. range:0-255
    IMAGE_PROP_LEVEL_BRIGHTNESS = 3,

    /// Contrast level. range:0-255
    IMAGE_PROP_LEVEL_CONTRAST = 4,

    /// AGC(Auto Gain Control) level.Range:0-5.
    IMAGE_PROP_MODE_AGC = 5,

    /// AGC max gain.Range:0-255
    IMAGE_PROP_LEVEL_MAX_GAIN = 6,

    /// AGC BOS(Brightness Offset).Range:0-255
    IMAGE_PROP_LEVEL_BOS = 7,

    /// AGC switch. 0:OFF, 1:ON
    IMAGE_PROP_ONOFF_AGC = 8,

    /// Mirror flip status. 0:no mirror/flip, 1:only mirror the frame,
    /// 2:only flip the frame, 3:mirror and flip the frame
    IMAGE_PROP_SEL_MIRROR_FLIP = 9
};


/**
 * @brief Property page's TPD parameters types
 */
enum prop_tpd_params
{
    /// Distance property. unit:cnt(128cnt=1m), range:0-25600(0-200m)
    TPD_PROP_DISTANCE = 0,

    /// Reflection temperature property. unit:K, range:230-500(high gain), 230-900(low gain)
    TPD_PROP_TU,

    /// Atmospheric temperature property. unit:K, range:230-500(high gain), 230-900(low gain)
    TPD_PROP_TA,

    /// Emissivity property. unit:1/128, range:1-128(0.01-1)
    TPD_PROP_EMS,

    /// Atmospheric transmittance property. unit:1/128, range:1-128(0.01-1)
    TPD_PROP_TAU,

    /// Gain select. 0:low gain, 1:high gain
    TPD_PROP_GAIN_SEL,

    /// Distance correction factors. Unsupported now.
    TPD_PROP_DIST_COR_FACT,

    /// Environment parameters correction enable status. Unsupported now.
    TPD_PROP_ENV_CORRECT_SEL,

    /// Distance correction enable status. Unsupported now.
    TPD_PROP_DIST_CORRECT_SEL,

    /// Temperature mapping status. Unsupported now.
    TPD_PROP_TEMP_MAP_SEL,
};


/**
 * @brief Preview path
 */
enum preview_path
{
    /// preview path 0
    PREVIEW_PATH0 = 0,

    /// preview path 1
    PREVIEW_PATH1
};


/**
 * @brief Device id types
 */
enum device_id_types
{
    /// chip infomation
    DEV_INFO_CHIP_ID = 0,
    /// firmware compile data
    DEV_INFO_FW_COMPILE_DATE = 1,
    /// develop qualification
    DEV_INFO_DEV_QUALIFICATION = 2,
    /// ir sensor id(read aviliable when previewing)
    DEV_INFO_IR_INFO = 3,
    /// project infomation
    DEV_INFO_PROJECT_INFO = 4,
    /// firmware build version
    DEV_INFO_FW_BUILD_VERSION_INFO = 5,
    /// get product number
    DEV_INFO_GET_PN = 6,
    /// get serial number
    DEV_INFO_GET_SN = 7
};


/**
 * @brief Update B/OOC types
 */
enum ooc_b_update_types
{
    /// update B value
    B_UPDATE = 0,
    /// update OOC value
    OOC_UPDATE,
    /// update both B and OOC value
    OOC_B_UPDATE
};


/**
 * @brief Y16 mode source types
 */
enum y16_isp_stream_src_types
{
    /// temperature mode source
    Y16_MODE_TEMPERATURE = 0,
    /// ir sensor mode source
    Y16_MODE_IR,
    Y16_MODE_KBC,
    Y16_MODE_HBC_DPC,
    Y16_MODE_VBC,
    Y16_MODE_TNR,
    Y16_MODE_SNR,
    Y16_MODE_AGC,
    Y16_MODE_DDE,
    Y16_MODE_GAMMA,
    Y16_MODE_MIRROR
};


/**
 * @brief Shutter control enable
 */
enum shutter_sta_types
{
    /// shutter control disable
    SHUTTER_CTL_DIS = 0,
    /// shutter control enable
    SHUTTER_CTL_EN
};


/**
 * @brief Shutter control open/close
 */
enum shutter_manual_types
{
    /// shutter open
    SHUTTER_OPEN = 0,
    /// shutter close
    SHUTTER_CLOSE
};


/**
 * @brief Remove cover enable status
 */
enum rmcover_types
{
    /// remove cover enable
    RMCOVER_DIS = 0,
    /// remove cover disable
    RMCOVER_EN
};


/**
 * @brief Ktbt recalculate point index
 */
enum tpd_ktbt_recal_point_idx
{
    /// the first point
    TPD_KTBT_RECAL_P1 = 0,
    /// the second point
    TPD_KTBT_RECAL_P2
};


/**
 * @brief Tecless temperature area
 */
enum tecless_temp_area_types
{
    /// high temp area
    HIGH_TEMP = 0,
    /// normal temp area
    NORMAL_TEMP,
    /// low temp area
    LOW_TEMP
};


/**
 * @brief Mirror/flip types
 */
enum mirror_flip_types
{
    /// no mirror/flip
    NO_MIRROR_FLIP = 0,
    /// only mirror the frame
    ONLY_MIRROR,
    /// only flip the frame
    ONLY_FLIP,
    /// mirror and flip the frame
    MIRROR_FLIP
};


/**
 * @brief Pseudo color mode types
 */
enum pseudo_color_types
{

    /// pseudo color mode 1:white hot.
    PSEUDO_COLOR_MODE_1=1,
    /// pseudo color mode 2:reserved.
    PSEUDO_COLOR_MODE_2,
    /// pseudo color mode 3:hot.
    PSEUDO_COLOR_MODE_3,
    /// pseudo color mode 4:cold.
    PSEUDO_COLOR_MODE_4,
    /// pseudo color mode 5:sharp.
    PSEUDO_COLOR_MODE_5,
    /// pseudo color mode 6:rainbow.
    PSEUDO_COLOR_MODE_6,
    /// pseudo color mode 7:outdoor.
    PSEUDO_COLOR_MODE_7,
    /// pseudo color mode 8:night.
    PSEUDO_COLOR_MODE_8,
    /// pseudo color mode 9:medical.
    PSEUDO_COLOR_MODE_9,
    /// pseudo color mode 10:testing.
    PSEUDO_COLOR_MODE_10,
    /// pseudo color mode 11:black hot.
    PSEUDO_COLOR_MODE_11
};


/**
 * @brief Zoom scale step value
 */
enum zoom_scale_step
{
    /// zoom step = 1
    ZOOM_STEP1 = 1,
    /// zoom step = 2
    ZOOM_STEP2,
    /// zoom step = 3
    ZOOM_STEP3,
    /// zoom step = 4
    ZOOM_STEP4
};


/**
 * @brief Line struct
 */
typedef struct {
    /// preview path 0/1
    uint8_t path;
    /// preview source 0:sensor 1:fix pattern
    uint8_t source;
    /// preview width
    uint16_t width;
    /// preview height
    uint16_t height;
    /// preview fps
    uint8_t fps;
    /**
     * preview mode
     * 0: VOC_DVP_MODE
     * 1: VOC_ESAV_SMPTE296_MODE
     * 2: VOC_HSYNC_PAL_MODE
     * 3: VOC_ESAV_625LINE_MODE
     * 4: VOC_ESAV_525LINE_MODE
     * 5: VOC_COMPOSITE_PAL_MODE
     * 6: VOC_COMPOSITE_NTSC_MODE
     * 7: Square PAL mode
     * 8: VOSPI_MODE
     * 9: VOC_COMPOSITE_PAL_EXT_DAC_MODE
     */
    uint8_t mode;
}PreviewStartParam_t;


/**
 * @brief Point struct, start from 1
 */
typedef struct {
    /// x position
    uint16_t x;
    /// y position
    uint16_t y;
}IrPoint_t;


/**
 * @brief Line struct, start from 1
 */
typedef struct {
    /// start point's position
    IrPoint_t start_point;
    /// end point's position
    IrPoint_t end_point;
}IrLine_t;


/**
 * @brief Rectangle struct, start from 1
 */
typedef struct {
    /// start point's position
    IrPoint_t start_point;
    /// end point's position
    IrPoint_t end_point;
}IrRect_t;


/**
 * @brief Frame temp index struct
 */
typedef struct
{
    /// maximum temperature point index
    uint8_t max_point_idx;
    /// minimum temperature point index
    uint8_t min_point_idx;
    /// center temperature point index
    uint8_t center_point_idx;
}FrameTempIdx_t;


/**
 * @brief Maximum and minimum temperature information struct
 */
typedef struct
{
    /// maximum temperature value
    uint16_t max_temp;
    /// minimum temperature value
    uint16_t min_temp;
    /// maximum temperature point's position
    IrPoint_t max_temp_point;
    /// minimum temperature point's position
    IrPoint_t min_temp_point;
}MaxMinTempInfo_t;


/**
 * @brief Temperature information struct, including average, maximum,
 * and minimum temperature value.
 */
typedef struct
{
    /// average temperature value
    uint16_t ave_temp;
    /// maximum temperature value
    uint16_t max_temp;
    /// minimum temperature value
    uint16_t min_temp;
}TpdTempInfoValue_t;


/**
 * @brief Temperature information struct of a line or a rectangle.
 */
typedef struct
{
    /// the temperature information
    TpdTempInfoValue_t temp_info_value;
    /// maximum temperature's point position
    IrPoint_t max_temp_point;
    /// minimum temperature's point position
    IrPoint_t min_temp_point;
}TpdLineRectTempInfo_t;


/**
 * @brief Temperature threshold struct, including average, high,
 * and low temperature value's threshold.
 */
typedef struct
{
    /// average threshold
    uint16_t ave_thd;
    /// high threshold
    uint16_t high_thd;
    /// low threshold
    uint16_t low_thd;
}TpdTempThd_t;

#pragma pack()

/**
 * @brief Initialize and register vdcmd's handle by selected driver type
 *
 * @param NULL
 *
 * @return see ir_error_t
 */
ir_error_t vdcmd_init_by_type(enum vdcmd_driver_type type);


/**
 * @brief Load property page's defaut parameters. Recommend rebooting the camera.
 *
 * @param[in] default_param default parameter type
 *
 * @return see ir_error_t
 */
ir_error_t restore_default_cfg(enum prop_default_cfg default_cfg_type);


/**
 * @brief Load property page's defaut parameters
 *
 * @param[in] default_param default parameter type
 *
 * @return see ir_error_t
 */
ir_error_t load_prop_default_params(enum prop_default_params default_param);


/**
 * @brief Set property page's auto shutter parameters
 *
 * @param[in] shutter_param auto shutter's parameter type
 * @param[in] value auto shutter's parameter value
 *
 * @return see ir_error_t
 */
ir_error_t set_prop_auto_shutter_params(enum prop_auto_shutter_params shutter_param, uint16_t value);


/**
 * @brief Get property page's auto shutter parameters
 *
 * @param[in] shutter_param auto shutter's parameter type
 * @param[out] value auto shutter's parameter value
 *
 * @return see ir_error_t
 */
ir_error_t get_prop_auto_shutter_params(enum prop_auto_shutter_params shutter_param, uint16_t* value);


/**
 * @brief Set property page's image parameters
 *
 * @param[in] image_param image's parameter type
 * @param[in] value image's parameter value
 *
 * @return see ir_error_t
 */
ir_error_t set_prop_image_params(enum prop_image_params image_param, uint16_t value);


/**
 * @brief Get property page's image parameters
 *
 * @param[in] image_param image's parameter type
 * @param[out] value image's parameter value
 *
 * @return see ir_error_t
 */
ir_error_t get_prop_image_params(enum prop_image_params image_param, uint16_t* value);



/**
 * @brief Set property page's tpd parameters
 *
 * @param[in] tpd_param tpd's parameter type
 * @param[in] value tpd's parameter value
 *
 * @return see ir_error_t
 */
ir_error_t set_prop_tpd_params(enum prop_tpd_params tpd_param, uint16_t value);


/**
 * @brief Get property page's tpd parameters
 *
 * @param[in] tpd_param tpd's parameter type
 * @param[out] value tpd's parameter value
 *
 * @return see ir_error_t
 */
ir_error_t get_prop_tpd_params(enum prop_tpd_params tpd_param, uint16_t* value);


/**
 * @brief Get device infomation via id_type
 *
 * @param[in] id_type the device's id type
 * @param[out] id_content the device's infomation data
 *
 * @return see ir_error_t
 */
ir_error_t get_device_info(enum device_id_types id_type, uint8_t* id_content);


/**
 * @brief Switch to y16 mode
 *
 * @param[in] path preview path
  * @param[in] src_type y16 mode source type
 *
 * @return see ir_error_t
 */
ir_error_t y16_preview_start(enum preview_path path, enum y16_isp_stream_src_types src_type);


/**
 * @brief Switch from y16 mode to normal mode
 *
 * @param[in] path preview path
 *
 * @return see ir_error_t
 */
ir_error_t y16_preview_stop(enum preview_path path);


/**
 * @brief Start preview via command
 *
 * @param[in] path preview path
 * @param[in] preview_start_info preview start parameters
 *
 * @return see ir_error_t
 */
ir_error_t preview_start(PreviewStartParam_t* preview_start_param);


/**
 * @brief Stop preview via command
 *
 * @param[in] path preview path
 *
 * @return see ir_error_t
 */
ir_error_t preview_stop(enum preview_path path);


/**
 * @brief Update OOC or B value by select update_type
 *
 * @param[in] update_type update types
 *
 * @return see ir_error_t
 */
ir_error_t ooc_b_update(enum ooc_b_update_types update_type);


/**
 * @brief Get shutter's control status
 *
 * @param[out] shutter_en_sta shutter enable status. 0:disable, 1:enable
 * @param[out] shutter_sta shutter status. 0:close, 1:open
 *
 * @return see ir_error_t
 */
ir_error_t shutter_sta_get(uint8_t* shutter_en_sta, uint8_t* shutter_sta);


/**
 * @brief Set shutter's control status
 *
 * @param[in] sta_type shutter status type
 *
 * @return see ir_error_t
 */
ir_error_t shutter_sta_set(enum shutter_sta_types sta_type);


/**
 * @brief Switch shutter's open/close manually
 *
 * @param[in] manual_type shutter's open/close type
 *
 * @return see ir_error_t
 */
ir_error_t shutter_manual_switch(enum shutter_manual_types manual_type);



/**
 * @brief Get current shutter's vtemp.unsupported for tiny module.
 *
 * @param[out] cur_vtemp shutter's vtemp
 *
 * @return see iruvc_error_t
 */
ir_error_t shutter_vtemp_get(uint16_t* cur_vtemp);


/**
 * @brief Get current IR sensor's vtemp
 *
 * @param[out] cur_vtemp vtemp
 *
 * @return see ir_error_t
 */
ir_error_t cur_vtemp_get(uint16_t* cur_vtemp);


/**
 * @brief Set the status of the remove cover.
 *
 * @param[in] en_sts enable status. 0:disable, 1:enable
 *
 * @return see ir_error_t
 */
ir_error_t rmcover_sts_switch(enum rmcover_types en_sts);


/**
 * @brief Auto calculate the remove cover by firmware
 *
 * @param[in] zoom_scale zoom scale.  1/2/4  default 1
 *
 * @return see ir_error_t
 */
ir_error_t rmcover_auto_calc(uint8_t zoom_scale);


/**
 * @brief Get the pseudo color type via preview path
 *
 * @param[in] path preview path
 * @param[out] color_type pseudo color type, see pseudo_color_types, 0 means bypass.
 *
 * @return see ir_error_t
 */
ir_error_t pseudo_color_get(enum preview_path path, uint8_t* color_type);


/**
 * @brief Set the pseudo color type of preview path
 *
 * @param[in] path preview path
 * @param[in] color_type pseudo color type
 *
 * @return see ir_error_t
 */
ir_error_t pseudo_color_set(enum preview_path path, enum pseudo_color_types color_type);



/**
 * @brief Recalculate the kt_bt by 1 point's temperature
 *
 * @param[in] temp the target black body's temperature. units:K(integer)
 *
 * @return see ir_error_t
 */
ir_error_t tpd_ktbt_recal_1point(uint16_t temp);


/**
 * @brief Recalculate the kt_bt by 2 points' temperature.First set point1(low temp),then set point2(high temp).
 *
 * @param[in] point_idx point index
 * @param[in] temp the target black body's temperature. units:K(integer)
 *
 * @return see ir_error_t
 */
ir_error_t tpd_ktbt_recal_2point(enum tpd_ktbt_recal_point_idx point_idx, \
                                              uint16_t temp);


/**
 * @brief Get the maximum temperature of the frame.Unit:1/16 K
 *
 * @param[out] max_temp the maximum temperature of the frame
 *
 * @return see ir_error_t
 */
ir_error_t tpd_get_max_temp(uint16_t* max_temp);


/**
 * @brief Get the minimum temperature of the frame.Unit:1/16 K
 *
 * @param[out] min_temp the minimum temperature of the frame
 *
 * @return see ir_error_t
 */
ir_error_t tpd_get_min_temp(uint16_t* min_temp);


/**
 * @brief Get the maximum and minimum temperature information of the frame.Unit:1/16 K
 *
 * @param[out] max_min_temp_info the maximum and minimum temperature information of the frame
 *
 * @return see ir_error_t
 */
ir_error_t tpd_get_max_min_temp_info(MaxMinTempInfo_t* max_min_temp_info);


/**
 * @brief Get the point temperature information.Unit:1/16 K
 *
 * @param[in] point_pos point's position
 * @param[out] point_temp_value the temperature information of the point
 *
 * @return see ir_error_t
 */
ir_error_t tpd_get_point_temp_info(IrPoint_t point_pos, uint16_t* point_temp_value);


/**
 * @brief Get the line temperature information.Unit:1/16 K
 *
 * @param[in] line_pos line's position
 * @param[out] line_temp_info the temperature information of the line
 *
 * @return see ir_error_t
 */
ir_error_t tpd_get_line_temp_info(IrLine_t line_pos, TpdLineRectTempInfo_t* line_temp_info);


/**
 * @brief Get the rectangle temperature information.Unit:1/16 K
 *
 * @param[in] rect_pos rectangle's position
 * @param[out] rect_temp_info the temperature information of the rectangle
 *
 * @return see ir_error_t
 */
ir_error_t tpd_get_rect_temp_info(IrRect_t rect_pos, TpdLineRectTempInfo_t* rect_temp_info);


/**
 * @brief Zoom up from the center
 *
 * @param[in] path preview path
 * @param[in] scale_step the step of zoom scale
 *
 * @return see ir_error_t
 */
ir_error_t zoom_center_up(enum preview_path path, enum zoom_scale_step scale_step);


/**
 * @brief Zoom down from the center
 *
 * @param[in] path preview path
 * @param[in] scale_step the step of zoom scale
 *
 * @return see ir_error_t
 */
ir_error_t zoom_center_down(enum preview_path path, enum zoom_scale_step scale_step);


/**
 * @brief Zoom up from the point
 *
 * @param[in] path preview path
 * @param[in] scale_step the step of zoom scale
 * @param[in] position position of the point
 *
 * @return see ir_error_t
 */
ir_error_t zoom_position_up(enum preview_path path, enum zoom_scale_step scale_step, IrPoint_t position);


/**
 * @brief Zoom down from the point
 *
 * @param[in] path preview path
 * @param[in] scale_step the step of zoom scale
 * @param[in] position position of the point
 *
 * @return see ir_error_t
 */
ir_error_t zoom_position_down(enum preview_path path, enum zoom_scale_step scale_step, IrPoint_t position);

/**
 * @brief Add dead pixel point temporarily, need to use spi_config_save to save in flash.
 *
 * @param[in] position position of the point
 *
 * @return see ir_error_t
 */
ir_error_t dpc_add_point(IrPoint_t position);

/**
 * @brief Remove dead pixel point temporarily, need to use spi_config_save to save in flash.
 *
 * @param[in] position position of the point
 *
 * @return see ir_error_t
 */
ir_error_t dpc_remove_point(IrPoint_t position);


/**
 * @brief auto calibrate dead pixel and save to flash in firmware.
 *
 * @param[in] wait_time wait time of auto detect function (unit:s)
 *
 * @return see iruvc_error_t
 */
 ir_error_t dpc_auto_calibration(uint16_t wait_time);



/**
 * @brief Read xdata registers from ISP chip
 *
 * @param[in] memory address
 * @param[in] data length
 * @param[out] the data read from memory
 *
 * @return see ir_error_t
 */
ir_error_t xdata_read(uint16_t addr, uint8_t bylen, uint8_t* pbyData);

/**
 * @brief Write xdata registers to ISP chip
 *
 * @param[in] memory address
 * @param[in] data length
 * @param[in] the data write to memory
 *
 * @return see ir_error_t
 */
ir_error_t xdata_write(uint16_t addr, uint8_t bylen, uint8_t* pbyData);

/**
 * @brief Read data from ISP chip with SPI communication
 *
 * @param[in] memory address
 * @param[in] data length
 * @param[out] the data read from memory
 *
 * @return see ir_error_t
 */
ir_error_t spi_read(uint32_t addr, uint16_t wlen, uint8_t* pbyData);

/**
 * @brief Write data to ISP chip with SPI communication
 *
 * @param[in] memory address
 * @param[in] data length
 * @param[in] the data write to memory
 *
 * @return see ir_error_t
 */
ir_error_t spi_write(uint32_t addr, uint16_t wlen, uint8_t* pbyData);

/**
 * @brief Erase sector of ISP chip with SPI communication
 *
 * @param[in] memory address
 * @param[in] sector count that need to erase
 *
 * @return see ir_error_t
 */
ir_error_t spi_erase_sector(uint32_t addr, uint16_t sector_cnt);

/**
 * @brief Erase FW of ISP chip with SPI communication
 *
 * @return see ir_error_t
 */
ir_error_t spi_erase_fw(void);

/**
 * @brief Write status value to ISP chip with SPI communication
 *
 * @param[in] status register type
 * @param[in] status value will write to ISP chip
 *
 * @return see ir_error_t
 */
ir_error_t spi_write_status(uint8_t status_reg_type, uint8_t status_val);

/**
 * @brief Read status value from ISP chip with SPI communication
 *
 * @param[in] status register type
 * @param[out] status value will read from ISP chip
 *
 * @return see ir_error_t
 */
ir_error_t spi_read_status(uint8_t status_reg_type, uint8_t* pstatus_val);

/**
 * @brief Write tag to ISP chip with SPI communication
 *
 * @return see ir_error_t
 */
ir_error_t spi_write_tag(void);

/**
 * @brief Read ID from ISP chip with SPI communication
 *
 * @param[out] the ID will read from ISP chip
 *
 * @return see ir_error_t
 */
ir_error_t spi_read_id(uint8_t* pbyID);

/**
 * @brief Save config to flash
 *
 * @param[in] spi_module config module type
 *
 * @return see ir_error_t
 */
ir_error_t spi_config_save(enum prop_spi_cfg_module spi_module);

/**
 * @brief Load config from the flash
 *
 * @param[in] spi_module config module type
 *
 * @return see ir_error_t
 */
ir_error_t spi_config_load(enum prop_spi_cfg_module spi_module);

/**
 * @brief Reset to update firmware
 *
 * @return see ir_error_t
 */
ir_error_t sys_reset_to_update_fw(void);

/**
 * @brief Reset to rom
 *
 * @return see ir_error_t
 */
ir_error_t sys_reset_to_rom(void);

/**
 * @brief I2C slave read
 *
 * @param[in] memory address
 * @param[in] data length
 * @param[out] the data read from memory
 *
 * @return see ir_error_t
 */
ir_error_t i2c_slave_read(uint16_t addr, uint8_t bylen, uint8_t* pbyData);

#endif
