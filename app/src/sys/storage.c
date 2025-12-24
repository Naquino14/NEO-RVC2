#include <zephyr/fs/fs.h>
#include <ff.h>
#include <zephyr/logging/log.h>

#include "../roles.h"
#include "../nrvc2_errno.h"

#include "storage.h"

LOG_MODULE_REGISTER(storage, LOG_LEVEL_ERR);

static FATFS sd_fs_fat_info;
static struct fs_mount_t sd_mnt_info = {
    .type = FS_FATFS,
    .fs_data = &sd_fs_fat_info,
    .mnt_point = NRVC2_STORAGE_MP
};

static bool is_mounted = false;

int nrvc2_fs_mount() {
    if (is_mounted) {
        LOG_WRN(NRVC2_STORAGE_MP " already mounted");
        return -ESTORAGEMOUNTED;
    }

    if (role_devs->dev_sdcard_stat != DEVSTAT_RDY) 
        return -EDEVNOTRDY;

    int ret = fs_mount(&sd_mnt_info);
    if (ret < 0) {
        // default behavior that may change later: any mount error disables SD card until reboot
        LOG_ERR("Filesystem failed to mount (%d)", ret);
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return ret;
    }

    is_mounted = true;
    
    return 0;
}

int nrvc2_fs_unmount() {
    if (!is_mounted) {
        LOG_WRN(NRVC2_STORAGE_MP " not mounted");
        return -ESTORAGENOTMOUNTED;
    }

    if (role_devs->dev_sdcard_stat != DEVSTAT_RDY) 
        return -EDEVNOTRDY;

    int ret = fs_unmount(&sd_mnt_info);
    if (ret < 0) {
        // default behavior that may change later: any unmount error disables SD card until reboot
        LOG_ERR("Filesystem failed to unmount (%d)", ret);
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return ret;
    }

    is_mounted = false;

    return 0;
}

bool nrvc2_storage_is_mounted() {
    return is_mounted;
}
