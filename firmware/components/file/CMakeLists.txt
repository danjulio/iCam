file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../gui ../icam_specific ../icam_mini_specific ../../main ../palettes ../tiny1c ../video
                       REQUIRES esp_driver_gpio esp_driver_sdspi esp_driver_sdmmc fatfs icam_specific icam_mini_specific palettes)

