#include <stdio.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include "built-in-test.h"
#include "roles.h"

LOG_MODULE_REGISTER(main);

int main(void) {
    printk("Waking up...\n\n");
    
    k_msleep(2 * 1000);

    bool configured = role_config();
    if (!configured) {
        LOG_ERR("ROLE CFG FAIL");
    }
    
    bit_basic();

    return 0;
}