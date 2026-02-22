#define DT_DRV_COMPAT unicore_uc6580

#include <errno.h>
#include <zephyr/logging/log.h>

#include "uc6580.h"

LOG_MODULE_REGISTER(uc6580);

static int uc6580_init(const struct device* dev) {
    const struct uc6580_config* 
}

static const struct device* ic6580_get_uart(const struct device* dev) {
    return DEVICE_DT_GET(DT_PARENT(dev->node));
}

#define UC6580_DEFINE(inst)                                \
    DEVICE_DT_INST_DEFINE(inst,                            \
        uc6580_init,                                       \
        NULL,                                              \
        NULL,                                              \
        NULL,                                              \
        POST_KERNEL,                                       \
        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                \
        NULL);

DT_INST_FOREACH_STATUS_OKAY(UC6580_DEFINE)