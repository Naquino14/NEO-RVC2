#include "nrvc2_can.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/can.h>

#include "../roles.h"
#include "../nrvc2_errno.h"

LOG_MODULE_REGISTER(can);

static char* state_tostr(enum can_state state) {
    switch (state) {
        case CAN_STATE_ERROR_ACTIVE:
            return "ERROR_ACTIVE";
        case CAN_STATE_ERROR_WARNING:
            return "ERROR_WARNING";
        case CAN_STATE_ERROR_PASSIVE:
            return "ERROR_PASSIVE";
        case CAN_STATE_BUS_OFF:
            return "BUS_OFF";
        case CAN_STATE_STOPPED:
            return "STOPPED";
        default:
            return "UNKNOWN";
    }
}

int can_init() {
    return 0; 
}

enum can_state get_can_state(const struct device* dev) {
    enum can_state state;
    struct can_bus_err_cnt err_cnt;
    int ret = can_get_state(dev, &state, &err_cnt);
    if (ret != 0) {
        LOG_ERR("Failed to read CAN state from %s: %d", dev->name, ret);
        return CAN_STATE_STOPPED;
    }
    return state;
}