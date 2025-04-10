file(GLOB SOURCES *.c)

if(ESP_PLATFORM)
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../cmd ../lvgl ../palettes ../tiny1c
                       REQUIRES main esp32_utilities esp32_web icam_mini_specific icam_specific tiny1c)
else()
include_directories(../cmd ../lvgl ../palettes)
add_library(gui STATIC ${SOURCES})
endif()
