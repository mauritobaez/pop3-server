#ifndef CLIENT_STATUS_H
#define CLIENT_STATUS_H

#include "buffer.h"
#include "parser.h"

typedef enum blocking_state
{
    READING,
    WRITING,
    PROCEED
} blocking_state;

typedef enum
{
    AUTHORIZATION,
    TRANSACTION,
    UPDATE
} pop3_state;

typedef struct pop3_client
{
    blocking_state blocking_state;
    pop3_state pop3_state;
    struct parser *input;
    buffer *write_buffer;
    int num_read;
    int num_toread;
    buffer *read_buffer;
    int num_write;
    int num_towrite;
} pop3_client;

typedef struct socket_handler
{
    int fd;
    void *data;
    int (*handler)(void *);
    int occupied;
} socket_handler;

#endif