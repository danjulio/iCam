file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS .  ../core ../../i2cs
                       REQUIRES i2cs)
