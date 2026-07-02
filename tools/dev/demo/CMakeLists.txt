cmake_minimum_required(VERSION 3.16)
project(KernelFBSim C)

find_package(Threads REQUIRED)

if(APPLE AND NOT SDL2_DIR)
    set(SDL2_DIR "/opt/homebrew/lib/cmake/SDL2" CACHE PATH "Path to SDL2 CMake config (override if needed)")
endif()

find_package(SDL2 REQUIRED)

if(NOT DEFINED LIB_DIR)
    set(LIB_DIR "${CMAKE_SOURCE_DIR}/../../../usr/lib/libgui")
endif()

file(GLOB_RECURSE GUI_SRC
    ${LIB_DIR}/*.c
)

file(GLOB_RECURSE DEMO_SRC
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/../../../usr/temp/tasks/*.c
)

add_executable(demo
    ${DEMO_SRC}
    ${GUI_SRC}
)

target_link_libraries(demo
    SDL2::SDL2
    Threads::Threads
    m
)

target_include_directories(demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
    ${LIB_DIR}/src
    ${LIB_DIR}/include
    ${CMAKE_SOURCE_DIR}/../../../usr/temp
)

target_compile_definitions(demo PRIVATE
    GUI_DEMO=1
)