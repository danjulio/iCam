file(GLOB SOURCES lvgl_tft/*.c lvgl_touch/*.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS lvgl_tft lvgl_touch
                       REQUIRES lvgl)
                       
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_LVGL_H_INCLUDE_SIMPLE")