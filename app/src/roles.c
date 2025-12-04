#include "roles.h"

#include <zephyr/logging/log.h>

const char *FOB_STR = "FOB-COMMANDER-XMTR";
const char *TRC_STR = "TRACK-CONTROL-XPDR";

#define LED0_NODE DT_ALIAS(led0)
const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define SW0_NODE DT_ALIAS(sw0)
const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

#define LORA_NODE DT_NODELABEL(lora0)
const struct device * const lora = DEVICE_DT_GET(LORA_NODE);

#define CAN_NODE DT_CHOSEN(zephyr_canbus)
const struct device * const can0 = DEVICE_DT_GET(CAN_NODE);

#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_FOB)

#define DISPLAY_NODE DT_NODELABEL(ssd1306)
const struct device *display = DEVICE_DT_GET(DISPLAY_NODE);

static const role_devs_t m_role_devs = {
    .gpio_led0 = &led,
    .gpio_led0_stat = DEVSTAT_NOT_RDY,

    .gpio_sw0 = &sw0,
    .gpio_sw0_stat = DEVSTAT_NOT_RDY,

    .dev_lora = &lora,
    .dev_lora_stat = DEVSTAT_NOT_RDY,

    .dev_display = &display,
    .gpio_blight = NULL,
    .dev_display_stat = DEVSTAT_NOT_RDY,
    .gpio_blight_stat = DEVSTAT_NOTINSTALLED,

    .dev_can0 = &can0,
    .dev_can0_stat = DEVSTAT_NOT_RDY,

    .dev_i2s = NULL,
    .dev_i2s_stat = DEVSTAT_NOTINSTALLED,
    // .i2s_cfg = NULL,

    .dev_sdcard_mnt_info = NULL,
    .dev_sdcard_stat = DEVSTAT_NOTINSTALLED
};
const role_devs_t* role_devs = &m_role_devs;

#elif defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC)

#define DISPLAY_NODE DT_NODELABEL(st7735)
const struct device * const display = DEVICE_DT_GET(DISPLAY_NODE);

#define BLIGHT_NODE DT_ALIAS(blight)
const struct gpio_dt_spec blight = GPIO_DT_SPEC_GET(BLIGHT_NODE, gpios);

#define I2S_NODE DT_ALIAS(i2s_tx)
const struct device * const i2s = DEVICE_DT_GET(I2S_NODE);

static FATFS sd_fs_fat_info;
static const struct fs_mount_t sdcard_mnt_info = {
    .type = FS_FATFS,
    .fs_data = &sd_fs_fat_info,
    .mnt_point = "/SD:"
};

// struct i2s_config i2s_cfg = {0};

static role_devs_t m_role_devs = {
    .gpio_led0 = &led,
    .gpio_led0_stat = DEVSTAT_NOT_RDY,

    .gpio_sw0 = &sw0,
    .gpio_sw0_stat = DEVSTAT_NOT_RDY,

    .dev_lora = lora,
    .dev_lora_stat = DEVSTAT_NOT_RDY,

    .dev_display = display,
    .gpio_blight = &blight,
    .dev_display_stat = DEVSTAT_NOT_RDY,
    .gpio_blight_stat = DEVSTAT_NOT_RDY,

    .dev_can0 = can0,
    .dev_can0_stat = DEVSTAT_NOT_RDY,

    .dev_i2s = i2s,
    .dev_i2s_stat = DEVSTAT_NOT_RDY,
    // .i2s_cfg = &i2s_cfg,

    .dev_sdcard_mnt_info = &sdcard_mnt_info,
    .dev_sdcard_stat = DEVSTAT_NOT_RDY
};
role_devs_t* role_devs = &m_role_devs;

#endif

LOG_MODULE_REGISTER(roles, LOG_LEVEL_DBG);

static bool init_common()
{ 
    const char *HASHES = "################################";
    printk("%s\n", HASHES);
    printk("#           CFG INIT           #\n");
    printk("%s\n", HASHES);

    bool rdy = true;

    if (!device_is_ready(role_devs->gpio_led0->port)) {
        LOG_ERR("LED device is not ready");
        role_devs->gpio_led0_stat = DEVSTAT_ERR;
        rdy = false;
    }
    role_devs->gpio_led0_stat = DEVSTAT_RDY;
    LOG_INF("LED\t\tRDY");
    
    if (!device_is_ready(role_devs->gpio_sw0->port)) {
        LOG_ERR("User switch device is not ready");
        role_devs->gpio_sw0_stat = DEVSTAT_ERR;
        rdy = false;
    }
    role_devs->gpio_sw0_stat = DEVSTAT_RDY;
    LOG_INF("User switch\tRDY");
    
    // if (!device_is_ready(lora)) {
    //     LOG_ERR("LoRa device is not ready");
    //     role_devs->dev_lora_stat = DEVSTAT_ERR;
    //     rdy = false;
    // }
    // role_devs->dev_lora_stat = DEVSTAT_RDY;
    // LOG_INF("LoRa\t\tRDY");

    role_devs->dev_lora_stat = DEVSTAT_NOTINSTALLED; // DELETEME temporary lora disable
    LOG_INF("LoRa\t\tNOT INSTALLED");
    
    if (!device_is_ready(role_devs->dev_lora)) {
        LOG_ERR("Display device is not ready");
        rdy = false;
    }
    role_devs->dev_display_stat = DEVSTAT_RDY;
    LOG_INF("Display\t\tRDY");

    // if (!device_is_ready(role_devs->dev_can0)) {
    //     LOG_ERR("CAN device is not ready");
    //     role_devs->dev_can0_stat = DEVSTAT_ERR;
    //     rdy = false;
    // }
    // role_devs->dev_can0_stat = DEVSTAT_RDY;
    // LOG_INF("CAN\t\tRDY");
    role_devs->dev_can0_stat = DEVSTAT_NOTINSTALLED; // DELETEME temporary can disable
    LOG_INF("CAN\t\tNOT INSTALLED");

    return rdy;
}

static bool init_fob() {
    return true;
}

static bool init_trc_sdhc() {
    const char* disk_pdrv = "SD";

    int ret = disk_access_ioctl(disk_pdrv, DISK_IOCTL_CTRL_INIT, NULL);
    if (ret < 0) {
        LOG_ERR("SD card init failed: storage init error %d", ret);
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }

    k_msleep(200);

    ret = disk_access_ioctl(disk_pdrv, DISK_IOCTL_CTRL_DEINIT, NULL);
    if (ret < 0) {
        LOG_ERR("SD card init failed: storage deinit error %d", ret);
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }

    role_devs->dev_sdcard_stat = DEVSTAT_RDY;  
    LOG_INF("SDHC\t\tRDY");

    return true;
}

K_MEM_SLAB_DEFINE(bit_tx_slab, I2S_BLOCK_SIZE, I2S_NUM_BLOCKS, 4);

static bool init_trc_i2s() {
    int ret = device_is_ready(role_devs->dev_i2s);
    if (ret < 0) {
        LOG_ERR("I2S device is not ready");
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        return false;
    }

    // const uint8_t channels = I2S_CHANNELS;
    // const uint8_t word_size = I2S_WORD_SIZE_BYTES; /* bits */
    // const uint32_t sample_rate = I2S_SAMPLE_RATE_HZ;
    // const int NUM_BLOCKS = I2S_NUM_BLOCKS;
    // const size_t BLOCK_SIZE = I2S_BLOCK_SIZE;

    // /* Prepare i2s configuration */
    // role_devs->i2s_cfg->word_size = word_size;
    // role_devs->i2s_cfg->channels = channels;
    // role_devs->i2s_cfg->format = I2S_FMT_DATA_FORMAT_I2S;
    // role_devs->i2s_cfg->options = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER;
    // role_devs->i2s_cfg->frame_clk_freq = sample_rate;
    // role_devs->i2s_cfg->mem_slab = &bit_tx_slab;
    // role_devs->i2s_cfg->block_size = BLOCK_SIZE;
    // role_devs->i2s_cfg->timeout = 250;

    // ret = i2s_configure(role_devs->dev_i2s, I2S_DIR_TX, role_devs->i2s_cfg);
    // if (ret < 0) {
    //     LOG_ERR("I2S configure failed: %d", ret);
    //     role_devs->dev_i2s_stat = DEVSTAT_ERR;
    //     return false;
    // }

    role_devs->dev_i2s_stat = DEVSTAT_RDY;
    LOG_INF("I2S\t\tRDY");
    return true;
}

static bool init_trc() {
    bool rdy = true;

    rdy &= init_trc_sdhc();

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
