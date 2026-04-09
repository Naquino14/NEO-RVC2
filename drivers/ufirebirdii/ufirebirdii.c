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

#define UFBII_UM68xA_LATLON_SIZE 11 // ddmm.mmmmmmm (13 + nul)
#define UFBII_DEFAULT_LATLON_SIZE 10 // ddmm.mmmmm (11 + nul) 
static int nmea_lat_or_lon_to_struct(coord_t* out, const char* str, uint32_t len, enum ufirebirdii_variant variant) {
    int latstr_len;
    if (variant == UFBII_VARIANT_UM670A || variant == UFBII_VARIANT_UM680A || variant == UFBII_VARIANT_UM681A)
        latstr_len = UFBII_UM68xA_LATLON_SIZE;
    else
        latstr_len = UFBII_DEFAULT_LATLON_SIZE;

    int ret = sscanf(str, "%2d%lf", &out->deg, &out->min);
    if (ret != 2)
        return -EINVAL;

    return 0;
}

#define CTOI(x) ((x) - '0')

static int nmea_time_tostring(time_t* time, const char* str, uint32_t len) {
    // Expected format: hhmmss.ss (9 char + nul)
    if (len < 9 || time == NULL)
        return -EINVAL;
    
    struct tm tm = {0};
    tm.tm_hour = CTOI(str[0]) * 10 + CTOI(str[1]);
    tm.tm_min = CTOI(str[2]) * 10 + CTOI(str[3]);
    tm.tm_sec = CTOI(str[4]) * 10 + CTOI(str[5]);
    *time = mktime(&tm);

    return 0;
}

// Parsers

static const int echo(int sentence_idx, const char* content, uint32_t content_len, struct ufirebirdii_fix* fix, struct ufirebirdii_driver_config* cfg) {
    LOG_INF("%s%s", parser_table[sentence_idx].sentence_type, content);
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


#define DEFAULT_FORMAT "%2d%2d%5lf,%2d%7ld,%c,%2d%7ld,%c,%d,%d,%lf,%lf,M,%ld,M,,"
#define FORMAT_NUM_FIELDS 14
static const int parse_gga(int sentence_idx, const char* content, uint32_t content_len, struct ufirebirdii_fix* fix, struct ufirebirdii_driver_config* cfg) {
    struct ufirebirdii_fix local_fix = {0};

//     char* token = content;
//     char* next_token = memchr(token, ',', content_len);
//     if (!next_token)
//         return -EINVAL;
//     uint32_t token_len = next_token - token;
        
//     time_t timestamp = 0;
//     int ret = nmea_time_tostring(&timestamp, token, token_len);
//     if (ret < 0) 
//         return ret;
//     local_fix.timestamp = timestamp;

//     token = next_token + 1;
//     next_token = memchr(token, ',', content + content_len - token);
//     if (!next_token)
//         return -EINVAL;
//     token_len = next_token - token;

//    coord_t lat;
//    ret = nmea_lat_or_lon_to_struct(&lat, token, token_len, cfg->variant);
//    if (ret < 0)
//         return ret;
//     local_fix.latitude = lat;

//     token = next_token + 1;
//     next_token = memchr(token, ',', content + content_len - token);
//     if (!next_token)
//         return -EINVAL;
//     token_len = next_token - token;

//     coord_t lon;
//     ret = nmea_lat_or_lon_to_struct(&lon, token, token_len, cfg->variant);
//     if (ret < 0)
//         return ret;
//     local_fix.longitude = lon;

//     token = next_token + 1;
//     next_token = memchr(token, ',', content + content_len - token);
//     if (!next_token)
//         return -EINVAL;
//     token_len = next_token - token;
    
//     if (token_len != 1)
//         return -EINVAL;
//     uint8_t fix_validity = CTOI(token[0]);

//     if (fix_validity == 3 || fix_validity > 6)
//         return -EINVAL; 
//     local_fix.valid = fix_validity > 0;
//     local_fix.validity = fix_validity;

//     token = next_token + 1;
//     next_token = memchr(token, ',', content + content_len - token);
//     if (!next_token)        
//         return -EINVAL;
//     token_len = next_token - token;

//     ret = sscanf(token, "%d", &local_fix.altitude);

//     token = next_token + 1;
//     next_token = memchr(token, ',', content + content_len - token);
//     if (!next_token)        
//         return -EINVAL;
//     token_len = next_token - token;
    
//     ret = sscanf(token, "%lf", &local_fix.hdop);
//     if (ret != 1)        
//         return -EINVAL;
    
//     token = next_token + 1;
//     next_token = memchr(token, ',', content + content_len - token);
//     if (!next_token)
//         return -EINVAL;
//     token_len = next_token - token;



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
    {.sentence_type = "GAGGA", .parser = NULL},
    {.sentence_type = "GAGLL", .parser = NULL},
    {.sentence_type = "GAGSA", .parser = NULL},
    {.sentence_type = "GAGST", .parser = NULL},
    {.sentence_type = "GARMC", .parser = NULL},
    {.sentence_type = "GAVTG", .parser = NULL},
    {.sentence_type = "GAZDA", .parser = NULL},
    {.sentence_type = "GBGBS", .parser = NULL},
    {.sentence_type = "GBGGA", .parser = NULL},
    {.sentence_type = "GBGLL", .parser = NULL},
    {.sentence_type = "GBGSA", .parser = NULL},
    {.sentence_type = "GBGST", .parser = NULL},
    {.sentence_type = "GBRMC", .parser = NULL},
    {.sentence_type = "GBVTG", .parser = NULL},
    {.sentence_type = "GBZDA", .parser = NULL},
    {.sentence_type = "GIGBS", .parser = NULL},
    {.sentence_type = "GIGGA", .parser = NULL},
    {.sentence_type = "GIGLL", .parser = NULL},
    {.sentence_type = "GIGSA", .parser = NULL},
    {.sentence_type = "GIGST", .parser = NULL},
    {.sentence_type = "GIRMC", .parser = NULL},
    {.sentence_type = "GIVTG", .parser = NULL},
    {.sentence_type = "GIZDA", .parser = NULL},
    {.sentence_type = "GLGBS", .parser = NULL},
    {.sentence_type = "GLGGA", .parser = NULL},
    {.sentence_type = "GLGLL", .parser = NULL},
    {.sentence_type = "GLGSA", .parser = NULL},
    {.sentence_type = "GLGST", .parser = NULL},
    {.sentence_type = "GLRMC", .parser = NULL},
    {.sentence_type = "GLVTG", .parser = NULL},
    {.sentence_type = "GLZDA", .parser = NULL},
    {.sentence_type = "GNGBS", .parser = NULL},
    {.sentence_type = "GNGGA", .parser = NULL},
    {.sentence_type = "GNGLL", .parser = NULL},
    {.sentence_type = "GNGSA", .parser = NULL},
    {.sentence_type = "GNGST", .parser = NULL},
    {.sentence_type = "GNRMC", .parser = NULL},
    {.sentence_type = "GNVTG", .parser = NULL},
    {.sentence_type = "GNZDA", .parser = NULL},
    {.sentence_type = "GPGBS", .parser = NULL},
    {.sentence_type = "GPGGA", .parser = NULL},
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
    if (cfg->user_config.do_checksum) {
        if (sentence_len < 4 || sentence[sentence_len - 3] != '*')
            return -EINVAL;

        char* endp;
        char csum_ptr[3] = {sentence[sentence_len - 2], sentence[sentence_len - 1], '\0'};

        uint8_t expect = (uint8_t)strtol(csum_ptr, &endp, 16);

        if (endp == csum_ptr)
            return -EINVAL;

        uint8_t actual = 0;
        for (uint32_t i = 1; i < sentence_len - 3; i++) 
            actual ^= sentence[i];
        if (expect != actual) {
            return -EILSEQ;
        }
    }

    const char* sentence_type = memchr(sentence, ',', sentence_len);
    if (!sentence_type)        
        return -EINVAL;
    uint32_t sentence_type_len = sentence_type - sentence - 1;

    // binary search logic here

    // test: echo everything
    echo(88, sentence + sentence_type_len + 1, sentence_len - sentence_type_len - 1, NULL, NULL);

    return 0;
}
