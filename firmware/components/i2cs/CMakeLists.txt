file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../../main
                       PRIV_REQUIRES esp_driver_i2c)

