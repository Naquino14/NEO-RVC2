#ifndef UFIREBIRDII_H
#define UFIREBIRDII_H

#include <zephyr/device.h>
#include <zephyr/kernel.h>

typedef struct {
    double latitude;
    double longitude;
    double altitude;
    float hdop;
    uint8_t satellites;
    bool valid;
} ufirebirdii_fix_t;

struct ufirebirdii_api {
    int (*start)(const struct device* dev);
    int (*stop)(const struct device* dev);
    int (*get_fix)(const struct device* dev);
};

static inline int ufirebirdii_start(const struct device* dev) {
    return ((const struct ufirebirdii_api*)dev->api)->start(dev);
}

static inline int ufirebirdii_stop(const struct device* dev) {
    return ((const struct ufirebirdii_api*)dev->api)->stop(dev);
}

static inline int ufirebirdii_get_fix(const struct device* dev, ufirebirdii_fix_t* fix) {
    return ((const struct ufirebirdii_api*)dev->api)->get_fix(dev, fix);
}

#endif // UFIREBIRDII_H