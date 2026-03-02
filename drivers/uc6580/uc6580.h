#ifndef ZEPHYR_DRIVERS_UFIREBIRDII_UNICORE_UC6580_H
#define ZEPHYR_DRIVERS_UFIREBIRDII_UNICORE_UC6580_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include <drivers/ufirebirdii.h>

struct uc6580_config {
    const struct device* uart;
    struct gpio_dt_spec pps;
};

#endif // ZEPHYR_DRIVERS_UFIREBIRDII_UNICORE_UC6580_H