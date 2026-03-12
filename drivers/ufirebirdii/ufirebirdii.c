#include <drivers/ufirebirdii/ufirebirdii.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ufirebirdii);

int ufirebirdii_parse_sentence(const char *sentence, struct ufirebirdii_fix *fix) {
    // LOG_INF("Parsing sentence: %s", sentence);
    return 0;
}
