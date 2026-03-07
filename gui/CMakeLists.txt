cmake_minimum_required(VERSION 3.16)
project(KernelFBSim C)

find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)

# Grab all .c files from your kernel gui directory directly
file(GLOB_RECURSE KERNEL_SRC
    ${CMAKE_SOURCE_DIR}/../arch/x86/kernel/gui/*.c
    ${CMAKE_SOURCE_DIR}/../arch/x86/kernel/lab/*.c
)

add_executable(fbsim
    main.c
    ${KERNEL_SRC}
)

target_link_libraries(fbsim
    SDL2::SDL2
    Threads::Threads
)

target_include_directories(fbsim PRIVATE
    ${CMAKE_SOURCE_DIR}/../arch/x86/kernel/gui
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../arch/x86/kernel
    ${CMAKE_SOURCE_DIR}/
)

target_compile_definitions(fbsim PRIVATE
    FBSIM=1
)