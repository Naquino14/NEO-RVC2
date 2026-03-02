#define DT_DRV_COMPAT unicore_uc6580

#include <errno.h>
#include <zephyr/logging/log.h>
#include <drivers/ufirebirdii.h>

#include "uc6580.h"

LOG_MODULE_REGISTER(uc6580);

static int uc6580_init(const struct device* dev) {
    LOG_ERR("EUREKA!");

    // check if uart is ready
    const struct uc6580_config* cfg = dev->config;
    if (!device_is_ready(cfg->uart)) {
        LOG_ERR("UART device is not ready");
        return -ENODEV;
    }

    // check if pps gpio is ready
    if (cfg->pps.port != NULL && !device_is_ready(cfg->pps.port)) {
        LOG_ERR("PPS GPIO device is not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&cfg->pps, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure PPS GPIO pin: %d", ret);
        return ret;
    }

    return 0;
}

static int uc6580_start(const struct device* dev) {
    LOG_INF("START!");
    return 0;
}

static int uc6580_stop(const struct device* dev) {
    LOG_INF("STOP!");
    return 0;
}

static int uc6580_get_fix(const struct device* dev, ufirebirdii_fix_t* fix) {
    LOG_INF("GET FIX!");
    return 0;
}

static struct ufirebirdii_api uc6580_api = {
    .start = uc6580_start, 
    .stop = uc6580_stop, 
    .get_fix = uc6580_get_fix 
};

#define UC6580_CONFIG(inst)                                 \
{                                                           \
    .uart = DEVICE_DT_GET(DT_INST_PARENT(inst)),            \
    .pps  = GPIO_DT_SPEC_INST_GET_OR(inst, pps_gpios, {0})  \
}                                                           \

#define UC6580_DEFINE(inst)                                 \
    static const struct uc6580_config uc6580_cfg_##inst =   \
        UC6580_CONFIG(inst);                                \
    DEVICE_DT_INST_DEFINE(inst,                             \
        uc6580_init,                                        \
        NULL,                                               \
        NULL,                                               \
        &uc6580_cfg_##inst,                                 \
        POST_KERNEL,                                        \
        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                 \
        &uc6580_api);

DT_INST_FOREACH_STATUS_OKAY(UC6580_DEFINE)