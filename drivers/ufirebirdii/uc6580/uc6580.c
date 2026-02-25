#define DT_DRV_COMPAT unicore_uc6580

#include <errno.h>
#include <zephyr/logging/log.h>

#include "uc6580.h"

LOG_MODULE_REGISTER(uc6580);

static int uc6580_init(const struct device* dev) {
    LOG_ERR("EUREKA!");
    return 0;
}

static const struct device* uc6580_get_uart(const struct device* dev) {
    const struct uc6580_config* cfg = dev->config;
    return cfg->uart;
}

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
        NULL);

DT_INST_FOREACH_STATUS_OKAY(UC6580_DEFINE)