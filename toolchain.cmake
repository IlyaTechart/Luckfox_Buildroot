set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_BIN "/home/q/SDK_Bildroot/RV110x_linuxSDK/sysdrv/source/buildroot/buildroot-2024.02.10/output/host/bin")
set(CMAKE_SYSROOT "/home/q/SDK_Bildroot/RV110x_linuxSDK/sysdrv/source/buildroot/buildroot-2024.02.10/output/staging")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_BIN}/arm-rockchip830-linux-uclibcgnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN}/arm-rockchip830-linux-uclibcgnueabihf-g++")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)