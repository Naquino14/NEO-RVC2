#include <drivers/ufirebirdii/ufirebirdii.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(ufirebirdii);

typedef const int (*ufbii_parser_t)(int sentence_idx, const char* content, uint32_t content_len, struct ufirebirdii_fix* fix, struct ufirebirdii_driver_config* cfg);

struct ufbii_parser_table {
    const char* sentence_type;
    ufbii_parser_t parser;
};

const struct ufbii_parser_table parser_table[]; // fwd declaration

// Helpers

// Parsers

static const int echo(int sentence_idx, const char* content, uint32_t content_len, struct ufirebirdii_fix* fix, struct ufirebirdii_driver_config* cfg) {
    LOG_INF("%s,%.*s", parser_table[sentence_idx].sentence_type, content_len, content);
    return 0;
}

static const int ok(int sentence_idx, const char* content, uint32_t content_len, struct ufirebirdii_fix* fix, struct ufirebirdii_driver_config* cfg) {
    LOG_INF("OK");
    return 0;
}

static const int fail(int sentence_idx, const char* content, uint32_t content_len, struct ufirebirdii_fix* fix, struct ufirebirdii_driver_config* cfg) {
    if (content[1] == '1') 
        LOG_ERR("FAIL: Input to UFirebird II Device Checksum Invalid");
    else if (content[1] == '0')
        LOG_ERR("FAIL: Invalid parameters in command sent to UFirebird II Device");
    else
        return -EINVAL; // should never happen :shrug:

    return 0;
}


#define DEFAULT_FORMAT "%3d%lf,%c,%3d%lf,%c,%d,%d,%lf,%lf"
#define FORMAT_NUM_FIELDS 10
#define GGA_PARSE_BUF_SZ 128

static const int parse_gga(int sentence_idx, const char* content, uint32_t content_len, struct ufirebirdii_fix* fix, struct ufirebirdii_driver_config* cfg) {
    struct ufirebirdii_fix local_fix = {0};
    fix->valid = false;
    
    const char* content_no_ts = memchr(content, ',', content_len);
    if (content_no_ts == NULL || (content_no_ts + 1) >= (content + content_len)) {
        return -EINVAL;
    }

    content_no_ts += 1;
    
    size_t n = (size_t)((content + content_len) - content_no_ts);
    if (n >= GGA_PARSE_BUF_SZ) {
        return -EINVAL;
    }
    
    char buf[GGA_PARSE_BUF_SZ];
    memcpy(buf, content_no_ts, n);
    buf[n] = '\0';

    int ret = sscanf(buf, DEFAULT_FORMAT,
        &local_fix.latitude.deg, &local_fix.latitude.min, (char*)&local_fix.latitude.dir,
        &local_fix.longitude.deg, &local_fix.longitude.min, (char*)&local_fix.longitude.dir,
        (int*)&local_fix.validity,
        (int*)&local_fix.satellites,
        &local_fix.hdop,
        &local_fix.altitude
    );

    // DELETEME debugging parsing:
    if (ret != FORMAT_NUM_FIELDS) {
        // Only log error when debugging as invalid fixes are common during cold start and trigger this
        // LOG_ERR("Failed to parse GGA sentence, expected %d fields but got %d", FORMAT_NUM_FIELDS, ret);
        return -EINVAL;
    }

    fix->altitude = local_fix.altitude;
    fix->hdop = local_fix.hdop;
    fix->satellites = local_fix.satellites;
    fix->validity = local_fix.validity;
    fix->latitude = local_fix.latitude;
    fix->longitude = local_fix.longitude;
    fix->valid = true;
    return 0;
}

// NO PLANNED SUPPORT YET: Any messages not applicable to UC6580
// parser_table MUST BE SORTED
const struct ufbii_parser_table parser_table[] = {
    {.sentence_type = "AIDINFO", .parser = echo},
    {.sentence_type = "ANTSTAT", .parser = echo},
    {.sentence_type = "ANTSTAT", .parser = echo},
    {.sentence_type = "CFGACC", .parser = echo},
    {.sentence_type = "CFGCOG", .parser = echo},
    {.sentence_type = "CFGDOP", .parser = echo},
    {.sentence_type = "CFGDYN", .parser = echo},
    {.sentence_type = "CFGELAPSEC", .parser = echo},
    {.sentence_type = "CFGEOID", .parser = echo},
    {.sentence_type = "CFGFWCHECK", .parser = echo},
    {.sentence_type = "CFGGLARM", .parser = echo},
    {.sentence_type = "CFGILARM", .parser = echo},
    {.sentence_type = "CFGIMUMEAS", .parser = echo},
    {.sentence_type = "CFGINS", .parser = echo},
    {.sentence_type = "CFGKILOWEEK", .parser = echo},
    {.sentence_type = "CFGLOGLIST", .parser = echo},
    {.sentence_type = "CFGMSG", .parser = echo},
    {.sentence_type = "CFGMSK", .parser = echo},
    {.sentence_type = "CFGMSM", .parser = echo},
    {.sentence_type = "CFGNAV", .parser = echo},
    {.sentence_type = "CFGNMEA", .parser = echo},
    {.sentence_type = "CFGNMEAMODE", .parser = echo},
    {.sentence_type = "CFGODOFWD", .parser = echo},
    {.sentence_type = "CFGPRT", .parser = echo},
    {.sentence_type = "CFGROTAT", .parser = echo},
    {.sentence_type = "CFGRTK", .parser = echo},
    {.sentence_type = "CFGSYS", .parser = echo},
    {.sentence_type = "CFGTP", .parser = echo},
    {.sentence_type = "CFGWMODE", .parser = echo},
    {.sentence_type = "ENVINFO", .parser = echo},
    {.sentence_type = "FAIL", .parser = fail},
    {.sentence_type = "GAGBS", .parser = NULL},
    {.sentence_type = "GAGGA", .parser = parse_gga},
    {.sentence_type = "GAGLL", .parser = NULL},
    {.sentence_type = "GAGSA", .parser = NULL},
    {.sentence_type = "GAGST", .parser = NULL},
    {.sentence_type = "GARMC", .parser = NULL},
    {.sentence_type = "GAVTG", .parser = NULL},
    {.sentence_type = "GAZDA", .parser = NULL},
    {.sentence_type = "GBGBS", .parser = NULL},
    {.sentence_type = "GBGGA", .parser = parse_gga},
    {.sentence_type = "GBGLL", .parser = NULL},
    {.sentence_type = "GBGSA", .parser = NULL},
    {.sentence_type = "GBGST", .parser = NULL},
    {.sentence_type = "GBRMC", .parser = NULL},
    {.sentence_type = "GBVTG", .parser = NULL},
    {.sentence_type = "GBZDA", .parser = NULL},
    {.sentence_type = "GIGBS", .parser = NULL},
    {.sentence_type = "GIGGA", .parser = parse_gga},
    {.sentence_type = "GIGLL", .parser = NULL},
    {.sentence_type = "GIGSA", .parser = NULL},
    {.sentence_type = "GIGST", .parser = NULL},
    {.sentence_type = "GIRMC", .parser = NULL},
    {.sentence_type = "GIVTG", .parser = NULL},
    {.sentence_type = "GIZDA", .parser = NULL},
    {.sentence_type = "GLGBS", .parser = NULL},
    {.sentence_type = "GLGGA", .parser = parse_gga},
    {.sentence_type = "GLGLL", .parser = NULL},
    {.sentence_type = "GLGSA", .parser = NULL},
    {.sentence_type = "GLGST", .parser = NULL},
    {.sentence_type = "GLRMC", .parser = NULL},
    {.sentence_type = "GLVTG", .parser = NULL},
    {.sentence_type = "GLZDA", .parser = NULL},
    {.sentence_type = "GNGBS", .parser = NULL},
    {.sentence_type = "GNGGA", .parser = parse_gga},
    {.sentence_type = "GNGLL", .parser = NULL},
    {.sentence_type = "GNGSA", .parser = NULL},
    {.sentence_type = "GNGST", .parser = NULL},
    {.sentence_type = "GNRMC", .parser = NULL},
    {.sentence_type = "GNVTG", .parser = NULL},
    {.sentence_type = "GNZDA", .parser = NULL},
    {.sentence_type = "GPGBS", .parser = NULL},
    {.sentence_type = "GPGGA", .parser = parse_gga},
    {.sentence_type = "GPGLL", .parser = NULL},
    {.sentence_type = "GPGSA", .parser = NULL},
    {.sentence_type = "GPGST", .parser = NULL},
    {.sentence_type = "GPRMC", .parser = NULL},
    {.sentence_type = "GPVTG", .parser = NULL},
    {.sentence_type = "GPZDA", .parser = NULL},
    {.sentence_type = "GYOACC", .parser = echo},
    {.sentence_type = "IMURAW", .parser = echo},
    {.sentence_type = "IMUVEH", .parser = echo},
    {.sentence_type = "INSPVA", .parser = echo},
    {.sentence_type = "INSTALL", .parser = echo},
    {.sentence_type = "LSF", .parser = echo},
    {.sentence_type = "MAPFB", .parser = echo},
    {.sentence_type = "NAVATT", .parser = echo},
    {.sentence_type = "ODODATA", .parser = echo},
    {.sentence_type = "OK", .parser = ok},
    {.sentence_type = "PDTINFO", .parser = echo},
    {.sentence_type = "PNAVMSG", .parser = echo},
    {.sentence_type = "PRODUCTINFO", .parser = echo},
    {.sentence_type = "QZQSM", .parser = echo},
    {.sentence_type = "SNRSTAT", .parser = echo},
};

int ufirebirdii_init(const struct device* dev, struct ufirebirdii_driver_config* cfg) {
    return 0;
}

int ufirebirdii_parse_sentence(const char *sentence, uint32_t sentence_len, struct ufirebirdii_fix *fix, struct ufirebirdii_driver_config* cfg) {
    fix->valid = false;

    if (cfg->user_config.do_checksum) {
        if (sentence_len < 4 || sentence[sentence_len - 3] != '*')
            return -EINVAL;

        char* endp;
        char csum_ptr[3] = {sentence[sentence_len - 2], sentence[sentence_len - 1], '\0'};
        uint8_t expect = (uint8_t)strtol(csum_ptr, &endp, 16);

        if (endp == csum_ptr || *endp != '\0')
            return -EINVAL;

        uint8_t actual = 0;
        for (uint32_t i = 1; i < sentence_len - 3; i++)
            actual ^= (uint8_t)sentence[i];

        if (expect != actual)
            return -EILSEQ;
    }

    const char* sentence_type = memchr(sentence, ',', sentence_len);
    if (!sentence_type)
        return -EINVAL;

    uint32_t sentence_type_len = (uint32_t)(sentence_type - sentence - 1);

    int left = 0;
    int right = (int)(sizeof(parser_table) / sizeof(parser_table[0])) - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = strncmp(sentence + 1, parser_table[mid].sentence_type, sentence_type_len);
        if (cmp == 0) {
            if (!parser_table[mid].parser)
                return -ENOTSUP;

            const char *payload = sentence + sentence_type_len + 2;
            uint32_t payload_len = sentence_len - sentence_type_len - 2;

            const char *star = memchr(payload, '*', payload_len); // strip off checksum
            if (star != NULL) {
                payload_len = (uint32_t)(star - payload);
            }

            return parser_table[mid].parser(mid, payload, payload_len, fix, cfg);
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    return -ENOTSUP;
}
