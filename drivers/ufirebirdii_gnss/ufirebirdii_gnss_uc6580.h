// Copyright (c) 2026, Nate Aquino 
// SPDX-License-Identifier: Apache-2.0

#ifndef GNSS_UC6580_H
#define GNSS_UC6580_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

struct uc6580_config {
    const struct device* uart;
};

struct uc6580_data {
    struct k_mutex lock;
};

#endif // GNSS_UC6580_H
