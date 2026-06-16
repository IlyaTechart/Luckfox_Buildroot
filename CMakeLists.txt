cmake_minimum_required(VERSION 3.10)
project(mylogger  C)

set(CMAKE_C_STANDARD 11)
# if (MSVC)
#     # Для компилятора MSVC (Visual Studio)
#     add_compile_options(/W4)          # Уровень предупреждений 4 (максимальный полезный)[reference:2][reference:3]
#     # add_compile_options(/Wall)      # Можно включить все предупреждения, но их может быть слишком много
# else()
#     # Для GCC, Clang и подобных
#     add_compile_options(-Wall -Wextra -Wpedantic) # Основные, дополнительные и строгие предупреждения[reference:4][reference:5]
# endif()
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_FLAGS_DEBUG "-g -O0" CACHE STRING "" FORCE)

add_executable(mylogger main.c threads.c usb_com.c display_data.c rw_file.c epoll.c)
install(TARGETS mylogger RUNTIME DESTINATION bin)