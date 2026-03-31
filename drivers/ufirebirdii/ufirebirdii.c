#include <drivers/ufirebirdii/ufirebirdii.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include <errno.h>

LOG_MODULE_REGISTER(ufirebirdii);

typedef const int (*ufbii_parser_t)(int sentence_idx, const char* content, uint32_t content_len);

struct ufbii_parser_table {
    const char* sentence_type;
    ufbii_parser_t parser;
};

const struct ufbii_parser_table parser_table[]; // fwd declaration

// Parsers

static const int echo(int sentence_idx, const char* content, uint32_t content_len) {
    LOG_INF("%s%s", parser_table[sentence_idx].sentence_type, content);
    return 0;
}

static const int ok(int sentence_idx, const char* content, uint32_t content_len) {
    LOG_INF("OK");
    return 0;
}

static const int fail(int sentence_idx, const char* content, uint32_t content_len) {
    if (content[1] == '1') 
        LOG_ERR("FAIL: Input to UFirebird II Device Checksum Invalid");
    else if (content[1] == '0')
        LOG_ERR("FAIL: Invalid parameters in command sent to UFirebird II Device");
    else
        return -EINVAL; // should never happen :shrug:

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
    echo(88, sentence + sentence_type_len + 1, sentence_len - sentence_type_len - 1);

    return 0;
}
