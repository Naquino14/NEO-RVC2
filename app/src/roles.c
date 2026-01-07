#include "roles.h"

#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>

const char *FOB_STR = "FOB-COMMANDER-XMTR";
const char *TRC_STR = "TRACK-CONTROL-XPDR";

#if CONFIG_EN_GPIO_LED0
#define LED0_NODE DT_ALIAS(led0)
const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#endif

#if CONFIG_EN_GPIO_SW0
#define SW0_NODE DT_ALIAS(sw0)
const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
#endif

#if CONFIG_EN_DEV_LORA
#define LORA_NODE DT_NODELABEL(lora0)
const struct device * const lora = DEVICE_DT_GET(LORA_NODE);
#endif

#if CONFIG_EN_DEV_CAN0
#define CAN_NODE DT_CHOSEN(zephyr_canbus) /// TODO change chosen if second CAN is added
const struct device * const can0 = DEVICE_DT_GET(CAN_NODE);
#endif

#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_FOB)

#if CONFIG_EN_DEV_DISPLAY
#define DISPLAY_NODE DT_NODELABEL(ssd1306)
const struct device * const display = DEVICE_DT_GET(DISPLAY_NODE);
#endif

static role_devs_t m_role_devs_fob = {
#if CONFIG_EN_GPIO_LED0
    .gpio_led0 = &led0,
    .gpio_led0_stat = DEVSTAT_NOT_RDY,
#else
    .gpio_led0 = NULL,
    .gpio_led0_stat = DEVSTAT_NOTINSTALLED,
#endif

#if CONFIG_EN_GPIO_SW0
    .gpio_sw0 = &sw0,
    .gpio_sw0_stat = DEVSTAT_NOT_RDY,
#else
    .gpio_sw0 = &sw0,
    .gpio_sw0_stat = DEVSTAT_NOTINSTALLED,
#endif

#if CONFIG_EN_DEV_LORA
    .dev_lora = lora,
    .dev_lora_stat = DEVSTAT_NOT_RDY,
#else
    .dev_lora = NULL,
    .dev_lora_stat = DEVSTAT_NOTINSTALLED,
#endif

#if CONFIG_EN_DEV_DISPLAY
    .dev_display = display,
    .gpio_blight = NULL,
    .dev_display_stat = DEVSTAT_NOT_RDY,
    .gpio_blight_stat = DEVSTAT_NOTINSTALLED,
#else
    .dev_display = NULL,
    .gpio_blight = NULL,
    .dev_display_stat = DEVSTAT_NOTINSTALLED,
    .gpio_blight_stat = DEVSTAT_NOTINSTALLED,
#endif

#if CONFIG_EN_DEV_CAN0
    .dev_can0 = can0,
    .dev_can0_stat = DEVSTAT_NOT_RDY,
#else
    .dev_can0 = NULL,
    .dev_can0_stat  = DEVSTAT_NOTINSTALLED,
#endif

    .dev_i2s = NULL,
    .dev_i2s_stat = DEVSTAT_NOTINSTALLED,

#if CONFIG_EN_DEV_SDHC
    .dev_sdcard_stat = DEVSTAT_NOT_RDY,
#else
    .dev_sdcard_stat = DEVSTAT_NOTINSTALLED
#endif
};
role_devs_t* role_devs = &m_role_devs_fob;

#elif defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC)

#if CONFIG_EN_DEV_DISPLAY
#define DISPLAY_NODE DT_NODELABEL(st7735)
const struct device * const display = DEVICE_DT_GET(DISPLAY_NODE);
#define BLIGHT_NODE DT_ALIAS(blight)
const struct gpio_dt_spec blight = GPIO_DT_SPEC_GET(BLIGHT_NODE, gpios);
#endif

#if CONFIG_EN_DEV_I2S
#define I2S_NODE DT_ALIAS(i2s_tx)
const struct device * const i2s = DEVICE_DT_GET(I2S_NODE);
#endif

static role_devs_t m_role_devs_trc = {
#if CONFIG_EN_GPIO_LED0
    .gpio_led0 = &led0,
    .gpio_led0_stat = DEVSTAT_NOT_RDY,
#else
    .gpio_led0 = NULL,
    .gpio_led0_stat = DEVSTAT_NOTINSTALLED,
#endif

#if CONFIG_EN_GPIO_SW0
    .gpio_sw0 = &sw0,
    .gpio_sw0_stat = DEVSTAT_NOT_RDY,
#else
    .gpio_sw0 = &sw0,
    .gpio_sw0_stat = DEVSTAT_NOTINSTALLED,
#endif

#if CONFIG_EN_DEV_LORA
    .dev_lora = lora,
    .dev_lora_stat = DEVSTAT_NOT_RDY,
#else
    .dev_lora = NULL,
    .dev_lora_stat = DEVSTAT_NOTINSTALLED,
#endif

#if CONFIG_EN_DEV_DISPLAY
    .dev_display = display,
    .gpio_blight = &blight,
    .dev_display_stat = DEVSTAT_NOT_RDY,
    .gpio_blight_stat = DEVSTAT_NOT_RDY,
#else
    .dev_display = NULL,
    .gpio_blight = NULL,
    .dev_display_stat = DEVSTAT_NOTINSTALLED,
    .gpio_blight_stat = DEVSTAT_NOTINSTALLED,
#endif

#if CONFIG_EN_DEV_CAN0
    .dev_can0 = can0,
    .dev_can0_stat = DEVSTAT_NOT_RDY,
#else
    .dev_can0 = NULL,
    .dev_can0_stat  = DEVSTAT_NOTINSTALLED,
#endif

#if CONFIG_EN_DEV_I2S
    .dev_i2s = i2s,
    .dev_i2s_stat = DEVSTAT_NOT_RDY,
#else
    .dev_i2s = NULL,
    .dev_i2s_stat = DEVSTAT_NOTINSTALLED,
#endif

#if CONFIG_EN_DEV_SDHC
    .dev_sdcard_stat = DEVSTAT_NOT_RDY
#else
    .dev_sdcard_stat = DEVSTAT_NOTINSTALLED
#endif
};
role_devs_t* role_devs = &m_role_devs_trc;

#endif

LOG_MODULE_REGISTER(roles, LOG_LEVEL_DBG);

static bool init_common()
{ 
    const char *HASHES = "################################";
    printk("%s\n", HASHES);
    printk("#           CFG INIT           #\n");
    printk("%s\n", HASHES);

    bool rdy = true;

    // LED0
    if (role_devs->gpio_led0_stat == DEVSTAT_NOTINSTALLED)
        LOG_INF("LED0\t\tNOT INSTALLED");
    else if (!device_is_ready(role_devs->gpio_led0->port)) {
        LOG_ERR("LED device is not ready");
        role_devs->gpio_led0_stat = DEVSTAT_ERR;
        rdy = false;
    } else {
        role_devs->gpio_led0_stat = DEVSTAT_RDY;
        LOG_INF("LED0\t\tRDY");
    }
    
    // SW0
    if (role_devs->gpio_sw0_stat == DEVSTAT_NOTINSTALLED)
        LOG_INF("SW0\t\tNOT INSTALLED");
    if (!device_is_ready(role_devs->gpio_sw0->port)) {
        LOG_ERR("User switch device is not ready");
        role_devs->gpio_sw0_stat = DEVSTAT_ERR;
        rdy = false;
    } else {
        role_devs->gpio_sw0_stat = DEVSTAT_RDY;
        LOG_INF("SW0\t\tRDY");
    }
    
    // LORA
    if (role_devs->dev_lora_stat == DEVSTAT_NOTINSTALLED)
        LOG_INF("LORA\t\tNOT INSTALLED");
    else if (!device_is_ready(role_devs->dev_lora)) {
        LOG_ERR("LoRa device is not ready");
        role_devs->dev_lora_stat = DEVSTAT_ERR;
        rdy = false;
    } else {
        role_devs->dev_lora_stat = DEVSTAT_RDY;
        LOG_INF("LORA\t\tRDY");
    }

    // Display
    if (role_devs->dev_display_stat == DEVSTAT_NOTINSTALLED)
        LOG_INF("DISPLAY\t\tNOT INSTALLED");
    else if (!device_is_ready(role_devs->dev_display)) {
        LOG_ERR("Display device is not ready");
        role_devs->dev_display_stat = DEVSTAT_ERR;
        rdy = false;
    } else {
        role_devs->dev_display_stat = DEVSTAT_RDY;
        LOG_INF("DISPLAY\t\tRDY");
    }

    // CAN0
    if (role_devs->dev_can0_stat == DEVSTAT_NOTINSTALLED)
        LOG_INF("CAN0\t\tNOT INSTALLED");
    else if (!device_is_ready(role_devs->dev_can0)) {
        LOG_ERR("CAN device is not ready");
        role_devs->dev_can0_stat = DEVSTAT_ERR;
        rdy = false;
    } else {
        role_devs->dev_can0_stat = DEVSTAT_RDY;
        LOG_INF("CAN\t\tRDY");
    }

    // SDHC
    if (role_devs->dev_sdcard_stat == DEVSTAT_NOTINSTALLED) 
        LOG_INF("SDHC\t\tNOT INSTALLED");
    else {
        do {
            const char* disk_pdrv = "SD";

            int ret = disk_access_ioctl(disk_pdrv, DISK_IOCTL_CTRL_INIT, NULL);

            if (ret < 0) {
                LOG_ERR("SD card init failed: storage init error %d", ret);
                role_devs->dev_sdcard_stat = DEVSTAT_ERR;
                rdy = false;
                break;
            }

            k_msleep(2);

            ret = disk_access_ioctl(disk_pdrv, DISK_IOCTL_CTRL_DEINIT, NULL);
            if (ret < 0) {
                LOG_ERR("SD card init failed: storage deinit error %d", ret);
                role_devs->dev_sdcard_stat = DEVSTAT_ERR;
                rdy = false;
                break;
            }

            role_devs->dev_sdcard_stat = DEVSTAT_RDY;  
            LOG_INF("SDHC\t\tRDY");
        } while (false);
    }

    return rdy;
}

static bool init_fob() {
    return true;
}

static bool init_trc_i2s() {
    if (role_devs->dev_i2s_stat == DEVSTAT_NOTINSTALLED) {
        LOG_INF("I2S\t\tNOT INSTALLED");
        return true;
    }

    int ret = device_is_ready(role_devs->dev_i2s);
    if (ret < 0) {
        LOG_ERR("I2S device is not ready");
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        return false;
    }

    role_devs->dev_i2s_stat = DEVSTAT_RDY;
    LOG_INF("I2S\t\tRDY");
    return true;
}

static bool init_trc() {
    bool rdy = true;

    if (role_devs->dev_display_stat == DEVSTAT_RDY && !device_is_ready(role_devs->gpio_blight->port)) {
        LOG_ERR("TRC Backlight GPIO Dev not ready.");
        role_devs->dev_display_stat = DEVSTAT_NOT_RDY;
        role_devs->gpio_blight_stat = DEVSTAT_ERR;
        rdy = false;
    } else 
        role_devs->gpio_blight_stat = DEVSTAT_RDY;

    rdy &= init_trc_i2s();

    return rdy;
}

bool role_config()
{
    // config common
    bool success = true;

    success &= init_common();
    if (!success)
        LOG_ERR("Role init common failed.");

    dev_role role = role_get();

    switch (role)
    {
    case ROLE_FOB:
#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_FOB)
        success &= init_fob();
#endif
        break;
    case ROLE_TRC:
#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC)
        success &= init_trc();
#endif
        break;
    case ROLE_UKN:
    default:
        LOG_ERR("Role init failed: Unknown role.");
        return false;
    }

    LOG_INF("Role %s configuration %s.", role_tostring(), success ? "complete" : "incomplete");
    return success;
}
