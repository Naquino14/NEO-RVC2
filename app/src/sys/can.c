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

// static void tx_cb(const struct device* dev, int err, void* user_data) {
//     ARG_UNUSED(dev);
//     char* sender = (char*)user_data;
//     if (err < 0) {
//         LOG_ERR("TX on sender %s failed: %d", sender, err);
//         return;
//     }
// }

#define MAX_CAN_MSGS 6
#define CAN_FRAME_FLAG_EFF 0x80
CAN_MSGQ_DEFINE(can0_rx_msgq, MAX_CAN_MSGS);
CAN_MSGQ_DEFINE(can1_rx_msgq, MAX_CAN_MSGS);

struct can_rx_ctx {
    struct k_thread thread;
    const struct device* dev;
    struct k_msgq* msgq;
};

#define RX_THREAD_STACK_SIZE 512
#define RX_THREAD_PRIORITY 2

K_THREAD_STACK_DEFINE(rx_stack_can0, RX_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(rx_stack_can1, RX_THREAD_STACK_SIZE);

static struct can_rx_ctx can0_rx_ctx = {0};
static struct can_rx_ctx can1_rx_ctx = {0};

void poll_rx_thread(void* u1, void* u2, void* u3) {
    struct can_rx_ctx* ctx = (struct can_rx_ctx*)u1;
    ARG_UNUSED(u2);
    ARG_UNUSED(u3);

    const char* name = ctx->dev->name;
    struct can_frame frame;

    for (;;) {
        if (k_msgq_get(ctx->msgq, &frame, K_FOREVER) == 0)
            LOG_INF("%s RXed %s: id=0x%X dlc=%d data=%02X%02X%02X%02X%02X%02X%02X%02X", 
                name,
                frame.flags & CAN_FRAME_FLAG_EFF ? "EXT" : "STD",
                frame.id, frame.dlc,
                frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
    }
}

#define POLL_STATE_SLEEP_MS 100
#define POLL_STATE_THREAD_STACK_SIZE 512
#define POLL_STATE_THREAD_PRIORITY 2

struct can_state_ctx {
    struct k_thread poll_state_thread;
    struct k_work notify_work;
    const struct device* dev;
    enum can_state state;
    struct can_bus_err_cnt err_cnt;
};

K_THREAD_STACK_DEFINE(poll_state_stack_can0, POLL_STATE_THREAD_STACK_SIZE);
struct can_state_ctx can0_state_ctx = {0};

K_THREAD_STACK_DEFINE(poll_state_stack_can1, POLL_STATE_THREAD_STACK_SIZE);
struct can_state_ctx can1_state_ctx = {0};


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
        if (ret != 0) {
            LOG_ERR("Failed to read CAN state from %s: %d", name, ret);
            k_msleep(POLL_STATE_SLEEP_MS);
            continue;
        }

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

void state_change_cb(const struct device* dev, enum can_state state, struct can_bus_err_cnt err_cnt, void* user_data) {
    struct k_work *notify_work = (struct k_work*)user_data;

    if (dev == role_devs->dev_can0) {
        can0_state_ctx.state = state;
        can0_state_ctx.err_cnt = err_cnt;

    }
    else if (dev == role_devs->dev_can1) {
        can1_state_ctx.state = state;
        can1_state_ctx.err_cnt = err_cnt;
    }
    
    
    k_work_submit(notify_work);
}

void state_change_notify_wh(struct k_work* work) {
    struct can_state_ctx* context = CONTAINER_OF(work, struct can_state_ctx, notify_work);
    const char* devname = context->dev->name;
    enum can_state devstate = context->state; 
    struct can_bus_err_cnt dev_err_cnt = context->err_cnt; 
    LOG_INF("%s changed state to %s\nrx err cnt: %d\ntx err cnt: %d",
        devname,
        state_tostr(devstate),
        dev_err_cnt.rx_err_cnt,
        dev_err_cnt.tx_err_cnt);
}

static int add_rx_filters(const struct device* dev, struct k_msgq* msgq) {
    const struct can_filter std_filter = {
        .flags = 0U,
        .id = 0U,
        .mask = 0U
    };
    const struct can_filter ext_filter = {
        .flags = CAN_FILTER_IDE,
        .id = 0U,
        .mask = 0U
    };
    int ret;

    ret = can_add_rx_filter_msgq(dev, msgq, &std_filter);
    if (ret < 0) {
        LOG_WRN("Failed to add STD filter on %s: %d", dev->name, ret);
    }

    ret = can_add_rx_filter_msgq(dev, msgq, &ext_filter);
    if (ret < 0) {
        LOG_WRN("Failed to add EXT filter on %s: %d", dev->name, ret);
    }

    return 0;
}

int can_init() {
    if (role_devs->dev_can0_stat != DEVSTAT_RDY) 
        return -EDEVNOTRDY;

    if (!device_is_ready(role_devs->dev_can0)) {
        LOG_ERR("CAN0 device not ready");
        role_devs->dev_can0_stat = DEVSTAT_ERR;
        return -EDEVNOTRDY;
    }

    int ret = can_start(role_devs->dev_can0);
    if (ret < 0 && ret != -EALREADY) {
        LOG_ERR("Failed to start CAN0: %d", ret);
        role_devs->dev_can0_stat = DEVSTAT_ERR;
        return ret;
    }

    can0_state_ctx.dev = role_devs->dev_can0;
    can0_state_ctx.err_cnt.rx_err_cnt = 0;
    can0_state_ctx.err_cnt.tx_err_cnt = 0;
    k_work_init(&can0_state_ctx.notify_work, state_change_notify_wh);

    can_set_state_change_callback(role_devs->dev_can0, state_change_cb, &can0_state_ctx.notify_work);

    if (role_devs->dev_can1_stat != DEVSTAT_RDY) 
        return -EDEVNOTRDY;

    if (!device_is_ready(role_devs->dev_can1)) {
        LOG_ERR("CAN1 device not ready");
        role_devs->dev_can1_stat = DEVSTAT_ERR;
        return -EDEVNOTRDY;
    }

    ret = can_start(role_devs->dev_can1);
    if (ret < 0 && ret != -EALREADY) {
        LOG_ERR("Failed to start CAN1: %d", ret);
        role_devs->dev_can1_stat = DEVSTAT_ERR;
        return ret;
    }

    can1_state_ctx.dev = role_devs->dev_can1;
    can1_state_ctx.err_cnt.rx_err_cnt = 0;
    can1_state_ctx.err_cnt.tx_err_cnt = 0;
    k_work_init(&can1_state_ctx.notify_work, state_change_notify_wh);

    can_set_state_change_callback(role_devs->dev_can1, state_change_cb, &can1_state_ctx.notify_work);

    // CAN0 rx thread creation
    k_tid_t rx_tid = k_thread_create(&can0_state_ctx.poll_state_thread, poll_state_stack_can0, K_THREAD_STACK_SIZEOF(poll_state_stack_can0),
        (k_thread_entry_t)poll_state_thread, (void*)role_devs->dev_can0, NULL, NULL,
        POLL_STATE_THREAD_PRIORITY, 0, K_NO_WAIT);
    if (!rx_tid) {
        LOG_ERR("Failed to create CAN0 state poll thread");
        return -EAGAIN;
    }

    can0_rx_ctx.dev = role_devs->dev_can0;
    can0_rx_ctx.msgq = &can0_rx_msgq;
    add_rx_filters(role_devs->dev_can0, &can0_rx_msgq);

    rx_tid = k_thread_create(&can0_rx_ctx.thread, rx_stack_can0, K_THREAD_STACK_SIZEOF(rx_stack_can0),
        poll_rx_thread, &can0_rx_ctx, NULL, NULL,
        RX_THREAD_PRIORITY, 0, K_NO_WAIT);
    if (!rx_tid) {
        LOG_ERR("Failed to create CAN0 RX thread");
        return -EAGAIN;
    }

    // CAN1 rx thread creation
    rx_tid = k_thread_create(&can1_state_ctx.poll_state_thread, poll_state_stack_can1, K_THREAD_STACK_SIZEOF(poll_state_stack_can1),
        (k_thread_entry_t)poll_state_thread, (void*)role_devs->dev_can1, NULL, NULL,
        POLL_STATE_THREAD_PRIORITY, 0, K_NO_WAIT);

    if (!rx_tid) {
        LOG_ERR("Failed to create CAN1 state poll thread");
        return -EAGAIN;
    }

    can1_rx_ctx.dev = role_devs->dev_can1;
    can1_rx_ctx.msgq = &can1_rx_msgq;
    add_rx_filters(role_devs->dev_can1, &can1_rx_msgq);

    rx_tid = k_thread_create(&can1_rx_ctx.thread, rx_stack_can1, K_THREAD_STACK_SIZEOF(rx_stack_can1),
        poll_rx_thread, &can1_rx_ctx, NULL, NULL,
        RX_THREAD_PRIORITY, 0, K_NO_WAIT);

    if (!rx_tid) {
        LOG_ERR("Failed to create CAN1 RX thread");
        return -EAGAIN;
    }

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