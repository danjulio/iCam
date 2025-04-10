file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS .  ../../../main
                       REQUIRES esp_driver_spi lvgl lvgl_touch lvgl_tft)
