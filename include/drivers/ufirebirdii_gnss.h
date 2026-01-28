// Copyright (c) 2026, Nate Aquino 
// SPDX-License-Identifier: Apache-2.0

#ifndef UFIREBIRDII_GNSS_H
#define UFIREBIRDII_GNSS_H

#include <zephyr/device.h>
#include <zephyr/kernel.h>

typedef struct {
    double latitude;
    double longitude;
    double altitude;
    float hdop;
    uint8_t satellites;
    bool valid;
} ufirebirdii_gnss_fix_t;

struct ufirebirdii_gnss_driver_api {
    int (*start)(const struct device* dev);
    int (*stop)(const struct device* dev);
    int (*get_fix)(const struct device* dev, ufirebirdii_gnss_fix_t* fix);
};

static inline int ufirebirdii_gnss_start(const struct device* dev) {
    return ((const struct ufirebirdii_gnss_driver_api*)dev->api)->start(dev);
}

static inline int ufirebirdii_gnss_get_fix(const struct device* dev, ufirebirdii_gnss_fix_t* fix) {
    return ((const struct ufirebirdii_gnss_driver_api*)dev->api)->get_fix(dev, fix);
}

#endif