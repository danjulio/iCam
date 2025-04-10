/**
 * @file touch_driver.c
 */
#if !CONFIG_BUILD_ICAM_MINI

#include "touch_driver.h"
#include "ft6x36.h"


void touch_driver_init()
{
	ft6x36_init(FT6236_I2C_SLAVE_ADDR);
}


bool touch_driver_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    return ft6x36_read(drv, data);
}


bool touch_driver_saw_touch()
{
	return ft6x36_saw_touch();
}

#endif /* CONFIG_BUILD_ICAM_MINI */
