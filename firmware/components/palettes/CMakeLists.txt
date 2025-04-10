file(GLOB SOURCES *.c)

if(ESP_PLATFORM)
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS .)
else()
add_library(palettes STATIC ${SOURCES})
endif()