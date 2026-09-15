#include <nrvc2_security.h>

#include "../roles.h"

#include "comms.h"

static bool rdy = false;
bool comms_rdy() {
    return rdy;
}

int comms_init() {
    rdy = true;
    return 0;
}

int commms_transmit(uint8_t *txbuf, size_t txbuf_len) {
    return 0;
}
