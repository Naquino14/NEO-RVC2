zephyr_library()

zephyr_include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory_ifdef(CONFIG_UC6580 uc6580)