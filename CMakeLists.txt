cmake_minimum_required(VERSION 3.10)
project(mylogger  C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_FLAGS_DEBUG "-g -O0" CACHE STRING "" FORCE)

add_executable(mylogger main.c threads.c usb_com.c display_data.c)
install(TARGETS mylogger RUNTIME DESTINATION bin)