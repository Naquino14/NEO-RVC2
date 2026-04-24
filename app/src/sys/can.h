#ifndef CAN_H
#define CAN_H

#include <zephyr/device.h>

enum can_event_handler {
    CEH_LOG,
    CEH_SOUND,
    CEH_LORA
};

int can_init();

#endif // !CAN_H