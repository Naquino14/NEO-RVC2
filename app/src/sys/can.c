#include "can.h"

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

static void tx_cb(const struct device* dev, int err, void* user_data) {
    ARG_UNUSED(dev);
    char* sender = (char*)user_data;
    if (err < 0) {
        LOG_ERR("TX on sender %s failed: %d", sender, err);
        return;
    }
}

void poll_rx_thread(void* u1, void* u2, void* u3) {
    // processes can messages in the queue
    ARG_UNUSED(u1);
    ARG_UNUSED(u2);
    ARG_UNUSED(u3);
}

#define POLL_STATE_SLEEP_MS 100
#define POLL_STATE_THREAD_STACK_SIZE 512
#define POLL_STATE_THREAD_PRIORITY 2
K_THREAD_STACK_DEFINE(poll_state_stack_can0, POLL_STATE_THREAD_STACK_SIZE);
struct k_thread poll_state_thread_can0;
K_THREAD_STACK_DEFINE(poll_state_stack_can1, POLL_STATE_THREAD_STACK_SIZE);
struct k_thread poll_state_thread_can1;

void poll_state_thread(const struct device* can_dev, void* u2, void* u3) {
    ARG_UNUSED(u2);
    ARG_UNUSED(u3);

    const char* name = can_dev->name;

    struct can_bus_err_cnt err_cnt = {0, 0};
    struct can_bus_err_cnt prev_err_cnt = {0, 0};

    enum can_state prev_state = CAN_STATE_ERROR_ACTIVE;
    enum can_state state;
    int ret;

    for (;;) {
        ret = can_get_state(can_dev, &state, &err_cnt);

        if (err_cnt.tx_err_cnt != prev_err_cnt.tx_err_cnt
                || err_cnt.rx_err_cnt != prev_err_cnt.rx_err_cnt
                || prev_state != state) {
			prev_err_cnt.tx_err_cnt = err_cnt.tx_err_cnt;
			prev_err_cnt.rx_err_cnt = err_cnt.rx_err_cnt;
			prev_state = state;

            LOG_INF("node %s moved to %s. TX err: %d, RX err: %d", 
                name, 
                state_tostr(state),
                err_cnt.tx_err_cnt, 
                err_cnt.rx_err_cnt);
        } else 
            k_msleep(POLL_STATE_SLEEP_MS);
    }
}

int can_init() {
    if (role_devs->dev_can0_stat != DEVSTAT_RDY) 
        return -EDEVNOTRDY;

    int ret = can_start(role_devs->dev_can0);
    if (ret < 0 && ret != -EALREADY) {
        LOG_ERR("Failed to start CAN0: %d", ret);
        role_devs->dev_can0_stat = DEVSTAT_ERR;
        return ret;
    }

    // wip
    
    return 0;
}