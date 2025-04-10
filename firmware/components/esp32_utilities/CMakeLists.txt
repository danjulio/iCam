file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../cmd ../gui ../i2cs ../icam_specific ../icam_mini_specific ../../main ../tiny1c
                       REQUIRES esp_app_format esp_driver_gpio esp_wifi nvs_flash icam_specific icam_mini_specific esp32_web i2cs gui palettes)

