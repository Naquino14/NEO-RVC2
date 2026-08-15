cmake_minimum_required(VERSION 3.20.0)

add_library(nrvc2_security INTERFACE)

target_sources(nrvc2_security INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nrvc2_security.c
)

target_include_directories(nrvc2_security INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/keys
)

target_link_directories(nrvc2_security INTERFACE zephyr_interface)