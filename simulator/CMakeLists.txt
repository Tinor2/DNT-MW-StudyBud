cmake_minimum_required(VERSION 3.10)
project(studybud_sim C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# Find SDL2 manually
find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
find_library(SDL2_LIBRARY SDL2)
find_library(SDL2MAIN_LIBRARY SDL2main)

if(NOT SDL2_INCLUDE_DIR)
    message(FATAL_ERROR "SDL2 not found. Install with: brew install sdl2")
endif()

message(STATUS "SDL2 include: ${SDL2_INCLUDE_DIR}")
message(STATUS "SDL2 library: ${SDL2_LIBRARY}")

# LVGL library
set(LVGL_DIR ${CMAKE_SOURCE_DIR}/lvgl)

file(GLOB_RECURSE LVGL_SOURCES
    ${LVGL_DIR}/src/*.c
)

add_library(lvgl STATIC ${LVGL_SOURCES})

target_include_directories(lvgl PUBLIC
    ${LVGL_DIR}
    ${LVGL_DIR}/src
    ${CMAKE_SOURCE_DIR}
    ${SDL2_INCLUDE_DIR}
)

target_compile_definitions(lvgl PUBLIC
    LV_CONF_INCLUDE_SIMPLE
)

# StudyBud screen code (shared with ESP32 build)
set(SCREEN_DIR ${CMAKE_SOURCE_DIR}/../main/display)

add_library(studybud_screens STATIC
    ${SCREEN_DIR}/screens/screen_home.c
    ${SCREEN_DIR}/screens/screen_menu.c
    ${SCREEN_DIR}/screens/screen_todos.c
    ${SCREEN_DIR}/ui_manager.c
)

target_include_directories(studybud_screens PUBLIC
    ${SCREEN_DIR}
    ${SCREEN_DIR}/lvgl_driver
    ${SCREEN_DIR}/encoder
    ${SCREEN_DIR}/screens
    ${CMAKE_SOURCE_DIR}
    ${LVGL_DIR}
    ${LVGL_DIR}/src
)

target_compile_definitions(studybud_screens PRIVATE
    LV_CONF_INCLUDE_SIMPLE
)

# SDL2 display + encoder driver
add_library(sdl_driver STATIC
    ${CMAKE_SOURCE_DIR}/SDL_Driver.c
)

target_include_directories(sdl_driver PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${SCREEN_DIR}
    ${LVGL_DIR}
    ${LVGL_DIR}/src
    ${SDL2_INCLUDE_DIR}
)

target_compile_definitions(sdl_driver PRIVATE
    LV_CONF_INCLUDE_SIMPLE
)

# Main executable
add_executable(studybud_sim
    ${CMAKE_SOURCE_DIR}/main.c
)

target_include_directories(studybud_sim PRIVATE
    ${SCREEN_DIR}
    ${SCREEN_DIR}/lvgl_driver
    ${SCREEN_DIR}/encoder
    ${SCREEN_DIR}/screens
    ${CMAKE_SOURCE_DIR}
    ${LVGL_DIR}
    ${LVGL_DIR}/src
    ${SDL2_INCLUDE_DIR}
)

target_compile_definitions(studybud_sim PRIVATE
    LV_CONF_INCLUDE_SIMPLE
)

target_link_libraries(studybud_sim
    studybud_screens
    sdl_driver
    lvgl
    ${SDL2_LIBRARY}
    ${SDL2MAIN_LIBRARY}
    m
)
