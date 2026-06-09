#include "nrvc2_can.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/can.h>

#include "../roles.h"
#include "../nrvc2_errno.h"

#define CAN_BITRATE_KBPS 500
#define CAN_SAMPLE_POINT_PERMILLE 875

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

int can_init(const struct device* dev, const char* devname) {
    struct can_timing timing;
    int ret = can_calc_timing(dev, 
                            &timing, CAN_BITRATE_KBPS * 1000, 
                            CAN_SAMPLE_POINT_PERMILLE);
    if (ret > 0)
        LOG_WRN("Sample point error for %s: %d", devname, ret);
    else if (ret < 0) {
        LOG_ERR("Unable to compute CAN timing for %s: %d", devname, ret);
        return ret;
    }

    ret = can_stop(dev);
    if (ret < 0 && ret != -EALREADY) {
        LOG_ERR("Failed to stop %s device: %d", devname, ret);
        return ret;
    }

    ret = can_set_timing(dev, &timing);
    if (ret < 0) {
        LOG_ERR("Failed to set %s timing: %d", devname, ret);
        return ret;
    }

    ret = can_start(dev);
    if (ret < 0) {
        LOG_ERR("Failed to start %s device: %d", devname, ret);
        return ret;
    }
    return 0; 
}