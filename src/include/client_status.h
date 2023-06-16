#ifndef CLIENT_STATUS_H
#define CLIENT_STATUS_H

#include <stdbool.h>

#include "buffer.h"
#include "parser.h"


typedef union client_info_t {
    struct pop3_client* pop3_client_info;
    struct peep_client* peep_client_info;
} client_info_t;

typedef struct socket_handler
{
    int fd;
    int (*handler)(void * state, bool can_read, bool can_write);
    bool occupied;
    bool try_read;
    bool try_write;
    client_info_t client_info;
    buffer_t writing_buffer;
    void (*free_client)(int socket_index);
} socket_handler;

#endif