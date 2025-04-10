file(GLOB SOURCES core/*.c platform/*.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS core platform
                       REQUIRES i2cs)
