#ifndef CAN_H
#define CAN_H

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>

enum can_event_handler {
    CEH_LOG,
    CEH_SOUND,
    CEH_LORA
};

int can_init();

enum can_state get_can_state(const struct device* dev);

#endif // !CAN_H