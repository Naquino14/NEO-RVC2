// Copyright (c) 2026, Nate Aquino 
// SPDX-License-Identifier: Apache-2.0

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include <drivers/ufirebirdii_gnss.h>

#include "ufirebirdii_gnss_uc6580.h"

LOG_MODULE_REGISTER(uc6580, LOG_LEVEL_INF);

static int uc6580_start(const struct device* dev) {
    ARG_UNUSED(dev);
    LOG_INF("Start");
    return 0;
}

static int uc6580_stop(const struct device* dev) {
    ARG_UNUSED(dev);
    LOG_INF("Stop");
}

static int uc6580_get_fix(const struct device* dev, ufirebirdii_gnss_fix_t* fix) {
    struct uc6580_data *data = dev->data;
    k_mutex_lock(&data->lock, K_FOREVER);
    memset(fix, 0, sizeof(ufirebirdii_gnss_fix_t));
    fix->valid = false;
    
    // do stuff
    LOG_INF("Get fix");
    
    k_mutex_unlock(&data->lock);
    return 0;
}

static const struct ufirebirdii_gnss_driver_api uc6580_api = {
    .start = uc6580_start,
    .stop = uc6580_stop,
    .get_fix = uc6580_get_fix
};

static int uc6580_init(const struct device* dev) {
    const struct uc6580_config* cfg = dev->config;
    struct uc6580_data* data = dev->data;
    
    if (!device_is_ready(cfg->uart)) {
        LOG_ERR("UART not ready");
        return -ENODEV;
    }

    k_mutex_init(&data->lock);

    LOG_INF("UC6580 initialized");
    return 0;
}

// dts shenanigans
#define UC6580_DEFINE(inst)                                      \
    static struct uc6580_data uc6580_data_##inst;                \
                                                                 \
    static const struct uc6580_config uc6580_cfg_##inst = {      \
        .uart = DEVICE_DT_GET(DT_INST_BUS(inst)),                \
    };                                                           \
                                                                 \
    DEVICE_DT_INST_DEFINE(inst,                                  \
                          uc6580_init,                           \
                          NULL,                                  \
                          &uc6580_data_##inst,                   \
                          &uc6580_cfg_##inst,                    \
                          POST_KERNEL,                           \
                          CONFIG_KERNEL_INIT_PRIORITY_DEVICE,    \
                          &uc6580_api);

DT_INST_FOREACH_STATUS_OKAY(UC6580_DEFINE)