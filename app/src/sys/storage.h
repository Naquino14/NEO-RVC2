#ifndef STORAGE_H
#define STORAGE_H

#include <errno.h>

/// @brief Neo RVC2 SDHC Storage Mount Point, use to concat with file paths
#define NRVC2_STORAGE_MP "/SD:"

/**
 * @brief If the filesystem at `NRVC2_STORAGE_MP` is not mounted and the SDHC
 *      device is ready, it gets mounted.
 * @returns 0 on success, `errno < 0` on failure. 
 * @retval `-ESTORAGEMOUNTED` if storage is already mounted.
 * @retval `-EDEVNOTRDY` if the SDHC device is not ready. 
 * @retval `errno < 0` for other fs errors. 
 */
int nrvc2_fs_mount();

/**
 * @brief If the filesystem at `NRVC2_STORAGE_MP` is mounted and the SDHC device
 *      is ready, it gets unmounted. 
 * @returns 0 on success, `errno < 0` on failure. 
 * @retval `-ESTORAGENOTMOUNTED` if filesystem is not mounted.
 * @retval `-EDEVNOTRDY` if the SDHC device is not ready. 
 * @retval `errno < 0` for other fs errors. 
 */
int nrvc2_fs_unmount();

/**
 * @brief Check if the filesystem at `NRVC2_STORAGE_MP` is mounted.\
 * @returns true if `NRVC2_STORAGE_MP` is mounted, false otherwise.  
 */
bool nrvc2_storage_is_mounted();

#endif // STORAGE_H