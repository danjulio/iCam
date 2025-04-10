file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../esp32_utilities ../esp32_web ../i2cs ../icam_specific ../icam_mini_specific ../tiny1c ../../main ../video
                       REQUIRES esp_driver_spi esp_ringbuf icam_specific icam_mini_specific)
