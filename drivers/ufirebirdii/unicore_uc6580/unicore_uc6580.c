#define DT_DRV_COMPAT unicore_uc6580

#include <errno.h>
#include <zephyr/logging/log.h>

#include "unicore_uc6580.h"

LOG_MODULE_REGISTER(uc6580);

static int uc6580_init(const struct device* dev) {
    const struct uc6580_config* 
}