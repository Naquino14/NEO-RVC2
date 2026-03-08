#ifndef ZEPHYR_DRIVERS_UFIREBIRDII_UNICORE_UC6580_H
#define ZEPHYR_DRIVERS_UFIREBIRDII_UNICORE_UC6580_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include <drivers/ufirebirdii.h>

struct uc6580_config {
    const struct device* uart;
    struct gpio_dt_spec pps;
};

struct uc6580_data {
    struct ring_buf rx_ringbuf;
    uint8_t rx_data[CONFIG_UC6580_RINGBUFFER_SIZE];
    struct k_work rx_work;
    struct ufirebirdii_fix last_fix;
    struct k_sem fix_sem;    
    struct ufirebirdii_driver_config devconfig;
};

#endif // ZEPHYR_DRIVERS_UFIREBIRDII_UNICORE_UC6580_H