#ifndef LEDTACH_H
#define LEDTACH_H

#include <zephyr/drivers/led_strip.h>
#include <stdbool.h>

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }
#define SCALE(x, s) ((uint8_t)(((uint16_t)(x) * s + 127) / 255))
#define SCALE_MAXBRIGHT(x) SCALE(x, CONFIG_LEDTACH_MAXBRIGHT)

int ledtach_init();

int ledtach_pix_set(size_t idx, struct led_rgb newcolor);

int ledtach_flush();

bool ledtach_rdy();

#endif