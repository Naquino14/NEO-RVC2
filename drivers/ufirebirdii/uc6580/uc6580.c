#define DT_DRV_COMPAT unicore_uc6580

#include <errno.h>
#include <zephyr/logging/log.h>
#include <drivers/ufirebirdii.h>

#include "uc6580.h"

LOG_MODULE_REGISTER(uc6580);

static void uc6580_uart_cb(const struct device* uart, void* user_data) {
    const struct device* dev = user_data;
    struct uc6580_data* data = dev->data;
    uint8_t chr;
    
    if (!uart_irq_update(uart)) {
        return;  // No logging in ISR
    }

    while (uart_irq_rx_ready(uart)) {
        if (uart_fifo_read(uart, &chr, 1) == 1) {
            ring_buf_put(&data->rx_ringbuf, &chr, 1);
        }
    }

    k_work_submit(&data->rx_work);
}

#define UC6580_SENTENCE_BUF 256

static void uc6580_rx_work_handler(struct k_work* work) {
    struct uc6580_data* data = CONTAINER_OF(work, struct uc6580_data, rx_work);
    uint8_t sentence_buf[UC6580_SENTENCE_BUF];
    uint32_t len;

    while ((len = ring_buf_get(&data->rx_ringbuf, sentence_buf, sizeof(sentence_buf) - 1)) > 0) {
        sentence_buf[len] = '\0';
        // LOG_INF("RXed (%d): %s", len, sentence_buf);
    }
}

static int uc6580_init(const struct device* dev) {
    LOG_INF("Initializing UC6580");

    struct uc6580_data* data = dev->data;
    const struct uc6580_config* cfg = dev->config;

    // check if uart is ready
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

    // init ring buffer to store incoming data from uc6580 uart
    ring_buf_init(&data->rx_ringbuf, sizeof(data->rx_data), data->rx_data);

    // init workqueue for processing sentences
    k_work_init(&data->rx_work, uc6580_rx_work_handler); 
    k_sem_init(&data->fix_sem, 1, 1);

    uart_irq_callback_user_data_set(cfg->uart, uc6580_uart_cb, (void*)dev);
    uart_irq_rx_enable(cfg->uart);

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

static int uc6580_get_fix(const struct device* dev, struct ufirebirdii_fix* fix) {
    const struct uc6580_data* data = dev->data;
    if (!data->last_fix.valid) {
        return -EAGAIN;
    }
    
    *fix = data->last_fix;
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
    static struct uc6580_data uc6580_data_##inst;           \
    static const struct uc6580_config uc6580_cfg_##inst =   \
        UC6580_CONFIG(inst);                                \
    DEVICE_DT_INST_DEFINE(inst,                             \
        uc6580_init,                                        \
        NULL,                                               \
        &uc6580_data_##inst,                                \
        &uc6580_cfg_##inst,                                 \
        POST_KERNEL,                                        \
        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                 \
        &uc6580_api);

DT_INST_FOREACH_STATUS_OKAY(UC6580_DEFINE)