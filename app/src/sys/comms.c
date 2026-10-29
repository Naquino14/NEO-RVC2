#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
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

static const char MAGIC_HEADER[] = "NRVC2";
// no -1 here because there is 1 char for the type of transmission
// ex: ack is "NRVC2a"
#define MAGIC_HEADER_SIZE sizeof(MAGIC_HEADER)

#define MAX_CMD_BUF 128
#define BUF_MAGIC_OFFSET 0
#define BUF_MAGIC_TYPE_OFFSET (MAGIC_HEADER_SIZE - 1)
#define BUF_TAG_OFFSET (BUF_MAGIC_OFFSET + MAGIC_HEADER_SIZE)
#define BUF_SEQN_OFFSET (BUF_TAG_OFFSET + NRVC2_SECURITY_TAG_SIZE)
#define BUF_DATA_OFFSET (BUF_SEQN_OFFSET + sizeof(uint64_t))
// max lora buf is the max command buf + tag size + sequence number size + maximum command buffer size
#define MAX_LORA_BUF (MAGIC_HEADER_SIZE + NRVC2_SECURITY_TAG_SIZE + sizeof(uint64_t) + MAX_CMD_BUF)

// comms transmit work and workqueue
#define TX_WORK_QUEUE_PRIO 5
#define TX_WORK_STACK_SIZE 2048 // CHANGEME: if locked up during tx
K_THREAD_STACK_DEFINE(tx_work_stack_area, TX_WORK_STACK_SIZE);
static struct k_work_q tx_work_q;
static void comms_transmit_work(struct k_work *item);
#define TX_TIMEOUT_MSEC 800
static const k_timeout_t tx_timeout = K_MSEC(TX_TIMEOUT_MSEC);

struct comms_tx_item {
    enum pkt_type type;
    size_t cmd_len;
    struct k_work work;
    uint8_t tx_msg[MAX_CMD_BUF];
};

#define MAX_TX_QUEUE 4
K_MEM_SLAB_DEFINE_STATIC(tx_work_slab, sizeof(struct comms_tx_item), MAX_TX_QUEUE, 4);

// comms receive work and workqueue
#define RX_WORK_QUEUE_PRIO 4
#define RX_WORK_STACK_SIZE 2048 // CHANGEME: if locked up during rx
K_THREAD_STACK_DEFINE(rx_work_stack_area, RX_WORK_STACK_SIZE);
static struct k_work_q rx_work_q;
static void comms_receive_work(struct k_work *item);

struct comms_rx_item {
    size_t len;
    int8_t snr;
    int16_t rssi;
    enum pkt_type type;
    struct k_work work;
    uint8_t rx_buf[MAX_LORA_BUF];
};

#define MAX_RX_QUEUE 8
K_MEM_SLAB_DEFINE_STATIC(rx_work_slab, sizeof(struct comms_rx_item), MAX_RX_QUEUE, 4);

// forward decl
/// FUTURE: When updating zephyr, this return type will need to change to int; 
static void comms_receive(const struct device *dev, uint8_t *data, uint16_t size, int16_t rssi, int8_t snr, void *user_data);

static bool rdy = false;
bool comms_rdy() {
    return rdy;
}

/**
 * The comms modem semaphore. To prevent reconfiguring the modem during active transmit and receive.
 * To be taken when:
 * - idle in RX mode and need to process TX work by reconfiguring to TX mode
 * - done processing TX work queue and need to reconfigure for receive mode
 * - active in RX mode, and need to prevent TX work from reconfiguring while receiving
 */
static K_SEM_DEFINE(modem_sem, 0, 1);

/**
 * ISR friendly iff timeout == K_NO_WAIT
 */
static int set_modem_rx(k_timeout_t timeout) {
    if (!rdy)
        return -ENOINIT;

    int ret = k_sem_take(&modem_sem, timeout);
    if (ret < 0)
        return ret;
    
    if (modem_cfg.tx) {
        modem_cfg.tx = false;
        ret = lora_config(role_devs->dev_lora, &modem_cfg);
        if (ret < 0) {
            k_sem_give(&modem_sem);
            return ret;
        }
    
        ret = lora_recv_async(role_devs->dev_lora, comms_receive, NULL);
    }

    k_sem_give(&modem_sem);

    return ret;
}

/**
 * CALLEE MUST GIVE BACK modem_sem
 * ISR friendly iff timeout == K_NO_WAIT
 */
static int set_modem_tx(k_timeout_t timeout) {
    if (!rdy)
        return -ENOINIT;

    int ret = k_sem_take(&modem_sem, timeout);
    if (ret < 0)
        return ret; 

    if (!modem_cfg.tx) {
        modem_cfg.tx = true;
        lora_recv_async(role_devs->dev_lora, NULL, NULL);
        return lora_config(role_devs->dev_lora, &modem_cfg);
    }
    return 0;
}

int comms_bit_mode(bool bit_running) {
    // BIT needs to configure the modem with its own settings
    // if the comms system is in async RX mode, the LoRa modem is not 
    // configurable
    if (bit_running) {
        int ret = set_modem_tx(K_NO_WAIT);
        if (ret == 0)
            k_sem_give(&modem_sem);
        
        return ret;
    } else
        return set_modem_rx(K_FOREVER);
}

int comms_init() {
    if (rdy)
        return -EALREADY;

    // if the work queue is already initd and started, dont restart it
    if ((tx_work_q.flags & K_WORK_QUEUE_STARTED) == 0) {
        k_work_queue_init(&tx_work_q);
        k_work_queue_start(&tx_work_q, tx_work_stack_area, K_THREAD_STACK_SIZEOF(tx_work_stack_area), TX_WORK_QUEUE_PRIO, NULL);
    }

    if ((rx_work_q.flags & K_WORK_QUEUE_STARTED) == 0) {
        k_work_queue_init(&rx_work_q);
        k_work_queue_start(&rx_work_q, rx_work_stack_area, K_THREAD_STACK_SIZEOF(rx_work_stack_area), RX_WORK_QUEUE_PRIO, NULL);
    }

    // to call set_modem_rx, this needs to start in tx.
    modem_cfg.tx = true;
    k_sem_reset(&modem_sem);
    k_sem_give(&modem_sem);
    rdy = true;

    // setup async reception
    int ret = set_modem_rx(K_NO_WAIT);
    if (ret == -EBUSY) {
        LOG_ERR("%s: Could not put lora modem in RX mode, modem likely busy", __func__);
        return -EBUSY;
    } else if (ret < 0) {
        LOG_ERR("%s: Could not put LoRa modem in RX mode: %d", __func__, ret);
        // do not error out device, something else could have happened
        // maybe depending on the error code this could change in the future
        return ret;
    }

    return 0;
}


static int shell_comms_tx(const struct shell* shell, size_t argc, char** argv) {
    // string starts at argc[1]
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

    return comms_transmit(cmdbuf, cmdlen, COMMAND);
}

int comms_transmit(uint8_t* tx_cmd_buf, size_t tx_cmd_buf_len, enum pkt_type type) {
    if (role_devs->dev_lora_stat != DEVSTAT_RDY)
        return -EDEVNOTRDY;

    if (!rdy) 
        return -ENOINIT;

    struct comms_tx_item* tx_item;
    int ret = k_mem_slab_alloc(&tx_work_slab, (void**)&tx_item, K_NO_WAIT);
    if (ret == -ENOMEM) {
        LOG_ERR("TX Queue full");
        return ret;
    } else if (ret < 0 || (ret == 0 && tx_item == NULL) ) {
        // if number is 0, tx_item is null for some reason
        LOG_ERR("Failed to alloc block for TX work item: %d", ret);
        return ret;
    }

    // ACK and PING are empty
    if (tx_cmd_buf == NULL) {
        *tx_item->tx_msg = '\0';
        tx_cmd_buf_len = 0;
    }
    else
        memcpy(tx_item->tx_msg, tx_cmd_buf, tx_cmd_buf_len);
    tx_item->cmd_len = tx_cmd_buf_len;
    tx_item->type = type;
    k_work_init(&tx_item->work, comms_transmit_work);

    ret = k_work_submit_to_queue(&tx_work_q, &tx_item->work);
    if (ret < 0) {
        LOG_ERR("Failed to submit to tx work queue: %d", ret);
        k_mem_slab_free(&tx_work_slab, tx_item);
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
    int ret = nrvc2_security_encrypt_and_sign(keyopt, tx_item->tx_msg, tx_item->cmd_len, buf + BUF_DATA_OFFSET, tag);
    if (ret < 0) {
        // nrvc2 security will complain for us
        k_mem_slab_free(&tx_work_slab, tx_item);
        return;
    }

    memcpy(buf + BUF_MAGIC_OFFSET, MAGIC_HEADER, MAGIC_HEADER_SIZE - 1);
    buf[BUF_MAGIC_OFFSET + MAGIC_HEADER_SIZE - 1] = tx_item->type;
    memcpy(buf + BUF_TAG_OFFSET, tag, sizeof(tag));
    memcpy(buf + BUF_SEQN_OFFSET, (void*)&seqn_le, sizeof(uint64_t)); 

    ret = set_modem_tx(tx_timeout);
    if (ret == -EAGAIN) {
        LOG_WRN("TX Work: Modem still busy %s after %d ms waiting", modem_cfg.tx ? "TXing" : "RXing", TX_TIMEOUT_MSEC);
        k_mem_slab_free(&tx_work_slab, tx_item);
        return;
    } else if (ret < 0) {
        LOG_ERR("TX Work: Cant set TX mode: %d, disabling comms", ret);
        rdy = false; // disabling, dont release semaphore
        k_mem_slab_free(&tx_work_slab, tx_item);
        return;
    }

    ret = lora_send(role_devs->dev_lora, buf, BUF_DATA_OFFSET + tx_item->cmd_len);
    if (ret < 0) {
        LOG_ERR("TX work LoRa send failed: %d, disabling comms", ret);
        rdy = false; // disabling, dont release semaphore
        k_mem_slab_free(&tx_work_slab, tx_item);
        return;
    }

    k_mem_slab_free(&tx_work_slab, tx_item);

    // go back to receiving if nothing is left in the slab
    // its OK to release the semaphore here and immediately re-take it because work items
    // in the TX queue run in series, and the modem cant call comms_receive in TX mode
    k_sem_give(&modem_sem);
    if (tx_work_slab.info.num_used == 0)
        set_modem_rx(K_FOREVER);
}

static bool rx_frame_valid(const uint8_t* data, uint16_t size) {
    if (size > MAX_LORA_BUF) {
        printk("%s: discarding large pkt: %u\n", __func__, size); // debug only
        return false;
    }

    // command field must be at least 1 byte if not ack or ping
    if (size < MAGIC_HEADER_SIZE) {
        printk("%s: discarding invalid size pkt: %u\n", __func__, size); // debug only
        return false;
    }

    // header must start with NRVC2
    if (memcmp(data + BUF_MAGIC_OFFSET, MAGIC_HEADER, MAGIC_HEADER_SIZE - 1) != 0) {
        printk("%s: discarding invalid header\n", __func__); // debug only
        return false;
    }

    switch ((enum pkt_type)data[BUF_MAGIC_TYPE_OFFSET]) {
        case COMMAND:
        case RESPONSE:
            if (size <= BUF_DATA_OFFSET) { // must be at least 1 byte
                printk("%s: discarding invalid COMMAND or RESPONSE frame\n", __func__);
                return false;
            }
            return true;

        case ACK:
            if (size != BUF_DATA_OFFSET) { // payload size must be 0
                printk("%s: discarding invalid ACK frame\n", __func__);
                return false;
            }
            return true;

        case PING:
            return true; /// TODO: revise later

        default:
            printk("%s: discarding frame with invalid header\n", __func__);
            return false;
    }
}

/// FUTURE: When updating zephyr, this return type will need to change to int; 
static void comms_receive(const struct device *dev, uint8_t *data, uint16_t size, int16_t rssi, int8_t snr, void *user_data) {
    ARG_UNUSED(user_data);
    ARG_UNUSED(dev);

    // this counts as an ISR, if we cant take the semaphore return immediately
    // this should never fail in theory but better safe than sorry
    if (k_sem_take(&modem_sem, K_NO_WAIT) < 0) {
        printk("comms_receive: discarding, modem busy\n"); // debug only
        return;
    }

    if (!rx_frame_valid(data, size)) {
        printk("%s: discarding invalid frame\n", __func__);
        k_sem_give(&modem_sem);
        return;
    }

    // copy to work
    struct comms_rx_item* rx_item;
    int ret = k_mem_slab_alloc(&rx_work_slab, (void**)&rx_item, K_NO_WAIT);
    if (ret == -ENOMEM) {
        printk("RX Queue full\n"); // DO NOT USE LOG ON IMMEDIATE MODE IN THIS CALLBACK YOU WILL PANIC THE KERNEL
        k_sem_give(&modem_sem);
        return;
    } else if (ret < 0 || (ret == 0 && rx_item == NULL) ) {
        printk("Failed to alloc block for TX work item: %d\n", ret);
        k_sem_give(&modem_sem);
        return;
    }

    memcpy(rx_item->rx_buf, data, size);
    rx_item->len = size;
    k_work_init(&rx_item->work, comms_receive_work);
    rx_item->type = (enum pkt_type)data[BUF_MAGIC_OFFSET + MAGIC_HEADER_SIZE - 1];
    rx_item->rssi = rssi;
    rx_item->snr = snr;

    ret = k_work_submit_to_queue(&rx_work_q, &rx_item->work);
    if (ret < 0) 
        printk("Failed to submit to rx work queue: %d\n", ret);
        
    k_sem_give(&modem_sem);
}

static void comms_receive_work(struct k_work *item) {
    // if a lockup occurs here, the work queue stack size is too small :(

    struct comms_rx_item* rx_item = CONTAINER_OF(item, struct comms_rx_item, work);
    
    // special case: ping pong
    // TODO: Wip
    if (rx_item->type == PING) {
        LOG_INF("Ping received!");
        k_mem_slab_free(&rx_work_slab, rx_item);
        return;
    }
    
    // get verification tag and seqn
    uint8_t tag[NRVC2_SECURITY_TAG_SIZE];
    uint64_t seqn_le;
    memcpy(tag, rx_item->rx_buf + BUF_TAG_OFFSET, NRVC2_SECURITY_TAG_SIZE);
    memcpy((uint8_t*)&seqn_le, rx_item->rx_buf + BUF_SEQN_OFFSET, sizeof(uint64_t));
    seqn_le = sys_le64_to_cpu(seqn_le);
    
    // decrypt, verify, and build frame
    const keyopt_t keyopt = ROLE_IS_FOB ? NRVC2_KEYOPT_SESS_COMMS_TRC2FOB : NRVC2_KEYOPT_SESS_COMMS_FOB2TRC;
    uint8_t buf[MAX_LORA_BUF];
    size_t buf_len = rx_item->len - BUF_DATA_OFFSET;
    int ret = nrvc2_security_decrypt_and_verify(keyopt, rx_item->rx_buf + BUF_DATA_OFFSET, buf_len, seqn_le, tag, buf);
    if (ret < 0) {
        LOG_ERR("%s decrypt failed: %d", __func__, ret);
        k_mem_slab_free(&rx_work_slab, rx_item);
        return;
    }

    LOG_INF("PKT RECV. SEQN: %llu, RSSI: %d, SNR: %d", seqn_le, rx_item->rssi, rx_item->snr);

    /// TODO: use a custom remote shell in the future
    /// https://docs.zephyrproject.org/latest/samples/subsys/shell/shell_module/README.html#shell-module
    switch (rx_item->type) {
        case COMMAND: {
            // buffer is a command to be executed
            buf[buf_len] = '\0';
            LOG_INF("Executing \"%s\"", buf);
            const struct shell* sh = shell_backend_uart_get_ptr();
            int ret = shell_execute_cmd(sh, buf);
            if (ret < 0) {
                LOG_ERR("executing remote command failed: %d", ret);
                break;
            }

            // send ack, cant get response without implementing remote shell
            ret = comms_transmit(NULL, 0, ACK);
            if (ret < 0) {
                LOG_ERR("Failed to reply with ACK: %d", ret);
                break;
            }

        } break;
        case RESPONSE: {
            // print non-empty response
            LOG_INF("Response \"%s\"", buf);
        } break;
        case ACK: {
            // not implemented
            LOG_INF("ACK recvd");
        } break;
        default:
        break;
    }

    k_mem_slab_free(&rx_work_slab, rx_item);
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