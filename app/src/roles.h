/// Provides devices and role management

#ifndef ROLES_H
#define ROLES_H

#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <ff.h>
#include <zephyr/drivers/i2s.h>

extern const char* FOB_STR;
extern const char* TRC_STR;

#define DEF_ROLE_FOB 1
#define DEF_ROLE_TRC 2
#define DEF_ROLE_UKN 0

#define DEF_ROLE_IS_TRC (defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC))
#define ROLE_IS_FOB (role_get() == ROLE_FOB)
#define ROLE_IS_TRC (role_get() == ROLE_TRC)

#define I2S_SAMPLES_PER_BLOCK 64
#define I2S_CHANNELS 2
#define I2S_WORD_SIZE_BYTES sizeof(int16_t)
#define I2S_SAMPLE_RATE_HZ 44100
#define I2S_NUM_BLOCKS 8
#define I2S_BLOCK_SIZE (I2S_CHANNELS * I2S_SAMPLES_PER_BLOCK * I2S_WORD_SIZE_BYTES)

typedef enum {
   DEVSTAT_NOTINSTALLED = 0,
   DEVSTAT_NOT_RDY,
   DEVSTAT_RDY,
   DEVSTAT_ERR
} devstat_t;

typedef struct  {
   const struct gpio_dt_spec *gpio_led0;
   devstat_t gpio_led0_stat;

   const struct gpio_dt_spec *gpio_sw0;
   devstat_t gpio_sw0_stat;

   const struct device *dev_lora;
   devstat_t dev_lora_stat;

   const struct device *dev_display;
   const struct gpio_dt_spec *gpio_blight;
   devstat_t dev_display_stat;
   devstat_t gpio_blight_stat;

   const struct device *dev_can0;
   devstat_t dev_can0_stat;

   const struct device *dev_i2s;
   devstat_t dev_i2s_stat;
   struct i2s_config *i2s_cfg;

   const struct fs_mount_t *dev_sdcard_mnt_info;
   devstat_t dev_sdcard_stat;
} role_devs_t;

extern role_devs_t* role_devs;

#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_FOB)
#define LORA_MAX_POW_DBM 14

#elif defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC)
#define LORA_MAX_POW_DBM 18 // up to 21±1

#endif

typedef enum {
   ROLE_FOB = DEF_ROLE_FOB,
   ROLE_TRC = DEF_ROLE_TRC,
   ROLE_UKN = DEF_ROLE_UKN
} dev_role;

inline const dev_role role_get() {
#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_FOB)
   return ROLE_FOB;
#elif defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC)
   return ROLE_TRC;
#else
   return ROLE_UKN;
#endif
}

inline const char* role_tostring() {
   dev_role role = role_get();

   switch (role) {
      case ROLE_FOB:
         return FOB_STR;
      case ROLE_TRC:
         return TRC_STR;
      case ROLE_UKN:
      default:
         return "UNKNOWN";
   }
}

/**
 * Automatically get device role and configure GPIOs
 */
bool role_config();

bool device_config();

#endif // ROLES_H