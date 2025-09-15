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

extern const char* FOB_STR;
extern const char* TRC_STR;

#define DEF_ROLE_FOB 1
#define DEF_ROLE_TRC 2
#define DEF_ROLE_UKN 0

#define DEF_ROLE_IS_TRC (defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC))
#define ROLE_IS_FOB (role_get() == ROLE_FOB)
#define ROLE_IS_TRC (role_get() == ROLE_TRC)

extern const struct gpio_dt_spec led;
extern const struct gpio_dt_spec sw0;

extern const struct device *lora;
extern const struct device *display;
extern const struct gpio_dt_spec blight;

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