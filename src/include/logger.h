#ifndef LOGGER_H
#define LOGGER_H
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define ERROR_LOG_LENGTH 64

#define LOG_AND_RETURN(error_level, error_message, return_value)           \
    do                                                                     \
    {                                                                      \
        log((error_level), "%s - %s\n", (error_message), strerror(errno)); \
        return (return_value);                                             \
    } while (0)

typedef enum
{
	DEBUG = 0,
	INFO,
	ERROR,
	FATAL
} LOG_LEVEL;

extern LOG_LEVEL current_level;

/**
 *  Minimo nivel de log a registrar. Cualquier llamada a log con un nivel mayor a newLevel sera ignorada
 **/
void setLogLevel(LOG_LEVEL newLevel);

char *levelDescription(LOG_LEVEL level);

#define log(level, fmt, ...)                                                             \
	{                                                                                    \
		if (level >= current_level)                                                      \
		{                                                                                \
			fprintf(stderr, "%s: %s:%d, ", levelDescription(level), __FILE__, __LINE__); \
			fprintf(stderr, fmt, ##__VA_ARGS__);                                         \
			fprintf(stderr, "\n");                                                       \
		}                                                                                \
		if (level == FATAL)                                                              \
			exit(1);                                                                     \
	}

#endif