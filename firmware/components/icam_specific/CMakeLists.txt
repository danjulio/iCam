file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../cmd ../env ../file ../esp32_utilities ../esp32_web ../../main ../tiny1c
                       REQUIRES esp_driver_i2c gui)
