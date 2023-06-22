#include "logger.h"

LOG_LEVEL current_level = INFO;
static const char *description[] = {"DEBUG", "INFO", "ERROR", "FATAL"};

void set_log_level(LOG_LEVEL newLevel)
{
    if (newLevel >= DEBUG && newLevel <= FATAL)
        current_level = newLevel;
}

const char *level_description(LOG_LEVEL level)
{
    if (level < DEBUG || level > FATAL)
        return "";
    return description[level];
}
