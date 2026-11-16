#include "ledtach.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrvc2_errno.h>

#include "../roles.h"

LOG_MODULE_REGISTER(ledtach);

static struct led_rgb pixels[DT_PROP(DT_ALIAS(ledtach), chain_length)];

static bool rdy = false;
bool ledtach_rdy() {
    return rdy;
}


int ledtach_init() {
    memset(&pixels, 0x0, sizeof(pixels));
    int ret = led_strip_update_rgb(role_devs->dev_ledtach, pixels, role_devs->dev_ledtach_chainlen);
    if (ret < 0) {
        LOG_ERR("Error zeroing LEDTACH: %d", ret);
        role_devs->dev_ledtach_stat = DEVSTAT_ERR;
        return false;
    }

    rdy = true;
    return true;
}


int ledtach_pix_set(size_t idx, struct led_rgb* newcolor) {
    if (!rdy)
        return -ENOINIT;

    if (role_devs->dev_ledtach_stat != DEVSTAT_RDY)
        return -EDEVNOTRDY;

    size_t chainlen = role_devs->dev_ledtach_chainlen;
    if (idx > chainlen - 1 || newcolor == NULL)
        return -EINVAL;

    memcpy(&pixels[idx], newcolor, sizeof(struct led_rgb));

    return 0;
}

int ledtach_flush() {
    if (!rdy)
        return -ENOINIT;
    
    if (role_devs->dev_ledtach_stat != DEVSTAT_RDY)
        return -EDEVNOTRDY;

    
    int ret = led_strip_update_rgb(role_devs->dev_ledtach, pixels, role_devs->dev_ledtach_chainlen);
    if (ret < 0)
        return ret;

    return 0;
}