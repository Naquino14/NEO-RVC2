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

#include <ff.h>
#include <lvgl.h>
#include <lvgl_input_device.h>

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
    int ret = gpio_pin_set_dt(&led, true);
    if (ret == -EIO) {
        LOG_ERR("LED port IO err");
        return false;
    }
    k_msleep(10);
    gpio_pin_set_dt(&led, false);
    LOG_INF("LED\t\tOK");
    return true;
}

bool bit_lora(bool call_resp) {
    lora_cfg.tx = true;
    char call[] = "PING";
    if (!call_resp)
        lora_cfg.tx_power = 2; // 2dbm
    else
        lora_cfg.tx_power = LORA_MAX_POW_DBM;
    
    int ret = lora_config(lora, &lora_cfg);
    if (ret < 0) {
        LOG_ERR("LoRa config failed: %d", ret);
        return false;
    }

    ret = lora_send(lora, call, sizeof(call));
    if (ret < 0) {
        LOG_ERR("LoRa send failed: %d", ret);
        return false;
    }

    if (call_resp) {
        char resp[] = "PONG";
        char recv[sizeof(resp)];
        int16_t rssi;
        int8_t snr;

        ret = lora_recv(lora, recv, sizeof(recv), K_MSEC(1000), &rssi, &snr);
        if (ret < 0) {
            LOG_ERR("LoRa receive failed: %d", ret);
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
    int width = DT_PROP(DT_CHOSEN(zephyr_display), width);
    int height = DT_PROP(DT_CHOSEN(zephyr_display), height);
    
    struct display_capabilities capabilities;
    display_get_capabilities(display, &capabilities);

    struct display_buffer_descriptor fbuf_descr = {
        .width = 1,
        .height = 1,
        .pitch = 1
    };

    if (ROLE_IS_TRC) {
        gpio_pin_set_dt(&blight, true);
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
                int ret = display_write(display, x, y, &fbuf_descr, &pixel);

                if (ret < 0) {
                    LOG_ERR("Display write failed: %d", ret);

                    if (ROLE_IS_TRC)
                        gpio_pin_set_dt(&blight, false);
                    
                    return false;
                }
            }
        }
        
        if (wait_sw0) 
            k_sem_take(&sw0_sem, K_FOREVER);
        else
            k_msleep(1000);

        // clear display now 
        display_blanking_on(display);
        display_blanking_off(display);

    } while ((second = !second));

    if (ROLE_IS_TRC)
        gpio_pin_set_dt(&blight, false);

    LOG_INF("Display\t\tOK");
    return true;
}

static FATFS sd_fs_fat_info;

static struct fs_mount_t sd_mnt_info = {
    .type = FS_FATFS,
    .fs_data = &sd_fs_fat_info,
    .mnt_point = "/SD:"
};

static bool bit_sdhc() {
    // mount, read, write, check write, unmount
    int ret = fs_mount(&sd_mnt_info);
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
        return false;
    }
    
    const char write_buf[11] = "SDHC\t\tOK?\n";
    ssize_t read_ret = fs_write(&tst_file, write_buf, sizeof(write_buf));
    if (read_ret < 0) {
        LOG_ERR("SD write test file failed");
        return false;
    }

    ret = fs_close(&tst_file);
    if (ret < 0) {
        LOG_ERR("SD close written test file failed");
        return false;
    }

    k_msleep(10);
    fs_file_t_init(&tst_file);
    ret = fs_open(&tst_file, test_path, FS_O_READ);
    if (ret < 0) {
        LOG_ERR("SD open test file for read failed %d", ret);
        return false;
    }

    char read_buf[sizeof(write_buf)];
    read_ret = fs_read(&tst_file, read_buf, sizeof(read_buf));
    if (read_ret < 0) {
        LOG_ERR("SD read test file failed %d", read_ret);
        return false;
    }

    ret = fs_close(&tst_file);
    if (ret < 0) {
        LOG_ERR("SD close read test file failed");
        return false;
    }

    ret = strncmp(read_buf, write_buf, sizeof(read_buf));
    if (ret != 0) {
        LOG_ERR("SD read/write data mismatch");
        return false;
    }

    // delete test file
    ret = fs_unlink(test_path);
    if (ret < 0) { 
        LOG_ERR("SD delete test file failed %d", ret);
        return false;
    }

    k_msleep(100);
    ret = fs_unmount(&sd_mnt_info);
    if (ret < 0) {
        LOG_ERR("SD unmount failed %d", ret);
        return false;
    }

    LOG_INF("SDHC\t\tOK");
    return true;
}
#endif

#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == DEF_ROLE_FOB)
static bool bit_display_ssd1306(bool wait_sw0) {
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
            int ret = display_write(display, 0, i, &fbuf_descr, fbuf);

            if (ret < 0) {
                LOG_ERR("Display write failed: %d", ret);
                return false;
            }
        }

        display_blanking_off(display);

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
    // return bit_display_ssd1306(wait_sw0);
    return true; // skip display BIT for fob for now
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

    if (sw0.port) {
        gpio_init_callback(&sw0_cb_data, button_pressed, BIT(sw0.pin));
        int ret = gpio_add_callback(sw0.port, &sw0_cb_data);
        if (ret < 0) {
            printk("Failed to register SW0 callback: %d\n", ret);
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

    if (sw0.port) {
        static struct gpio_callback sw0_cb_data;
        gpio_init_callback(&sw0_cb_data, button_pressed, BIT(sw0.pin));
        ret = gpio_add_callback(sw0.port, &sw0_cb_data);
        if (ret < 0) 
            printk("Failed to register SW0 callback: %d\n", ret);
    }

    if (lora) do {
                if (!device_is_ready(lora)) 
                    printk("Lora device is not ready\n");
        
                lora_cfg.tx = (role_get() == ROLE_FOB);

                ret = lora_config(lora, &lora_cfg);
                if (ret < 0) {
                    printk("Lora config failed: %d\n", ret);
                    break;
                }
                
                if (lora && role_get() == ROLE_TRC) {
                    ret = lora_recv_async(lora, lora_rx_cb, NULL);
                    if (ret < 0) {
                        printk("LoRa callback register failed: %d\n", ret);
                        break;
                    }
                    listening = true;
                    printk("Lora OK\n");
                }
            } while (false);

    
    // use graphics library to BIT screen
    if (display) do {
                if (!device_is_ready(display)) {
                    printk("Display device not ready\n");
                    break;
                }

                display_blanking_off(display);

                if (ROLE_IS_TRC) 
                    gpio_pin_set_dt(&blight, true);

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
        /*int ret = */gpio_pin_toggle_dt(&led);
        // int read_state = gpio_pin_get_dt(&led);
        // printk("Hello World! %s\tState: %s\tRead State: %s\t", CONFIG_BOARD, state ? "ON " : "OFF", read_state == GPIO_OUTPUT_HIGH ? "HI" : "LO");
        // printk("%s%d\n\n", ret == 0 ? "    OK: " : "NOT OK: ", ret);

        // state = !state;
        if (sw0_ok && sw0.port) {
            sw0_ok = false;
            // display sw0 pressed on display here eventually

            if (lora && role_get() == ROLE_FOB) {
                // send ping
                char data[] = "PING";
                printk("Pinging TRC...\n");
                ret = lora_send(lora, data, sizeof(data));
                if (ret < 0) {
                    printk("Lora send failed: %d\n", ret);
                }

                // listen for pong
                int16_t rssi;
                uint8_t snr;
                ret = lora_recv(lora, data, sizeof(data), K_MSEC(10000), &rssi, &snr);
                if (ret < 0) {
                    printk("Lora recv failed: %d\n", ret);
                } else {
                    printk("PONG received: %s, RSSI: %d, SNR: %d\n", data, rssi, snr);
                    // display pong on display here eventually
                }
            }
        }

        if (lora && do_pong) do {
                printk("Pinged, ponging...\n");

                // stop rx
                lora_recv_async(lora, NULL, NULL);
                listening = false;
                
                lora_cfg.tx = true; 
                int ret = lora_config(lora, &lora_cfg);
                if (ret < 0) {
                    printk("Lora rx cb: Set Lora cfg TX failed: %d\n", ret);
                    break;
                }

                ret = lora_send(lora, "PONG", 4);
                if (ret < 0) {
                    printk("Lora send failed: %d\n", ret);
                    break;
                } else 
                    printk("PONG sent successfully.\n");

                k_msleep(100);
                lora_cfg.tx = false; 
                ret = lora_config(lora, &lora_cfg);
                if (ret < 0) 
                    printk("Lora rx cb: Set Lora cfg RX failed: %d\n", ret);
                
                do_pong = false;
            } while (do_pong);

        if (!do_pong && !listening) {
            lora_recv_async(lora, lora_rx_cb, NULL);
            listening = true;
        }

        k_msleep(500);
    }
}

void stop_bit() {
    gpio_remove_callback(sw0.port, &sw0_cb_data);
    k_sem_reset(&sw0_sem);
}