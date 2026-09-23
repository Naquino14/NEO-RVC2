#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/lora.h>
#include <nrvc2_security.h>
#include <nrvc2_errno.h>

#include "../roles.h"

#include "comms.h"

LOG_MODULE_REGISTER(comms);

static struct lora_modem_config modem_cfg = {
    .frequency = MHZ(915),
    .bandwidth = BW_500_KHZ,
    .datarate = LORA_DEFAULT_SF,
    .preamble_len = LORA_DEFAULT_PREAMBLE_LEN,
    .coding_rate = CR_4_5,
    .iq_inverted = false,
    .public_network = false,
    .tx_power = LORA_MAX_POW_DBM,
};

// forward decl
/// FUTURE: When updating zephyr, this return type will need to change to int; 
static void comms_receive(const struct device *dev, uint8_t *data, uint16_t size, int16_t rssi, int8_t snr, void *user_data);

static int set_modem_rx() {
    modem_cfg.tx = false;
    int ret = lora_config(role_devs->dev_lora, &modem_cfg);
    if (ret < 0)
        return ret;

    ret = lora_recv_async(role_devs->dev_lora, comms_receive, NULL);
    if (ret < 0)
        return ret;

    return 0;
}

static int set_modem_tx() {
    modem_cfg.tx = true;
    lora_recv_async(role_devs->dev_lora, NULL, NULL);
    return lora_config(role_devs->dev_lora, &modem_cfg);
}

static bool rdy = false;
bool comms_rdy() {
    return rdy;
}

void comms_bit_mode(bool bit_running) {
    // BIT needs to configure the modem with its own settings
    // if the comms system is in async RX mode, the LoRa modem is not 
    // configurable
    if (bit_running && modem_cfg.tx == false)
        set_modem_tx();
    else
        comms_init();
}

K_SEM_DEFINE(modem_sem, 0, 1);

int comms_init() {
    // setup async reception
    int ret = set_modem_rx();
    if (ret < 0) {
        LOG_ERR("%s: Could not put LoRa modem in RX mode: %d", __func__, ret);
        // do not error out device, something else could have happened
        // maybe depending on the error code this could change in the future
        return ret;
    }
    
    k_sem_give(&modem_sem);
    rdy = true;

    return 0;
}


static int shell_comms_tx(const struct shell* shell, size_t argc, char** argv) {
    // string starts at idx 1
    // TEMPORARY: before fully fleshing out this system
    // running commands requires sending the full command up to a point
    static size_t MAX_CMD = 256;
    uint8_t cmdbuf[MAX_CMD];
    size_t cmdlen = 0;

    for (int i = 1; i < argc; i++) {
        // strings in argv will always be null terminated, strlen is ok here
        size_t len = strlen(argv[i]);
        if (cmdlen + len + 1 > MAX_CMD) {
            LOG_WRN("tx shell command: too long!");
            return -EINVAL;
        }
        
        memcpy(cmdbuf + cmdlen, argv[i], len);
        cmdbuf[cmdlen + len] = ' ';
        cmdlen += len + 1;
    }

    cmdbuf[cmdlen - 1] = '\0';

    LOG_INF("shell cmd: %s", cmdbuf);

    return comms_transmit(cmdbuf, cmdlen);
}

int comms_transmit(uint8_t* txbuf, size_t txbuf_len) {
    if (role_devs->dev_lora_stat != DEVSTAT_RDY)
        return -EDEVNOTRDY;

    if (!rdy) 
        return -ENOINIT;

    // wip: queue work and exit immediately
    printk("queueing lora tx work...\n");
    
    return 0;
}

/// FUTURE: When updating zephyr, this return type will need to change to int; 
static void comms_receive(const struct device *dev, uint8_t *data, uint16_t size, int16_t rssi, int8_t snr, void *user_data) {
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_comms,
#if CONFIG_DEVICE_ROLE == 1
    SHELL_CMD(tx, NULL, "Transmit a shell command to the TRC", shell_comms_tx),
#elif CONFIG_DEVICE_ROLE == 2
    SHELL_CMD(tx, NULL, "Transmit a shell command to the FOB", shell_comms_tx),
#endif
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(comms, &sub_comms, "Communications between devices", NULL);