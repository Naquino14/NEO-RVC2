#define DT_DRV_COMPAT unicore_uc6580

#include <errno.h>
#include <zephyr/logging/log.h>
#include <drivers/ufirebirdii/ufirebirdii.h>
#include <limits.h>

#include "uc6580.h"

LOG_MODULE_REGISTER(uc6580);

static void uc6580_uart_cb(const struct device* uart, void* user_data) {
    // Do not log here! I learned my lesson the hard way
    const struct device* dev = user_data;
    struct uc6580_data* data = dev->data;
    uint8_t chr;
    
    if (!uart_irq_update(uart)) 
        return; 

    while (uart_irq_rx_ready(uart)) 
        if (uart_fifo_read(uart, &chr, 1) == 1) 
            ring_buf_put(&data->rx_ringbuf, &chr, 1);

    k_work_submit(&data->rx_work);
}

// #define UC6580_SENTENCE_BUF 82
#define UC6580_SENTENCE_BUF 128

static void uc6580_rx_work_handler(struct k_work* work) {
    struct uc6580_data* data = CONTAINER_OF(work, struct uc6580_data, rx_work);
    uint8_t sentence_buf[UC6580_SENTENCE_BUF];
    uint32_t len;

    while ((len = ring_buf_peek(&data->rx_ringbuf, sentence_buf, sizeof(sentence_buf) - 1)) > 0) {
        uint32_t start_idx = UINT_MAX;
        uint32_t end_idx = UINT_MAX;

        for (uint32_t i = 0; i < len; i++) {
            if (sentence_buf[i] == '$' && start_idx == UINT_MAX)
                start_idx = i;
            if (i > 0 && sentence_buf[i] == '\n' && sentence_buf[i - 1] == '\r') {
                end_idx = i;
                break;  // found complete sentence
            }
        }

        if (start_idx != UINT_MAX && end_idx != UINT_MAX && end_idx > start_idx) {
            // discard bytes before '$'
            ring_buf_get(&data->rx_ringbuf, NULL, start_idx);
            
            // read the sentence (from '$' to '\n' inclusive)
            uint32_t sentence_len = end_idx - start_idx + 1;
            ring_buf_get(&data->rx_ringbuf, sentence_buf, sentence_len);
            sentence_buf[sentence_len] = '\0';
            
            ufirebirdii_parse_sentence((char*)sentence_buf, sentence_len, &data->last_fix, &data->devconfig); // discard return code for now
        } else if (end_idx != UINT_MAX && start_idx == UINT_MAX) 
            ring_buf_get(&data->rx_ringbuf, NULL, end_idx + 1); // end but no start, discard
        else 
            break; // wait for more data
    }
}

static int uc6580_init(const struct device* dev) {
    LOG_INF("Initializing UC6580");

    struct uc6580_data* data = dev->data;
    const struct uc6580_config* cfg = dev->config;

    data->devconfig.baud = DT_PROP(DT_PARENT(DT_DRV_INST(0)), current_speed);
    data->devconfig.addr = 0; // UART Mode only supported right now
    data->devconfig.variant = UFBII_VARIANT_UC6580;
    data->devconfig.inpro = 0; // TODO
    data->devconfig.outpro = 0; // TODO

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
    if (!data->last_fix.valid) 
        return -EAGAIN;
    
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
        90,                                                 \
        &uc6580_api);

DT_INST_FOREACH_STATUS_OKAY(UC6580_DEFINE)