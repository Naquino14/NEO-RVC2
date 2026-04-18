zephyr_library()

zephyr_include_directories(${CMAKE_SOURCE_DIR}/include)
zephyr_library_sources(ufirebirdii.c)

add_subdirectory_ifdef(CONFIG_UC6580 uc6580)