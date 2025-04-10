file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../../icam_specific/i2cg ../../../main
                       REQUIRES lvgl)
