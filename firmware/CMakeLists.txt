# RBMS - Reptile Habitat Monitor Firmware
# ESP-IDF v5.2+ / ESP32-C6

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/components/sensor
    ${CMAKE_CURRENT_SOURCE_DIR}/components/actuator
    ${CMAKE_CURRENT_SOURCE_DIR}/components/control
    ${CMAKE_CURRENT_SOURCE_DIR}/components/safety
    ${CMAKE_CURRENT_SOURCE_DIR}/components/comm
    ${CMAKE_CURRENT_SOURCE_DIR}/components/power
    ${CMAKE_CURRENT_SOURCE_DIR}/components/config
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rbms_firmware)
