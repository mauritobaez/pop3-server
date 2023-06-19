#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#define MAX_COMMAND_LENGTH 268
#define STD_BUFFER_SIZE 512
#define FATAL_ERROR(fmt, ...)                                                            \
	{                                                                                    \
        fprintf(stderr, fmt, __VA_ARGS__);                                         	     \
        fprintf(stderr, "\n");                                                           \
        exit(1);                                                                         \
	}


typedef enum config_t {ADDRESS = 0, PORT, USERNAME, PASSWORD, CONFIG_COUNT} config_t;

typedef struct client_config {
    char* values[CONFIG_COUNT];
} client_config;

typedef struct argument_t {
    char *argument;
    config_t config;
} argument_t;

client_config get_client_config (int argc, char* argv[]);
int setup_tcp_client_socket(const char *host,const char *port);
int handle_server_response(FILE* server_fd, bool last_command_was_multiline);

#endif