#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/sys/byteorder.h>
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

// comms transmit work and workqueue
#define TX_WORK_QUEUE_PRIO 5
#define TX_WORK_QUEUE_SIZE 2048
K_THREAD_STACK_DEFINE(tx_work_stack_area, TX_WORK_QUEUE_SIZE);
static struct k_work_q tx_work_q;
static void comms_transmit_work(struct k_work *item);

#define TX_TIMEOUT_SEC 15
static const k_timeout_t tx_timeout = K_SECONDS(TX_TIMEOUT_SEC);

static const char MAGIC_HEADER[] = "RVC2";
#define MAGIC_HEADER_SIZE (sizeof(MAGIC_HEADER) - 1)

#define MAX_CMD_BUF 128
#define BUF_MAGIC_OFFSET 0
#define BUF_TAG_OFFSET (BUF_MAGIC_OFFSET + MAGIC_HEADER_SIZE)
#define BUF_SEQN_OFFSET (BUF_TAG_OFFSET + NRVC2_SECURITY_TAG_SIZE)
#define BUF_DATA_OFFSET (BUF_SEQN_OFFSET + sizeof(uint64_t))
// max lora buf is the max command buf + tag size + sequence number size + maximum command buffer size
#define MAX_LORA_BUF (MAGIC_HEADER_SIZE + NRVC2_SECURITY_TAG_SIZE + sizeof(uint64_t) + MAX_CMD_BUF)

struct comms_tx_item {
    struct k_work work;
    uint8_t tx_msg[MAX_CMD_BUF];
    size_t len;
};

#define MAX_TX_QUEUE 4
K_MEM_SLAB_DEFINE_STATIC(tx_work_slab, sizeof(struct comms_tx_item), MAX_TX_QUEUE, 4);

// forward decl
/// FUTURE: When updating zephyr, this return type will need to change to int; 
static void comms_receive(const struct device *dev, uint8_t *data, uint16_t size, int16_t rssi, int8_t snr, void *user_data);

/**
 * THIS FUNCTION ASSUMES CALLER HAS TAKEN modem_sem
 */
static int set_modem_rx() {
    if (modem_cfg.tx) {
        modem_cfg.tx = false;
        int ret = lora_config(role_devs->dev_lora, &modem_cfg);
        if (ret < 0)
            return ret;

        ret = lora_recv_async(role_devs->dev_lora, comms_receive, NULL);
        if (ret < 0)
            return ret;
    }

    return 0;
}

/**
 * THIS FUNCTION ASSUMES CALLER HAS TAKEN modem_sem
 */
static int set_modem_tx() {
    if (!modem_cfg.tx) {
        modem_cfg.tx = true;
        lora_recv_async(role_devs->dev_lora, NULL, NULL);
        return lora_config(role_devs->dev_lora, &modem_cfg);
    }
    return 0;
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

    // if the work queue is already initd and started, dont restart it
    if ((tx_work_q.flags & K_WORK_QUEUE_STARTED) == 0) {
        k_work_queue_init(&tx_work_q);
        k_work_queue_start(&tx_work_q, tx_work_stack_area, K_THREAD_STACK_SIZEOF(tx_work_stack_area), TX_WORK_QUEUE_PRIO, NULL);
    }
    
    k_sem_give(&modem_sem);
    rdy = true;

    return 0;
}


static int shell_comms_tx(const struct shell* shell, size_t argc, char** argv) {
    // string starts at idx 1
    // TEMPORARY: before fully fleshing out this system
    // running commands requires sending the full command up to a point
    uint8_t cmdbuf[MAX_CMD_BUF];
    size_t cmdlen = 0;

    // if tx slab full, dont even bother
    if (tx_work_slab.info.num_used == MAX_TX_QUEUE) {
        LOG_WRN("TX Queue full, wait and try again");
        return -EAGAIN;
    }

    for (int i = 1; i < argc; i++) {
        // strings in argv will always be null terminated, strlen is ok here
        size_t len = strlen(argv[i]);
        if (cmdlen + len + 1 > MAX_CMD_BUF) {
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

int comms_transmit(uint8_t* tx_cmd_buf, size_t tx_cmd_buf_len) {
    if (role_devs->dev_lora_stat != DEVSTAT_RDY)
        return -EDEVNOTRDY;

    if (!rdy) 
        return -ENOINIT;

    struct comms_tx_item *tx_item;
    int ret = k_mem_slab_alloc(&tx_work_slab, (void**)&tx_item, K_NO_WAIT);
    if (ret == -ENOMEM) {
        LOG_ERR("TX Queue full");
        return ret;
    } else if (ret < 0 || (ret == 0 && tx_item == NULL) ) {
        // if number is 0, tx_item is null for some reason
        LOG_ERR("Failed to alloc block for TX work item: %d", ret);
        return ret;
    }

    memcpy(tx_item->tx_msg, tx_cmd_buf, tx_cmd_buf_len);
    tx_item->len = tx_cmd_buf_len;
    k_work_init(&tx_item->work, comms_transmit_work);

    ret = k_work_submit_to_queue(&tx_work_q, &tx_item->work);
    if (ret < 0) {
        LOG_ERR("Failed to submit to work queue: %d", ret);
        return ret;
    }
    
    return 0;
}

static void comms_transmit_work(struct k_work *item) {
    // if a lockup occurs here, the work queue stack size is too small :(

    struct comms_tx_item* tx_item = CONTAINER_OF(item, struct comms_tx_item, work);

    // encrypt, sign, and build frame
    const keyopt_t keyopt = ROLE_IS_FOB ? NRVC2_KEYOPT_SESS_COMMS_FOB2TRC : NRVC2_KEYOPT_SESS_COMMS_TRC2FOB;
    uint8_t buf[MAX_LORA_BUF];
    uint8_t tag[NRVC2_SECURITY_TAG_SIZE];
    // there still be people using big endian in 2026 😂
    uint64_t seqn_le = sys_cpu_to_le64(nrvc2_security_get_seqn(keyopt));
    int ret = nrvc2_security_encrypt_and_sign(keyopt, tx_item->tx_msg, tx_item->len, buf + BUF_DATA_OFFSET, tag);
    if (ret < 0) {
        // nrvc2 security will complain for us
        k_mem_slab_free(&tx_work_slab, tx_item);
        return;
    }

    memcpy(buf + BUF_MAGIC_OFFSET, MAGIC_HEADER, MAGIC_HEADER_SIZE);
    memcpy(buf + BUF_TAG_OFFSET, tag, sizeof(tag));
    memcpy(buf + BUF_SEQN_OFFSET, (void*)&seqn_le, sizeof(uint64_t)); 

    ret = k_sem_take(&modem_sem, tx_timeout);
    if (ret == -EAGAIN) {
        LOG_WRN("TX Work: Modem Busy %s", modem_cfg.tx ? "TXing" : "RXing");
        k_mem_slab_free(&tx_work_slab, tx_item);
        return;
    }

    ret = set_modem_tx();
    if (ret < 0) {
        LOG_ERR("TX Work: Cant set TX mode: %d, disabling comms", ret);
        rdy = false; // disabling, dont release semaphore
        k_mem_slab_free(&tx_work_slab, tx_item);
        return;
    }

    ret = lora_send(role_devs->dev_lora, buf, sizeof(buf));
    if (ret < 0) {
        LOG_ERR("TX work LoRa send failed: %d, disabling comms", ret);
        rdy = false; // disabling, dont release semaphore
        k_mem_slab_free(&tx_work_slab, tx_item);
        return;
    }

    k_mem_slab_free(&tx_work_slab, tx_item);

    // go back to receiving if nothing is left in the slab
    if (tx_work_slab.info.num_used == 0)
        set_modem_rx();

    k_sem_give(&modem_sem);
    LOG_INF("TX done");
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