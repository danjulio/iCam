file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../esp32_utilities ../icam_mini_specific ../../main ../tiny1c
                       REQUIRES app_update driver freertos main esp32_web gui palettes)

