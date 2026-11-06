#ifndef CAN_H
#define CAN_H

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <stdbool.h>

int can_init();

bool can_rdy();

int can_dev_init(const struct device* dev, const char* devname);

enum can_state get_can_state(const struct device* dev);

const char* can_state_tostr(enum can_state state);

#endif // !CAN_H