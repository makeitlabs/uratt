cmake_minimum_required(VERSION 3.10)
project(lvgl)
set(CMAKE_C_STANDARD 11)#C11
set(CMAKE_CXX_STANDARD 17)#C17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

file(GLOB_RECURSE INCLUDES "lv_drivers/*.h" "lvgl/*.h"  "ui_wrapper/*.h" "ui_wrapper/ui/*.h" "./*.h" )
file(GLOB_RECURSE SOURCES  "lv_drivers/*.c" "lvgl/*.c" "ui_wrapper/*.c" "ui_wrapper/ui/ui_*.c" "ui_wrapper/ui/images/*.c" )

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin) 
SET(CMAKE_CXX_FLAGS "-O3")
link_libraries("-lm")

find_package(SDL2 REQUIRED SDL2)
include_directories(${SDL2_INCLUDE_DIRS})
add_executable(main main.c ${SOURCES} ${INCLUDES})
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)
add_compile_definitions(UI_SIMULATOR)
target_link_libraries(main PRIVATE SDL2 )
add_custom_target (run COMMAND ${EXECUTABLE_OUTPUT_PATH}/main)
