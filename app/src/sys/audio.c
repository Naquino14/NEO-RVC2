#include "audio.h"

#include <zephyr/drivers/i2s.h>
#include <zephyr/logging/log.h>

#include "../roles.h"
#include "../nrvc2_errno.h"
#include "storage.h"

LOG_MODULE_REGISTER(audio, LOG_LEVEL_ERR);

typedef struct {
    struct fs_file_t wav_file;
    off_t filesize;
    size_t chunk_size;
    size_t subchunk1_size;
    int16_t audio_format;
    int16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    size_t subchunk2_size;
} wav_file_t;

static int open_parse_wav(const char* path, wav_file_t* out_wav) {
    if (!out_wav)
        return -EINVAL;
    
    fs_file_t_init(&out_wav->wav_file);

    // open file
    int ret = fs_open(&out_wav->wav_file, path, FS_O_READ);
    if (ret < 0)
        return ret;

    // get total filesize
    ret = fs_seek(&out_wav->wav_file, 0, FS_SEEK_END);
    if (ret < 0)
        return ret; 
    out_wav->filesize = fs_tell(&out_wav->wav_file);
    ret = fs_seek(&out_wav->wav_file, 0, FS_SEEK_SET);
    if (ret < 0)
        return ret;

    // read all 44 header bytes at once
    const size_t header_min_size = 44;
    if (out_wav->filesize < header_min_size) {
        fs_close(&out_wav->wav_file);
        return -EINVAL;
    }

    uint8_t header_buf[header_min_size];
    ssize_t read_ret = fs_read(&out_wav->wav_file, header_buf, sizeof(header_buf));
    if (read_ret < 0) {
        fs_close(&out_wav->wav_file);
        return read_ret;
    }
    
    const uint8_t* riff_off = header_buf;
    const uint8_t* chunk_size_off = header_buf + 0x04;
    const uint8_t* wave_off = header_buf + 0x08;
    const uint8_t* fmt_off = header_buf + 0x0C;
    const uint8_t* data_off = header_buf + 0x24;
    const int pcm_subchunk1_size = 16;
    const size_t subchunk1size_idx = 0x10,
                audio_format_idx = 0x14,
                num_channels_idx = 0x16,
                sample_rate_idx = 0x18,
                byte_rate_idx = 0x1C,
                block_align_idx = 0x20,
                bits_per_sample_idx = 0x22,
                subchunk2_size_idx = 0x28;

    if ((strncmp(riff_off, "RIFF", 4) != 0) 
            || (strncmp(wave_off, "WAVE", 4) != 0)
            || (strncmp(fmt_off, "fmt ", 4) != 0))
        return -EFTYPE;

    out_wav->chunk_size = chunk_size_off[0]
                        | (chunk_size_off[1] << 8)
                        | (chunk_size_off[2] << 16)
                        | (chunk_size_off[3] << 24);
    
    out_wav->subchunk1_size = header_buf[subchunk1size_idx]
                        | (header_buf[subchunk1size_idx + 1] << 8)
                        | (header_buf[subchunk1size_idx + 2] << 16)
                        | (header_buf[subchunk1size_idx + 3] << 24);
    
    if (out_wav->subchunk1_size != pcm_subchunk1_size) {
        LOG_ERR("WAV subchunk1 size not uncompressed PCM standard (unsupported WAV format) for file %s", path);
        return -ENOTSUP;    // other format support not planned atm
    }

    out_wav->audio_format = header_buf[audio_format_idx] | (header_buf[audio_format_idx + 1] << 8);
    if (out_wav->audio_format != 1) {
        LOG_ERR("WAV audio format not uncompressed PCM (unsupported WAV format) for file %s", path);
        return -ENOTSUP;    // other format support not planned atm
    }
    
    out_wav->num_channels = header_buf[num_channels_idx] | (header_buf[num_channels_idx + 1] << 8);

    out_wav->sample_rate = header_buf[sample_rate_idx] 
                            | (header_buf[sample_rate_idx + 1] << 8)
                            | (header_buf[sample_rate_idx + 2] << 16)
                            | (header_buf[sample_rate_idx + 3] << 24);
    
    out_wav->byte_rate = header_buf[byte_rate_idx]
                        | (header_buf[byte_rate_idx + 1] << 8)
                        | (header_buf[byte_rate_idx + 2] << 16)
                        | (header_buf[byte_rate_idx + 3] << 24);
    
    out_wav->block_align = header_buf[block_align_idx] | (header_buf[block_align_idx + 1] << 8);

    out_wav->bits_per_sample = header_buf[bits_per_sample_idx] | (header_buf[bits_per_sample_idx + 1] << 8);

    if (strncmp(data_off, "data", 4) != 0) {
        LOG_ERR("WAV data subchunk not found in file %s", path);
        return -EFTYPE;
    }

    out_wav->subchunk2_size = header_buf[subchunk2_size_idx] | (header_buf[subchunk2_size_idx + 1] << 8) 
                            | (header_buf[subchunk2_size_idx + 2] << 16) | (header_buf[subchunk2_size_idx + 3] << 24);
                            
    return 0;
}

#define I2S_TX_BLOCKS 8
#define I2S_TX_BLOCKSIZE 64
#define TX_QUEUE_FULL_TIMEOUT_MS 500
K_MEM_SLAB_DEFINE_STATIC(i2s_tx_slab, I2S_TX_BLOCKSIZE, I2S_TX_BLOCKS, 2 * sizeof(uint16_t));
static struct i2s_config i2s_cfg = {
    .format = I2S_FMT_DATA_FORMAT_I2S,
    .options = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER,
    .block_size = I2S_TX_BLOCKSIZE,
    .timeout = TX_QUEUE_FULL_TIMEOUT_MS,
    .mem_slab = &i2s_tx_slab
};

/// Semaphore to limit how many audio files that can wait at once.
K_SEM_DEFINE(i2s_dev_sem, 1, 1);

int audio_play_file_blocking(const char* filename, k_timeout_t busy_timeout) {
    // Playing audio requires ready SD card and I2S amp
    if ((role_devs->dev_i2s_stat != DEVSTAT_RDY) || (role_devs->dev_sdcard_stat != DEVSTAT_RDY)) 
        return -EDEVNOTRDY;

    // Playing audio requires a mounted filesystem
    if (!nrvc2_storage_is_mounted())
        return -ESTORAGENOTMOUNTED;

    wav_file_t playing_wav;
    int ret = open_parse_wav(filename, &playing_wav);
    if (ret < 0)
        return ret; // dont consider opening/parsing errors to disable I2S system
    
    // begin I2S PCM stream critical zone
    ret = k_sem_take(&i2s_dev_sem, busy_timeout);
    if (ret < 0) // I2S is busy and may have timed out
        return ret;

    // set params
    i2s_cfg.word_size = playing_wav.bits_per_sample;
    i2s_cfg.channels = playing_wav.num_channels;
    i2s_cfg.frame_clk_freq = playing_wav.sample_rate;

    ret = i2s_configure(role_devs->dev_i2s, I2S_DIR_TX, &i2s_cfg);
    if (ret < 0) {
        LOG_ERR("I2S BIT SD i2s configure failed: %d", ret);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        fs_close(&playing_wav.wav_file);
        k_sem_give(&i2s_dev_sem); 
        return ret;
    }

    // #warning FIXME This may fall apart when WAV is not signed 16 bit PCM
    // solution: I want to move on from this, so dont support anything thats not 2 bytes
    uint16_t audio_buf[I2S_TX_BLOCKSIZE / sizeof(uint16_t)]; 
    void* tx_block[I2S_NUM_BLOCKS];
    int blockno = 0;

    // check if data is 16 bit unsigned PCM
    if (playing_wav.bits_per_sample != 16) {
        LOG_ERR("I2S BIT SD unsupported bits per sample %d", playing_wav.bits_per_sample);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        fs_close(&playing_wav.wav_file);
        k_sem_give(&i2s_dev_sem); 
        return -ENOTSUP;
    } 

    // prime i2s fifo with first block
    ret = fs_read(&playing_wav.wav_file, audio_buf, sizeof(audio_buf));
    if (ret < 0) {
        LOG_ERR("I2S read file failed :( %d", ret);
        // error doesnt need to disable I2S or SD storage
        fs_close(&playing_wav.wav_file);
        nrvc2_fs_unmount();
        k_sem_give(&i2s_dev_sem); 
        return ret;
    }

    /// FIXME theres a better way to do this
    // check if ret is less than buffer size, zero out rest, play, and exit
    if (ret < sizeof(audio_buf)) {
        LOG_WRN("I2S BIT abnormally SD small wav detected");
        size_t to_zero = sizeof(audio_buf) - ret;
        memset(((uint8_t*)audio_buf) + ret, 0, to_zero);
        
        ret = k_mem_slab_alloc(&i2s_tx_slab, (void**)&tx_block[blockno], K_MSEC(TX_QUEUE_FULL_TIMEOUT_MS));
        if (ret < 0) {
            // error doesnt necessarily need to disable I2S
            LOG_ERR("I2S TX mem slab alloc for small buf failed: %d", ret);
            fs_close(&playing_wav.wav_file);
            k_sem_give(&i2s_dev_sem); 
            return ret;
        }

        memcpy(tx_block[blockno], audio_buf, sizeof(audio_buf));
        ret = i2s_write(role_devs->dev_i2s, tx_block[blockno], i2s_cfg.block_size);
        if (ret < 0) {
            LOG_ERR("I2S write small buf to dev failed: %d", ret);
            role_devs->dev_i2s_stat = DEVSTAT_ERR;
            k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]); // best effort slab free
            fs_close(&playing_wav.wav_file);
            k_sem_give(&i2s_dev_sem); 
            return ret;
        }

        // signal I2S device to start playing
        ret = i2s_trigger(role_devs->dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START);
        if (ret < 0) {
            LOG_ERR("I2S trigger start for small buf failed: %d", ret);
            role_devs->dev_i2s_stat = DEVSTAT_ERR;
            k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]);
            fs_close(&playing_wav.wav_file);
            nrvc2_fs_unmount();
            k_sem_give(&i2s_dev_sem); 
            return ret;
        }

        // signal I2S device to drain queue
        k_msleep(1);
        ret = i2s_trigger(role_devs->dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
        if (ret < 0) {
            LOG_ERR("I2S trigger drain for small buf failed: %d", ret);
            // error doesnt necessarily need to disable I2S
            k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]); // best effort slab free
            fs_close(&playing_wav.wav_file);
            k_sem_give(&i2s_dev_sem); 
            return ret;
        }

        k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]);
        ret = fs_close(&playing_wav.wav_file);
        if (ret < 0) {
            LOG_ERR("I2S BIT SD close read test file failed");
            role_devs->dev_sdcard_stat = DEVSTAT_ERR;
            k_sem_give(&i2s_dev_sem); 
            return ret;
        }

        // done playing small buffer
        return 0;
    }

    // block is full, copy to slab and write, then increment blockno
    ret = k_mem_slab_alloc(&i2s_tx_slab, (void**)&tx_block[blockno], K_MSEC(1000));
    if (ret < 0) {
        LOG_ERR("I2S BIT SD mem slab alloc failed: %d", ret);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        fs_close(&playing_wav.wav_file);
        k_sem_give(&i2s_dev_sem); 
        return ret;
    }

    memcpy(tx_block[blockno], audio_buf, sizeof(audio_buf));
    ret = i2s_write(role_devs->dev_i2s, tx_block[blockno], i2s_cfg.block_size);
    if (ret < 0) {
        LOG_ERR("I2S write to dev failed: %d", ret);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]);
        fs_close(&playing_wav.wav_file);
        k_sem_give(&i2s_dev_sem); 
        return ret;
    }

    blockno = (blockno + 1) % I2S_NUM_BLOCKS;
    // first block is primed, trigger i2s and start reading/writing rest of data;
    ret = i2s_trigger(role_devs->dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START);
    if (ret < 0) {
        LOG_ERR("I2S trigger start failed: %d", ret);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        fs_close(&playing_wav.wav_file);
        k_sem_give(&i2s_dev_sem); 
        return ret;
    }

    // loop until we reach the end of file
    // read block, write block, increment blockno
    // if block at blockno is in use by i2s, read data and wait for it to be free
    // WARNING: this system zeroes the end of blocks if they are underfull
    // might be able to pass true size of final block to i2s_write
    for (;;) { 
        // check if end of file reached
        off_t file_pos = fs_tell(&playing_wav.wav_file);
        if (file_pos >= playing_wav.filesize)
            break;
        
        // check difference betweeen file pos and filesize
        off_t diff = playing_wav.filesize - file_pos;

        // if difference is less than buffer size, read only remaining data
        size_t to_read = diff > sizeof(audio_buf) ? sizeof(audio_buf) : diff;
        
        ret = fs_read(&playing_wav.wav_file, audio_buf, to_read);
        if (ret < 0) {
            LOG_ERR("I2S BIT SD mastercautiom.wav read failed :( %d", ret);
            role_devs->dev_sdcard_stat = DEVSTAT_ERR;
            fs_close(&playing_wav.wav_file);
            nrvc2_fs_unmount();
            k_sem_give(&i2s_dev_sem); 
            return ret;
        }

        // if block is not full, zero out the rest
        if (to_read < sizeof(audio_buf))
            memset(((uint8_t*)audio_buf) + to_read, 0, sizeof(audio_buf) - to_read);

        // try to alloc block at blockno
        ret = k_mem_slab_alloc(&i2s_tx_slab, (void**)&tx_block[blockno], K_MSEC(1000));
        if (ret < 0) {
            LOG_ERR("I2S BIT SD mem slab alloc failed (may have timed out): %d", ret);
            role_devs->dev_i2s_stat = DEVSTAT_ERR;
            fs_close(&playing_wav.wav_file);
            k_sem_give(&i2s_dev_sem);
            return ret;
        }

        memcpy(tx_block[blockno], audio_buf, sizeof(audio_buf));
        ret = i2s_write(role_devs->dev_i2s, tx_block[blockno], i2s_cfg.block_size);
        if (ret < 0) {
            LOG_ERR("I2S write to dev failed: %d", ret);
            role_devs->dev_i2s_stat = DEVSTAT_ERR;
            k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]);
            fs_close(&playing_wav.wav_file);
            k_sem_give(&i2s_dev_sem);
            return ret;
        }

        // increment write blockno
        blockno = (blockno + 1) % I2S_NUM_BLOCKS;
    }

    // all data is read, trigger i2s fifo drain
    ret = i2s_trigger(role_devs->dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
    if (ret < 0) {
        LOG_ERR("I2S trigger drain failed: %d", ret);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        fs_close(&playing_wav.wav_file);
        k_sem_give(&i2s_dev_sem);
        return ret;
    }

    k_msleep(1);

    ret = fs_close(&playing_wav.wav_file);
    if (ret < 0) {
        k_sem_give(&i2s_dev_sem);
        return ret;
    }
    
    k_sem_give(&i2s_dev_sem);

    return 0;
}


int audio_halt() {
    return 0; // NYI
}