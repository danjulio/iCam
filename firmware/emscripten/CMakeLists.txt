cmake_minimum_required(VERSION 3.12)
project (iCamCntrl-gui)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Oz -s USE_SDL=2")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g -s USE_SDL=2")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lwebsocket.js -sINITIAL_MEMORY=83886080 -sLLD_REPORT_UNDEFINED -sALLOW_MEMORY_GROWTH=1 -Oz")

include_directories(${PROJECT_SOURCE_DIR} ./cmd ./gui ./lvgl ./main)

add_subdirectory(cmd)
add_subdirectory(gui)
add_subdirectory(lvgl)
add_subdirectory(lv_drivers)
add_subdirectory(palettes)

file(GLOB MY_SOURCES ./main/*.c)
set(SOURCES ${MY_SOURCES})

add_executable(index ${SOURCES} ${INCLUDES})

set(CMAKE_EXECUTABLE_SUFFIX ".html")
#target_link_libraries(index)
target_link_libraries(index
	cmd
	gui
    lvgl
    palettes
    lv_drivers
)
set_target_properties(index PROPERTIES LINK_FLAGS "--shell-file ${PROJECT_SOURCE_DIR}/lvgl_shell.html -s SINGLE_FILE=1")
