zephyr_library()

zephyr_library_sources_ifdef( CONFIG_GNSS_UC6580
    ufirebirdii_gnss_uc6580.c
)
