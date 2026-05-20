#ifndef CAN_H
#define CAN_H

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>

int can_init();

enum can_state get_can_state(const struct device* dev);

#endif // !CAN_H