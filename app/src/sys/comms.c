#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <nrvc2_security.h>

#include "../roles.h"

#include "comms.h"

LOG_MODULE_REGISTER(comms);

static bool rdy = false;
bool comms_rdy() {
    return rdy;
}

int comms_init() {
    rdy = true;
    return 0;
}


static int shell_comms_tx(const struct shell* shell, size_t argc, char** argv) {
    // string starts at idx 2
    // TEMPORARY: before fully fleshing out this system
    // running commands requires sending the full command up to a point
    static size_t MAX_CMD = 255;
    uint8_t cmdbuf[MAX_CMD + 1];
    size_t cmdlen = 0;

    for (int i = 0; i < argc - 2; i++) {
        // strings in argv will always be null terminated, strlen is ok here
        size_t len = strlen(argv[i + 2]);
        if (cmdlen > MAX_CMD) {
            LOG_WRN("tx shell command: too long!");
            return -EINVAL;
        }
        
        memcpy(cmdbuf, argv[i + 2], len);
        cmdlen += len;
    }

    cmdbuf[cmdlen] = '\0';

    LOG_INF("shell cmd: %s", cmdbuf);

    return comms_transmit(cmdbuf, cmdlen);
}

int comms_transmit(uint8_t* txbuf, size_t txbuf_len) {
    return 0;
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