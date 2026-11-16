#include "ledtach.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrvc2_errno.h>

#include "../roles.h"

LOG_MODULE_REGISTER(ledtach);

const size_t chainlen = DT_PROP(DT_ALIAS(ledtach), chain_length);
static struct led_rgb pixels[DT_PROP(DT_ALIAS(ledtach), chain_length)];

static bool rdy = false;
bool ledtach_rdy() {
    return rdy;
}


int ledtach_init() {
    memset(pixels, 0x0, sizeof(pixels));
    int ret = led_strip_update_rgb(role_devs->dev_ledtach, pixels, role_devs->dev_ledtach_chainlen);
    if (ret < 0) {
        LOG_ERR("Error zeroing LEDTACH: %d", ret);
        role_devs->dev_ledtach_stat = DEVSTAT_ERR;
        return ret;
    }

    rdy = true;
    return 0;
}

int ledtach_pix_set(size_t idx, struct led_rgb newcolor) {
    if (!rdy)
        return -ENOINIT;

    if (role_devs->dev_ledtach_stat != DEVSTAT_RDY)
        return -EDEVNOTRDY;

    if (idx > chainlen - 1)
        return -EINVAL;
    
    // map to max brightness
    newcolor.b = SCALE_MAXBRIGHT(newcolor.b);
    newcolor.g = SCALE_MAXBRIGHT(newcolor.g);
    newcolor.r = SCALE_MAXBRIGHT(newcolor.r);

    memcpy(&pixels[idx], &newcolor, sizeof(struct led_rgb));

    return 0;
}

int ledtach_flush() {
    if (!rdy)
        return -ENOINIT;
    
    if (role_devs->dev_ledtach_stat != DEVSTAT_RDY)
        return -EDEVNOTRDY;

    
    int ret = led_strip_update_rgb(role_devs->dev_ledtach, pixels, chainlen);
    if (ret < 0)
        return ret;

    return 0;
}