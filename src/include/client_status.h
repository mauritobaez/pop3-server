#ifndef CLIENT_STATUS_H
#define CLIENT_STATUS_H

#include <stdbool.h>

#include "buffer.h"
#include "parser.h"
#include "pop3_utils.h"

typedef enum blocking_state
{
    READING,
    WRITING,
    PROCEED
} blocking_state;

typedef struct socket_handler
{
    int fd;
    int (*handler)(void * state, bool can_read, bool can_write);
    bool occupied;
    bool try_read;
    bool try_write;
    pop3_client* pop3_client_info;
    buffer_t writing_buffer;
} socket_handler;

#endif