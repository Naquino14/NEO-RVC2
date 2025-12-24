cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(ESPTOOL "/home/naquino14/zephyrproject/.venv/bin/esptool.py")

project(hyundai-remote)

target_sources(app PRIVATE 
    src/main.c
    src/built-in-test.c
    src/roles.c
    src/sys/storage.c
)