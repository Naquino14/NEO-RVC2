#include "built-in-test.h"
#include "roles.h"

#include <zephyr/logging/log.h>
#include <zephyr/linker/linker-defs.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gnss.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/drivers/display.h>
#include <zephyr/fs/fs.h>

#include <lvgl.h>
#include <lvgl_input_device.h>
#include <math.h>

#include "sys/storage.h"

LOG_MODULE_REGISTER(bit, LOG_LEVEL_DBG);

K_SEM_DEFINE(sw0_sem, 0, 1);

static bool sw0_ok = false;
static struct gpio_callback sw0_cb_data;
static void button_pressed(const struct device* dev, struct gpio_callback *cb, uint32_t pins) {
    if (!sw0_ok)
        LOG_INF("User switch\t\tOK");
    k_sem_give(&sw0_sem);
    sw0_ok = true;
}

static struct lora_modem_config lora_cfg = {
    .frequency = MHZ(915),
    .bandwidth = BW_125_KHZ,
    .datarate = SF_10,
    .preamble_len = 8,
    .coding_rate = CR_4_5,
    .iq_inverted = false,
    .public_network = false,
    .tx_power = LORA_MAX_POW_DBM
};

static bool do_pong = false, listening = true;
static void lora_rx_cb(const struct device *dev, uint8_t* data, uint16_t len, int16_t rssi, int8_t snr, void* user_data) {
    LOG_INF("Lora Packet Rx: %.*s, RSSI: %d, SNR: %d", len, data, rssi, snr);
    do_pong = true;
}

bool bit_led() {
    if (role_devs->gpio_led0_stat != DEVSTAT_RDY) {
        LOG_WRN("LED0\t\tSKIP");
        return true;
    }

    int ret = gpio_pin_set_dt(role_devs->gpio_led0, true);
    if (ret == -EIO) {
        LOG_ERR("LED0 port IO err");
        role_devs->gpio_led0_stat = DEVSTAT_ERR;
        return false;
    }
    k_msleep(10);
    gpio_pin_set_dt(role_devs->gpio_led0, false);
    LOG_INF("LED0\t\tOK");
    return true;
}

bool bit_lora(bool call_resp) {
    if (role_devs->dev_lora_stat != DEVSTAT_RDY) {
        LOG_WRN("LoRa\t\tSKIP");
        return true;
    }

    lora_cfg.tx = true;
    char call[] = "PING";
    if (!call_resp)
        lora_cfg.tx_power = 2; // 2dbm
    else
        lora_cfg.tx_power = LORA_MAX_POW_DBM;
    
    int ret = lora_config(role_devs->dev_lora, &lora_cfg);
    if (ret < 0) {
        LOG_ERR("LoRa config failed: %d", ret);
        role_devs->dev_lora_stat = DEVSTAT_ERR;
        return false;
    }

    ret = lora_send(role_devs->dev_lora, call, sizeof(call));
    if (ret < 0) {
        LOG_ERR("LoRa send failed: %d", ret);
        role_devs->dev_lora_stat = DEVSTAT_ERR;
        return false;
    }

    if (call_resp) {
        char resp[] = "PONG";
        char recv[sizeof(resp)];
        int16_t rssi;
        int8_t snr;

        ret = lora_recv(role_devs->dev_lora, recv, sizeof(recv), K_MSEC(1000), &rssi, &snr);
        if (ret < 0) {
            LOG_ERR("LoRa receive failed: %d", ret);
            role_devs->dev_lora_stat = DEVSTAT_ERR;
            return false;
        }

        LOG_INF("LoRa Pong received. RSSI: %d, SNR: %d", rssi, snr);
    }

    LOG_INF("LoRa\t\tOK");
    return true;
}

#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC)
static bool bit_display_st7735(bool wait_sw0)
{
    if (role_devs->dev_display_stat != DEVSTAT_RDY) {
        LOG_WRN("Display\t\tSKIP");
        return true;
    }

    int width = DT_PROP(DT_CHOSEN(zephyr_display), width);
    int height = DT_PROP(DT_CHOSEN(zephyr_display), height);
    
    struct display_capabilities capabilities;
    display_get_capabilities(role_devs->dev_display, &capabilities);

    struct display_buffer_descriptor fbuf_descr = {
        .width = 1,
        .height = 1,
        .pitch = 1
    };

    if (ROLE_IS_TRC) {
        gpio_pin_set_dt(role_devs->gpio_blight, true);
        fbuf_descr.buf_size = sizeof(uint16_t);
    } else 
        fbuf_descr.buf_size = sizeof(uint8_t);

    bool second = false;
    do {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                bool off = (x + y) % 2 == 0;
                off = second ? off : !off;

                // compute pixel color bit
                #if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_FOB)
                const uint8_t pixel = off ? 0x00 : 0xFF; 
                #else
                const uint16_t pixel = off ? 0x0000 : 0xFFFF;
                #endif

                // set pixel
                int ret = display_write(role_devs->dev_display, x, y, &fbuf_descr, &pixel);

                if (ret < 0) {
                    LOG_ERR("Display write failed: %d", ret);

                    if (ROLE_IS_TRC)
                        gpio_pin_set_dt(role_devs->gpio_blight, false);
                    
                    role_devs->dev_display_stat = DEVSTAT_ERR;
                    return false;
                }
            }
        }
        
        if (wait_sw0) 
            k_sem_take(&sw0_sem, K_FOREVER);
        else
            k_msleep(1000);

        // clear display now 
        display_blanking_on(role_devs->dev_display);
        display_blanking_off(role_devs->dev_display);

    } while ((second = !second));

    if (ROLE_IS_TRC)
        gpio_pin_set_dt(role_devs->gpio_blight, false);

    LOG_INF("Display\t\tOK");
    return true;
}

static bool bit_sdhc() {
    if (role_devs->dev_sdcard_stat != DEVSTAT_RDY) {
        LOG_WRN("SDHC\t\tSKIP");
        return true;
    }

    // mount, read, write, check write, unmount
    int ret = nrvc2_fs_mount();
    if (ret < 0)
        return false;

    struct fs_file_t tst_file;
    fs_file_t_init(&tst_file);
    k_msleep(100);
    const char* test_path = "/SD:/bit.txt";
    ret = fs_open(&tst_file, test_path, FS_O_CREATE | FS_O_RDWR);
    if (ret == -ENOENT) 
        LOG_WRN("SD could not open bit.txt for writing");
    else if (ret < 0) {
        LOG_ERR("SD open test failed %d", ret);
        nrvc2_fs_unmount();
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }
    
    const char write_buf[11] = "SDHC\t\tOK?\n";
    ssize_t read_ret = fs_write(&tst_file, write_buf, sizeof(write_buf));
    if (read_ret < 0) {
        LOG_ERR("SD write test file failed");
        nrvc2_fs_unmount();
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }

    ret = fs_close(&tst_file);
    if (ret < 0) {
        LOG_ERR("SD close written bit.txt file failed");
        nrvc2_fs_unmount();
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }

    k_msleep(10);
    fs_file_t_init(&tst_file);
    ret = fs_open(&tst_file, test_path, FS_O_READ);
    if (ret < 0) {
        LOG_ERR("SD open test file for read failed %d", ret);
        nrvc2_fs_unmount();
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }

    char read_buf[sizeof(write_buf)];
    read_ret = fs_read(&tst_file, read_buf, sizeof(read_buf));
    if (read_ret < 0) {
        LOG_ERR("SD read test file failed %d", read_ret);
        nrvc2_fs_unmount(); 
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }

    ret = fs_close(&tst_file);
    if (ret < 0) {
        LOG_ERR("SD close read test file failed");
        nrvc2_fs_unmount();
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }

    ret = strncmp(read_buf, write_buf, sizeof(read_buf));
    if (ret != 0) {
        LOG_ERR("SD read/write data mismatch");
        nrvc2_fs_unmount();
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }

    // delete test file
    ret = fs_unlink(test_path);
    if (ret < 0) { 
        LOG_ERR("SD delete test file failed %d", ret);
        nrvc2_fs_unmount();
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }

    k_msleep(100);
    ret = nrvc2_fs_unmount();
    if (ret < 0) 
        return false;

    LOG_INF("SDHC\t\tOK");
    return true;
}

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
K_MEM_SLAB_DEFINE_STATIC(i2s_tx_slab, I2S_TX_BLOCKSIZE, I2S_TX_BLOCKS, 2 * sizeof(uint16_t));

static bool bit_i2s() {
    if (role_devs->dev_i2s_stat != DEVSTAT_RDY) {
        LOG_WRN("I2S\t\tSKIP");
        return true;
    }

    if (role_devs->dev_sdcard_stat != DEVSTAT_RDY) {
        LOG_WRN("I2S\t\tSKIP");
        return true;
    }

    int ret = nrvc2_fs_mount();
    if (ret < 0) {
        return false;
    }

    wav_file_t master_caution_wav;
    ret = open_parse_wav("/SD:/master-caution.wav", &master_caution_wav);
    if (ret < 0) {
        LOG_ERR("I2S BIT SD open/parse master-caution.wav failed: %d", ret);
        nrvc2_fs_unmount();
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }
    
    // printk("WAV info:\nFilesize: %lu bytes\n", master_caution_wav.filesize);
    // printk("Audio format: %d\n", master_caution_wav.audio_format);
    // printk("Num channels: %d\n", master_caution_wav.num_channels);
    // printk("Sample rate: %u Hz\n", master_caution_wav.sample_rate);
    // printk("Bits per sample: %d\n", master_caution_wav.bits_per_sample);
    // printk("Byte rate: %u\n", master_caution_wav.byte_rate);
    // printk("Block align: %d\n", master_caution_wav.block_align);
    // printk("Subchunk1 size: %u\n", master_caution_wav.subchunk1_size);
    // printk("Subchunk2 size: %u\n", master_caution_wav.subchunk2_size);
    // printk("Wav fpos: %ld\n", fs_tell(&master_caution_wav.wav_file));

    struct i2s_config i2s_cfg = {0};
    i2s_cfg.word_size = master_caution_wav.bits_per_sample;
    i2s_cfg.channels = master_caution_wav.num_channels;
    i2s_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
    i2s_cfg.options = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER;
    i2s_cfg.frame_clk_freq = master_caution_wav.sample_rate;
    i2s_cfg.timeout = 1000;
    i2s_cfg.block_size = I2S_TX_BLOCKSIZE;
    i2s_cfg.mem_slab = &i2s_tx_slab;

    // printk("I2S info:\nword_size: %d\n", i2s_cfg.word_size);
    // printk("channels: %d\n", i2s_cfg.channels);
    // printk("format: %d\n", i2s_cfg.format);
    // printk("options: %d\n", i2s_cfg.options);
    // printk("frame_clk_freq: %u Hz\n", i2s_cfg.frame_clk_freq);
    // printk("block_size: %d bytes\n", i2s_cfg.block_size);
    // printk("mem_slab: %p\n", i2s_cfg.mem_slab);

    ret = i2s_configure(role_devs->dev_i2s, I2S_DIR_TX, &i2s_cfg);
    if (ret < 0) {
        LOG_ERR("I2S BIT SD i2s configure failed: %d", ret);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        fs_close(&master_caution_wav.wav_file);
        nrvc2_fs_unmount();
        return false;
    }

    uint16_t audio_buf[I2S_TX_BLOCKSIZE / sizeof(uint16_t)];
    void* tx_block[I2S_NUM_BLOCKS];
    int blockno = 0;
    // prime i2s fifo with first block
    ret = fs_read(&master_caution_wav.wav_file, audio_buf, sizeof(audio_buf));
    if (ret < 0) {
        LOG_ERR("I2S BIT SD mastercautiom.wav read failed :( %d", ret);
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        fs_close(&master_caution_wav.wav_file);
        nrvc2_fs_unmount();
        return false;
    }

    // check if ret is less than buffer size, zero out rest, play, and exit
    if (ret < sizeof(audio_buf)) {
        LOG_WRN("I2S BIT abnormally SD small wav detected");
        size_t to_zero = sizeof(audio_buf) - ret;
        memset(((uint8_t*)audio_buf) + ret, 0, to_zero);
        
        ret = k_mem_slab_alloc(&i2s_tx_slab, (void**)&tx_block[blockno], K_MSEC(1000));
        if (ret < 0) {
            LOG_ERR("I2S BIT SD mem slab alloc for small buf failed: %d", ret);
            role_devs->dev_i2s_stat = DEVSTAT_ERR;
            fs_close(&master_caution_wav.wav_file);
            nrvc2_fs_unmount();
            return false;
        }
        memcpy(tx_block[blockno], audio_buf, sizeof(audio_buf));
        ret = i2s_write(role_devs->dev_i2s, tx_block[blockno], i2s_cfg.block_size);
        if (ret < 0) {
            LOG_ERR("I2S write small buf to dev failed: %d", ret);
            role_devs->dev_i2s_stat = DEVSTAT_ERR;
            k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]);
            fs_close(&master_caution_wav.wav_file);
            nrvc2_fs_unmount();
            return false;
        }
        ret = i2s_trigger(role_devs->dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START);
        if (ret < 0) {
            LOG_ERR("I2S trigger start for small buf failed: %d", ret);
            role_devs->dev_i2s_stat = DEVSTAT_ERR;
            k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]);
            fs_close(&master_caution_wav.wav_file);
            nrvc2_fs_unmount();
            return false;
        }

        k_msleep(10);

        ret = i2s_trigger(role_devs->dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
        if (ret < 0) {
            LOG_ERR("I2S trigger drain for small buf failed: %d", ret);
            role_devs->dev_i2s_stat = DEVSTAT_ERR;
            k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]);
            fs_close(&master_caution_wav.wav_file);
            nrvc2_fs_unmount();
            return false;
        }

        k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]);
        ret = fs_close(&master_caution_wav.wav_file);
        if (ret < 0) {
            LOG_ERR("I2S BIT SD close read test file failed");
            role_devs->dev_sdcard_stat = DEVSTAT_ERR;
            return false;
        }

        nrvc2_fs_unmount();

        LOG_INF("I2S\t\tOK");
        return true;
    }

    // block is full, copy to slab and write, then increment blockno
    ret = k_mem_slab_alloc(&i2s_tx_slab, (void**)&tx_block[blockno], K_MSEC(1000));
    if (ret < 0) {
        LOG_ERR("I2S BIT SD mem slab alloc failed: %d", ret);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        fs_close(&master_caution_wav.wav_file);
        nrvc2_fs_unmount();
        return false;
    }
    memcpy(tx_block[blockno], audio_buf, sizeof(audio_buf));
    ret = i2s_write(role_devs->dev_i2s, tx_block[blockno], i2s_cfg.block_size);
    if (ret < 0) {
        LOG_ERR("I2S write to dev failed: %d", ret);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]);
        fs_close(&master_caution_wav.wav_file);
        nrvc2_fs_unmount();
        return false;
    }
    blockno = (blockno + 1) % I2S_NUM_BLOCKS;

    // first block is primed, trigger i2s and start reading/writing rest of data;
    ret = i2s_trigger(role_devs->dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START);
    if (ret < 0) {
        LOG_ERR("I2S trigger start failed: %d", ret);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        fs_close(&master_caution_wav.wav_file);
        nrvc2_fs_unmount();
        return false;
    }

    // idea: loop until we reach the end of file
    // read block, write block, increment blockno
    // if block at blockno is in use by i2s, read data and wait for it to be free
    for (;;) {
        // check if end of file reached
        off_t file_pos = fs_tell(&master_caution_wav.wav_file);
        if (file_pos >= master_caution_wav.filesize)
            break;
        
        // check difference betweeen file pos and filesize
        off_t diff = master_caution_wav.filesize - file_pos;

        // if difference is less than buffer size, read only remaining data
        size_t to_read = diff > sizeof(audio_buf) ? sizeof(audio_buf) : diff;
        
        ret = fs_read(&master_caution_wav.wav_file, audio_buf, to_read);
        if (ret < 0) {
            LOG_ERR("I2S BIT SD mastercautiom.wav read failed :( %d", ret);
            role_devs->dev_sdcard_stat = DEVSTAT_ERR;
            fs_close(&master_caution_wav.wav_file);
            nrvc2_fs_unmount();
            return false;
        }

        // if block is not full, zero out the rest
        if (to_read < sizeof(audio_buf)) {
            size_t to_zero = sizeof(audio_buf) - to_read;
            memset(((uint8_t*)audio_buf) + to_read, 0, to_zero);
        }

        // try to alloc block at blockno
        ret = k_mem_slab_alloc(&i2s_tx_slab, (void**)&tx_block[blockno], K_MSEC(1000));
        if (ret < 0) {
            LOG_ERR("I2S BIT SD mem slab alloc failed (may have timed out): %d", ret);
            role_devs->dev_i2s_stat = DEVSTAT_ERR;
            fs_close(&master_caution_wav.wav_file);
            nrvc2_fs_unmount();
            return false;
        }
        memcpy(tx_block[blockno], audio_buf, sizeof(audio_buf));
        ret = i2s_write(role_devs->dev_i2s, tx_block[blockno], i2s_cfg.block_size);
        if (ret < 0) {
            LOG_ERR("I2S write to dev failed: %d", ret);
            role_devs->dev_i2s_stat = DEVSTAT_ERR;
            k_mem_slab_free(&i2s_tx_slab, (void**)&tx_block[blockno]);
            fs_close(&master_caution_wav.wav_file);
            nrvc2_fs_unmount();
            return false;
        }

        // increment write blockno
        blockno = (blockno + 1) % I2S_NUM_BLOCKS;
    }

    // all data is read, trigger i2s fifo drain
    ret = i2s_trigger(role_devs->dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
    if (ret < 0) {
        LOG_ERR("I2S trigger drain failed: %d", ret);
        role_devs->dev_i2s_stat = DEVSTAT_ERR;
        fs_close(&master_caution_wav.wav_file);
        nrvc2_fs_unmount();
        return false;
    }

    k_msleep(10);

    ret = fs_close(&master_caution_wav.wav_file);
    if (ret < 0) {
        LOG_ERR("I2S BIT SD close read test file failed");
        role_devs->dev_sdcard_stat = DEVSTAT_ERR;
        return false;
    }

    nrvc2_fs_unmount();

    LOG_INF("I2S\t\tOK");
    return true;
}

#endif

#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_FOB)
static bool bit_display_ssd1306(bool wait_sw0) {
    if (role_devs->dev_display_stat != DEVSTAT_RDY) {
        LOG_WRN("Display\t\tSKIP");
        return true;
    }

    const int width = DT_PROP(DT_CHOSEN(zephyr_display), width);
    const int height = DT_PROP(DT_CHOSEN(zephyr_display), height);

    const char PAT_A = 0b10101010, PAT_B = 0b1010101;
    
    // each byte is 8 vertical bits on display
    char fbuf[width * height / 8];

    struct display_buffer_descriptor fbuf_descr = {
        .width = width,
        .height = 8,
        .pitch = width, 
        .buf_size = sizeof(fbuf),
        .frame_incomplete = true
    };

    for (int i = 0; i < sizeof(fbuf); i++) 
        fbuf[i] = i % 2 ? PAT_A : PAT_B;

    int cnt = 0; 
    do {
        for (int i = 0; i < sizeof(fbuf); i++) 
            fbuf[i] = cnt != 2 ? ~fbuf[i] : 0x0;

        for (int i = 0; i < height; i += 8) {
            fbuf_descr.frame_incomplete = (height - 8) == i ? false : true;
            int ret = display_write(role_devs->dev_display, 0, i, &fbuf_descr, fbuf);

            if (ret < 0) {
                LOG_ERR("Display write failed: %d", ret);
                role_devs->dev_display_stat = DEVSTAT_ERR;
                return false;
            }
        }

        display_blanking_off(role_devs->dev_display);

        if (cnt >= 2)
            break;
        else if (!wait_sw0)
            k_msleep(1000);
        else 
            k_sem_take(&sw0_sem, K_FOREVER);

        cnt++;
    } while (true);

    LOG_INF("Display\t\tOK");
    return true;
}
#endif

bool bit_display(bool wait_sw0) {
#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_FOB)
    return bit_display_ssd1306(wait_sw0);
#elif defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC)
    return bit_display_st7735(wait_sw0);
#endif
    return false;
}

bool bit_role_specific_basic() {
    bool ok = true;
#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_FOB)
#elif defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_TRC)
    if (!bit_sdhc())
        ok = false;
    
    if (!bit_i2s())
        ok = false;

#endif
    
    return ok;
}

bool bit_basic() {
    // run bit on:
    // led
    // display
    // lora
    // 
    // trc specific:
    // CAN
    // sdcard
    // gps (later)

    bool ok = true;

    const char *HASHES = "################################";

    printk("%s\n", HASHES);
    printk("#      HYUNDAI-REMOTE BIT      #\n");
    printk("# Board: %-21s #\n", CONFIG_BOARD);
    printk("# Role: %-22s #\n", role_tostring());
    printk("%s\n", HASHES);

    if (role_devs->gpio_sw0_stat != DEVSTAT_RDY) 
        LOG_WRN("SW0\t\tSKIP");
    else if (role_devs->gpio_sw0->port) {
        gpio_init_callback(&sw0_cb_data, button_pressed, BIT(role_devs->gpio_sw0->pin));
        int ret = gpio_add_callback(role_devs->gpio_sw0->port, &sw0_cb_data);
        if (ret < 0) {
            LOG_ERR("Failed to register SW0 callback: %d\n", ret);
            role_devs->gpio_sw0_stat = DEVSTAT_ERR;
            ok = false;
        }
    }

    ok &= bit_led();

    ok &= bit_lora(false);
    
    ok &= bit_display(false);

    // device specific BIT
    ok &= bit_role_specific_basic();

    if (!ok) 
        LOG_ERR("BIT errors have occured");
    
    stop_bit();
    LOG_INF("BIT %s complete.", role_tostring());

    return ok;
}

void run_bit() {
    const char *HASHES = "################################";

    printk("%s\n", HASHES);
    printk("#      HYUNDAI-REMOTE BIT      #\n");
    printk("# Board: %-21s #\n", CONFIG_BOARD);
    printk("# Role: %-22s #\n", role_tostring());
    printk("%s\n", HASHES);

    int ret;

    if (role_devs->gpio_sw0->port) {
        static struct gpio_callback sw0_cb_data;
        gpio_init_callback(&sw0_cb_data, button_pressed, BIT(role_devs->gpio_sw0->pin));
        ret = gpio_add_callback(role_devs->gpio_sw0->port, &sw0_cb_data);
        if (ret < 0) 
            printk("Failed to register SW0 callback: %d\n", ret);
    }

    if (role_devs->dev_lora) do {
                if (!device_is_ready(role_devs->dev_lora)) 
                    printk("Lora device is not ready\n");
        
                lora_cfg.tx = (role_get() == ROLE_FOB);

                ret = lora_config(role_devs->dev_lora, &lora_cfg);
                if (ret < 0) {
                    printk("Lora config failed: %d\n", ret);
                    break;
                }
                
                if (role_devs->dev_lora && role_get() == ROLE_TRC) {
                    ret = lora_recv_async(role_devs->dev_lora, lora_rx_cb, NULL);
                    if (ret < 0) {
                        printk("LoRa callback register failed: %d\n", ret);
                        break;
                    }
                    listening = true;
                    printk("Lora OK\n");
                }
            } while (false);

    
    // use graphics library to BIT screen
    if (role_devs->dev_display) do {
                if (!device_is_ready(role_devs->dev_display)) {
                    printk("Display device not ready\n");
                    break;
                }

                display_blanking_off(role_devs->dev_display);

                if (ROLE_IS_TRC) 
                    gpio_pin_set_dt(role_devs->gpio_blight, true);

                printk("Display OK\n");

                // display role on screen
                lv_obj_t *role_label = lv_label_create(lv_screen_active());
                char role_label_str[11]; 
                snprintf(role_label_str, sizeof(role_label_str), "Role: %s", role_get() == ROLE_FOB ? "FOB" : "TRC");
                lv_label_set_text(role_label, role_label_str);
                lv_obj_align(role_label, LV_ALIGN_CENTER, 0, 0);
                lv_timer_handler(); // writes fb to screen?
            } while (false);

    // static bool state = true;

    for (;;) {
        /*int ret = */gpio_pin_toggle_dt(role_devs->gpio_led0);
        // int read_state = gpio_pin_get_dt(&led);
        // printk("Hello World! %s\tState: %s\tRead State: %s\t", CONFIG_BOARD, state ? "ON " : "OFF", read_state == GPIO_OUTPUT_HIGH ? "HI" : "LO");
        // printk("%s%d\n\n", ret == 0 ? "    OK: " : "NOT OK: ", ret);

        // state = !state;
        if (sw0_ok && role_devs->gpio_sw0->port) {
            sw0_ok = false;
            // display sw0 pressed on display here eventually

            if (role_devs->dev_lora && role_get() == ROLE_FOB) {
                // send ping
                char data[] = "PING";
                printk("Pinging TRC...\n");
                ret = lora_send(role_devs->dev_display, data, sizeof(data));
                if (ret < 0) {
                    printk("Lora send failed: %d\n", ret);
                }

                // listen for pong
                int16_t rssi;
                uint8_t snr;
                ret = lora_recv(role_devs->dev_display, data, sizeof(data), K_MSEC(10000), &rssi, &snr);
                if (ret < 0) {
                    printk("Lora recv failed: %d\n", ret);
                } else {
                    printk("PONG received: %s, RSSI: %d, SNR: %d\n", data, rssi, snr);
                    // display pong on display here eventually
                }
            }
        }

        if (role_devs->dev_display && do_pong) do {
                printk("Pinged, ponging...\n");

                // stop rx
                lora_recv_async(role_devs->dev_lora, NULL, NULL);
                listening = false;
                
                lora_cfg.tx = true; 
                int ret = lora_config(role_devs->dev_lora, &lora_cfg);
                if (ret < 0) {
                    printk("Lora rx cb: Set Lora cfg TX failed: %d\n", ret);
                    break;
                }

                ret = lora_send(role_devs->dev_lora, "PONG", 4);
                if (ret < 0) {
                    printk("Lora send failed: %d\n", ret);
                    break;
                } else 
                    printk("PONG sent successfully.\n");

                k_msleep(100);
                lora_cfg.tx = false; 
                ret = lora_config(role_devs->dev_lora, &lora_cfg);
                if (ret < 0) 
                    printk("Lora rx cb: Set Lora cfg RX failed: %d\n", ret);
                
                do_pong = false;
            } while (do_pong);

        if (!do_pong && !listening) {
            lora_recv_async(role_devs->dev_display, lora_rx_cb, NULL);
            listening = true;
        }

        k_msleep(500);
    }
}

void stop_bit() {
    gpio_remove_callback(role_devs->gpio_sw0->port, &sw0_cb_data);
    k_sem_reset(&sw0_sem);
}