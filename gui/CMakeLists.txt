cmake_minimum_required(VERSION 3.16)
project(KernelFBSim C)

find_package(Threads REQUIRED)

if(APPLE AND NOT SDL2_DIR)
    set(SDL2_DIR "/opt/homebrew/lib/cmake/SDL2" CACHE PATH "Path to SDL2 CMake config (override if needed)")
endif()

find_package(SDL2 REQUIRED)

# Grab all .c files from your kernel gui directory directly
file(GLOB_RECURSE KERNEL_SRC
    ${CMAKE_SOURCE_DIR}/../arch/x86/kernel/gui/*.c
    ${CMAKE_SOURCE_DIR}/../arch/x86/kernel/lab/*.c
)

add_executable(fbsim
    main.c
    guitest.c
    globals.c
    ${KERNEL_SRC}
)

target_link_libraries(fbsim
    SDL2::SDL2
    Threads::Threads
)

target_include_directories(fbsim PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/../arch/x86/kernel/gui
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../arch/x86/kernel
    ${CMAKE_SOURCE_DIR}/
)

target_compile_definitions(fbsim PRIVATE
    FBSIM=1
)