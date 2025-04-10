file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../aht20 ../esp32_utilities ../VL53L4CX/core ../VL53L4CX/platform ../tiny1c
                       REQUIRES aht20 VL53L4CX)