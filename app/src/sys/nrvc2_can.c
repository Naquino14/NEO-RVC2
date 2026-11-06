#include "nrvc2_can.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/drivers/can.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/drivers/uart.h>
#include <drivers/ufirebirdii/ufirebirdii.h>
#include "audio.h"
#include "storage.h"

#include "../roles.h"
#include <nrvc2_errno.h>

#define CAN_BITRATE_KBPS 500
#define CAN_SAMPLE_POINT_PERMILLE 875

LOG_MODULE_REGISTER(can);

static K_SEM_DEFINE(en_can_sniffer_sem, 0, 1);

struct en_can_sniffer_item {
    struct k_work work;
    struct shell* shell;
};
static struct en_can_sniffer_item en_can_sniffer_work_item;
static void en_can_sniffer(struct k_work* work); // forward decl

static bool rdy = false;
bool can_rdy() {
    return rdy;
}

const char* can_state_tostr(enum can_state state) {
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

int can_init() {
    k_work_init(&en_can_sniffer_work_item.work, en_can_sniffer);
    k_sem_init(&en_can_sniffer_sem, 0, 1);
    rdy = true;

    return 0;
}

int can_dev_init(const struct device* dev, const char* devname) {
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

    ret = can_set_mode(dev, CAN_MODE_NORMAL);
    if (ret < 0) {
        LOG_ERR("Failed to set %s mode: %d", devname, ret);
        return ret;
    }

    ret = can_start(dev);
    if (ret < 0) {
        LOG_ERR("Failed to start %s device: %d", devname, ret);
        return ret;
    }

    return 0; 
}

static void en_can_sniffer(struct k_work* work) {
    ARG_UNUSED(work);

    k_sem_take(&en_can_sniffer_sem, K_FOREVER);

    printk("Enabling CAN sniffing mode.\n");

    // disable ALL systems and drivers except CAN
    // TODO: Put this in a function in roles.c called role_deinit or something
    if (role_devs->dev_sdcard_stat == DEVSTAT_RDY) {
        nrvc2_fs_unmount();
        role_devs->dev_sdcard_stat = DEVSTAT_NOT_RDY;
    }

    // comms_deinit(); // TODO: NYI
    if (role_devs->dev_ufirebirdii_stat == DEVSTAT_RDY) {
        ufirebirdii_stop(role_devs->dev_ufirebirdii);
        role_devs->dev_ufirebirdii_stat = DEVSTAT_NOT_RDY;
    }

    if (role_devs->dev_i2s_stat == DEVSTAT_RDY) {
        audio_halt();
        role_devs->dev_i2s_stat = DEVSTAT_NOT_RDY;
    }

    if (role_devs->dev_display_stat == DEVSTAT_RDY)
        role_devs->dev_display_stat = DEVSTAT_NOT_RDY;
    
    // disable shell
    int ret = shell_stop(en_can_sniffer_work_item.shell);
    if (ret < 0) {
        printk("Failed to disable shell: %d\n", ret);
        return;
    }
    
    shell_uninit(en_can_sniffer_work_item.shell, NULL);
    
    // disable log backends
    log_backend_deactivate(log_backend_get_by_name("shell_uart_backend"));
    
    // install catch all filter on CAN0

    // test: write hello
    
    const struct device* uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    const char* test = "HUH??? $100???? MAN GET THIS CRAP OUT MY FACE\r\n";
    while (*test)
        uart_poll_out(uart, *test++);

}

static int shell_can_sniffer(const struct shell* shell, size_t argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    printk("Are you sure you want to enable CAN reverse engineering/sniffer mode? (y/n)\n");
    char c[2];
    int ret = shell_readline(shell, c, sizeof(c), K_FOREVER);
    if (ret < 0 || (*c != 'y' && *c != 'Y'))
        return 0;

    // this re-init may be a problem but I dont care, the shell is deactivated and there is no way to re-enable it atm
    en_can_sniffer_work_item.shell = (struct shell*)shell;

    k_work_submit(&en_can_sniffer_work_item.work);

    k_sem_give(&en_can_sniffer_sem);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_nrvc2_can,
// #if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC) && defined(CONFIG_EN_DEV_CAN0)
    SHELL_CMD(sniffer, NULL, "Enable CAN reverse engineering/sniffer mode", shell_can_sniffer),
// #endif
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(nrvc2_can, &sub_nrvc2_can, "NRVC2 CAN system utilities", NULL);