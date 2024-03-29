# Copyright (c) 2023, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

cmake_minimum_required(VERSION 3.7.2)
list(APPEND CMAKE_MODULE_PATH $ENV{KOS_CORE_PATH})
set(CMAKE_C_COMPILER $ENV{KOS_CC})
find_package(kos_core REQUIRED)

set(CMAKE_TOOLCHAIN_FILE ${KOS_RUNTIME_PATH}/share/toolchain.cmake)

project(kos_am335x_starterware C)

add_executable(kos_am335x_gpio ${CMAKE_CURRENT_LIST_DIR}/c_src/gpio/kos_am335x_gpio.c ${CMAKE_CURRENT_LIST_DIR}/c_src/gpio/gpio_v2.c)
target_include_directories(kos_am335x_gpio PRIVATE c_src/gpio)
target_compile_options(kos_am335x_gpio PRIVATE -fPIC)
target_link_options(kos_am335x_gpio PRIVATE -static)

add_executable(kos_am335x_pwm ${CMAKE_CURRENT_LIST_DIR}/c_src/pwm/kos_am335x_pwm.c ${CMAKE_CURRENT_LIST_DIR}/c_src/pwm/ehrpwm.c)
target_include_directories(kos_am335x_pwm PRIVATE c_src/pwm)
target_compile_options(kos_am335x_pwm PRIVATE -fPIC)
target_link_options(kos_am335x_pwm PRIVATE -static)
