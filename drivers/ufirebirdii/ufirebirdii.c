#include <drivers/ufirebirdii/ufirebirdii.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/hash_map.h>

LOG_MODULE_REGISTER(ufirebirdii);

SYS_HASHMAP_DEFINE_STATIC(ufbii_handlers);

typedef const int (*ufbii_parser_t)(const char* sentence);

struct ufbii_parser_table {
    const char* sentence_type;
    ufbii_parser_t parser;
};

static const struct ufbii_parser_table parser_table[] = {
    {.sentence_type = "GPGGA", .parser = NULL},
    {.sentence_type = "GBGGA", .parser = NULL},
    {.sentence_type = "GAGGA", .parser = NULL},
    {.sentence_type = "GLGGA", .parser = NULL},
    {.sentence_type = "GIGGA", .parser = NULL},
    {.sentence_type = "GNGGA", .parser = NULL},
    {.sentence_type = "GPGLL", .parser = NULL},
    {.sentence_type = "GBGLL", .parser = NULL},
    {.sentence_type = "GAGLL", .parser = NULL},
    {.sentence_type = "GLGLL", .parser = NULL},
    {.sentence_type = "GIGLL", .parser = NULL},
    {.sentence_type = "GNGLL", .parser = NULL},
    {.sentence_type = "GPGSA", .parser = NULL},
    {.sentence_type = "GBGSA", .parser = NULL},
    {.sentence_type = "GAGSA", .parser = NULL},
    {.sentence_type = "GLGSA", .parser = NULL},
    {.sentence_type = "GIGSA", .parser = NULL},
    {.sentence_type = "GNGSA", .parser = NULL},
    {.sentence_type = "GPRMC", .parser = NULL},
    {.sentence_type = "GBRMC", .parser = NULL},
    {.sentence_type = "GARMC", .parser = NULL},
    {.sentence_type = "GLRMC", .parser = NULL},
    {.sentence_type = "GIRMC", .parser = NULL},
    {.sentence_type = "GNRMC", .parser = NULL},
    {.sentence_type = "GPVTG", .parser = NULL},
    {.sentence_type = "GBVTG", .parser = NULL},
    {.sentence_type = "GAVTG", .parser = NULL},
    {.sentence_type = "GLVTG", .parser = NULL},
    {.sentence_type = "GIVTG", .parser = NULL},
    {.sentence_type = "GNVTG", .parser = NULL},
    {.sentence_type = "GPZDA", .parser = NULL},
    {.sentence_type = "GBZDA", .parser = NULL},
    {.sentence_type = "GAZDA", .parser = NULL},
    {.sentence_type = "GLZDA", .parser = NULL},
    {.sentence_type = "GIZDA", .parser = NULL},
    {.sentence_type = "GNZDA", .parser = NULL},
    {.sentence_type = "GPGST", .parser = NULL},
    {.sentence_type = "GBGST", .parser = NULL},
    {.sentence_type = "GAGST", .parser = NULL},
    {.sentence_type = "GLGST", .parser = NULL},
    {.sentence_type = "GIGST", .parser = NULL},
    {.sentence_type = "GNGST", .parser = NULL},
    {.sentence_type = "GPGBS", .parser = NULL},
    {.sentence_type = "GBGBS", .parser = NULL},
    {.sentence_type = "GAGBS", .parser = NULL},
    {.sentence_type = "GLGBS", .parser = NULL},
    {.sentence_type = "GIGBS", .parser = NULL},
    {.sentence_type = "GNGBS", .parser = NULL},
};

int ufirebirdii_init(const struct device* dev, struct ufirebirdii_driver_config* cfg) {
    sys_hashmap_insert(&ufbii_handlers, 
    return 0;
}

int ufirebirdii_parse_sentence(const char *sentence, struct ufirebirdii_fix *fix, struct ufirebirdii_driver_config* cfg) {
    
    // LOG_INF("Parsing sentence: %s", sentence);
    return 0;
}
