// Copyright (c) 2026 Nate Aquino
// SPDX-License-Identifier: Apache-2.0
//
// See manual: https://en.unicore.com/uploads/file/UFirebirdII%20Series%20Protocol%20Specification_EN_R1.2.pdf

#ifndef UFIREBIRDII_H
#define UFIREBIRDII_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <time.h>

enum ufbii_direction {
    DIRECTION_NORTH = 'N',
    DIRECTION_EAST = 'E',
    DIRECTION_SOUTH = 'S',
    DIRECTION_WEST = 'W',
    DIRECTION_INVALID = 'I'
};

typedef struct {
    int deg;
    double min;
    enum ufbii_direction dir;
} coord_t;

enum ufbii_fix_validity {
    FIX_VALIDITY_INVALID = 0,
    FIX_VALITIDY_SINGLE_POINT_POSITIONING = 1,
    FIX_VALITIDY_DIFFERENTIAL_POSITIONING = 2,
    FIX_VALITIDY_RTK_FIX_SOLUTION = 4,
    FIX_VALITIDY_RTK_FLOAT_SOLUTION = 5,
    FIX_VALITIDY_INS_POSITIONING = 6,
};

struct ufirebirdii_fix {
    coord_t latitude;
    coord_t longitude;
    double altitude;
    double hdop;
    uint8_t satellites;
    bool valid;
    enum ufbii_fix_validity validity;
};

#define UFBII_ADDR_I2C 0x46
#define UFBII_ADDR_DEFAULT 0x00

enum ufirebirdii_variant {
    UFBII_VARIANT_UC6580,
    UFBII_VARIANT_UM670A,
    UFBII_VARIANT_UM680A,
    UFBII_VARIANT_UM681A,
    UFBII_VARIANT_UM621,
    UFBII_VARIANT_UM621N
};

enum ufirebirdii_port_id {
    UFBII_PORT_ID_I2C   = 0x0,
    UFBII_PORT_ID_UART1 = 0x1,
    UFBII_PORT_ID_UART2 = 0x2,
    UFBII_PORT_ID_SPI   = 0x4,
};

enum ufirebirdii_inpro {
    UFBII_INPRO_UNICORE         = 0x0,      // DO NOT DISABLE (Page 27)
    UFBII_INPRO_RTCM3_X         = 0x40,
    UFBII_INPRO_MAPFB_ODODATA   = 0x400,
};

enum ufirebirdii_outpro  {
    UFBII_OUTPRO_UNICORE        = 0x0,
    UFBII_OUTPRO_NMEA           = 0x1, 
    UFBII_OUTPRO_RTCM3_X        = 0x2,
    UFBII_OUTPRO_NOTICE         = 0x10,
    UFBII_OUTPRO_ERTCM4074DR    = 0x40,
    UFBII_OUTPRO_ERTCM4074PVT   = 0x80,
};

enum ufirebirdii_msg_id {
    UFBII_MSG_ID_NMEA_GGA = 0,
    UFBII_MSG_ID_NMEA_GLL = 1,
    UFBII_MSG_ID_NMEA_GSA = 2,
    UFBII_MSG_ID_NMEA_GSV = 3,
    UFBII_MSG_ID_NMEA_RMC = 4,
    UFBII_MSG_ID_NMEA_VTG = 5,
    UFBII_MSG_ID_NMEA_ZDA = 6,
    UFBII_MSG_ID_NMEA_GST = 7,
    UFBII_MSG_ID_NMEA_GBS = 8,
    UFBII_MSG_ID_OBSERVATION_RTCM_MSM = 3,
    UFBII_MSG_ID_OBSERVATION_RTCM_EPH = 4,
    UFBII_MSG_ID_OBSERVATION_RTCM_STM = 5,
    UFBII_MSG_ID_OBSERVATION_SYS_PARAM = 14,
    UFBII_MSG_ID_SENSOR_FUSION_GYOACC = 0,
    UFBII_MSG_ID_SENSOR_FUSION_SNRSTAT = 1,
    UFBII_MSG_ID_SENSOR_FUSION_NAVATT = 2,
    UFBII_MSG_ID_SENSOR_FUSION_IMURAW = 3,
    UFBII_MSG_ID_SENSOR_FUSION_INSPVA = 4,
    UFBII_MSG_ID_SENSOR_FUSION_IMUVEH = 5,
    UFBII_MSG_ID_MISC_CWOUT = 0,
    UFBII_MSG_ID_MISC_OSNMA = 1,
    UFBII_MSG_ID_NOTICE_GENERAL = 0,
    UFBII_MSG_ID_NOTICE_MESS_PKG = 1,
    UFBII_MSG_ID_2ND_NMEA_GGA = 0,
    UFBII_MSG_ID_2ND_NMEA_GLL = 1,
    UFBII_MSG_ID_2ND_NMEA_GSA = 2,
    UFBII_MSG_ID_2ND_NMEA_GSV = 3,
    UFBII_MSG_ID_2ND_NMEA_RMC = 4,
    UFBII_MSG_ID_2ND_NMEA_VTG = 5,
    UFBII_MSG_ID_2ND_NMEA_ZDA = 6,
    UFBII_MSG_ID_2ND_NMEA_GST = 7,
    UFBII_MSG_ID_2ND_NMEA_GBS = 8,
    UFBII_MSG_ID_OBSERVATION_SENSOR_GYOACC = 0,
    UFBII_MSG_ID_OBSERVATION_SENSOR_SNRSTAT = 1,
    UFBII_MSG_ID_OBSERVATION_SENSOR_NAVATT = 2,
    UFBII_MSG_ID_OBSERVATION_SENSOR_IMURAW = 3,
    UFBII_MSG_ID_OBSERVATION_SENSOR_INSPVA = 4,
    UFBII_MSG_ID_OBSERVATION_SENSOR_IMUVEH = 5,
    UFBII_MSG_ID_OBSERVATION_RF_RX_INFO = 1,
    UFBII_MSG_ID_OBSERVATION_RF_SIG_INFO = 2,
    UFBII_MSG_ID_OBSERVATION_RF_TGD = 3,
    UFBII_MSG_ID_OBSERVATION_RF_ION = 4,
};

enum ufirebirdii_msg_class {
    UFBII_MSG_CLASS_NMEA = 0,
    UFBII_MSG_CLASS_OBSERVATION = 2, // UM670A & UM680A & UM681A
    UFBII_MSG_CLASS_SENSOR_FUSION = 4, // UM621 & UM681A
    UFBII_MSG_CLASS_MISC = 5,
    UFBII_MSG_CLASS_NOTICE = 6,
    UFBII_MSG_CLASS_2ND_NMEA = 7, // UM621N
    UFBII_MSG_CLASS_OBSERVATION_SENSOR = 8, // UM621 & UM681A
    UFBII_MSG_CLASS_OBSERVATION_RF = 9, // UC6580 & UM670A & UM680A
};

struct ufirebirdii_user_config {
    bool do_checksum;
    // TODO
};

struct ufirebirdii_driver_config {
    uint32_t baud;
    uint8_t addr;
    enum ufirebirdii_variant variant;
    enum ufirebirdii_inpro inpro;
    enum ufirebirdii_outpro outpro;
    struct ufirebirdii_user_config user_config;
    // TODO
};

struct ufirebirdii_api {
    int (*start)(const struct device* dev);
    int (*stop)(const struct device* dev);
    int (*get_fix)(const struct device* dev, struct ufirebirdii_fix* fix);
};

static inline int ufirebirdii_start(const struct device* dev) {
    return ((const struct ufirebirdii_api*)dev->api)->start(dev);
}

static inline int ufirebirdii_stop(const struct device* dev) {
    return ((const struct ufirebirdii_api*)dev->api)->stop(dev);
}

static inline int ufirebirdii_get_fix(const struct device* dev, struct ufirebirdii_fix* fix) {
    return ((const struct ufirebirdii_api*)dev->api)->get_fix(dev, fix);
}

// ufirebirdii api
int ufirebirdii_parse_sentence(const char *sentence, uint32_t sentence_len, struct ufirebirdii_fix *fix, struct ufirebirdii_driver_config* cfg);

#endif // UFIREBIRDII_H
