file(GLOB SOURCES *.c)

if(ESP_PLATFORM)
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . )
else()
add_library(cmd STATIC ${SOURCES})
endif()
