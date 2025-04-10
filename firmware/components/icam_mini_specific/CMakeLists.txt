file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../cmd ../env ../file ../esp32_utilities ../esp32_web ../i2cs ../../main ../tiny1c ../video
                       REQUIRES esp_adc esp_driver_gpio)
