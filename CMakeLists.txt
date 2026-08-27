cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "ESP32-S3-LCD-2.8C-V1")

# Suppress LVGL 8.2.0 internal enum-enum bitwise OR warning
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=deprecated-enum-enum-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=deprecated-enum-enum-conversion")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(studybud)
