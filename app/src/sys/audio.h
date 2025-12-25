#ifndef AUDIO_H
#define AUDIO_H

#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>

/**
 * @brief Plays the WAV file in the storage device at path `filename`. 
 * This function call returns when the audio transmission is complete.
 * If the I2S device is busy, the thread blocks up until `busy_timeout`. 
 * @param filename the full path to the WAV file to play
 * @param busy_timeout the maximum timeout to wait for the I2S device to be available. 
 * @returns 0 on success, `errno < 0` on failure. 
 * @retval -EAGAIN when timeout timer expires.
 * @retval -EBUSY if `K_NO_WAIT` was specified, and a stream is in progress.
 * @retval `errno < 0` for other I2S/MMIO/Zephyr errors. 
 */
int audio_play_file_blocking(const char* filename, k_timeout_t busy_timeout);

/**
 * @brief Plays the WAV file in the storage device at path `filename`. 
 * This function call returns when the audio transmission is complete.
 * This function will block until the I2S device is ready. 
 * @param filename the full path to the WAV file to play
 * @returns 0 on success, `errno < 0` on failure. 
 * @retval `errno < 0` for other I2S/MMIO/Zephyr errors. 
 */
inline int audio_play_file_blocking_forever(const char* filename) {
    return audio_play_file_blocking(filename, K_FOREVER);
}

/**
 * @brief Signal to the I2S audio device to halt transmission
 * @returns 0 on success, `errno < 0` on failure. 
 * @retval `errno < 0` for errors.
 */
int audio_halt();

#endif