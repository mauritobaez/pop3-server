#ifndef CLIENT_STATUS_H
#define CLIENT_STATUS_H

#include "buffer.h"
#include "parser.h"

typedef enum {
    READING,
    WRITING,
    PROCEED
} state;

typedef enum {
    AUTHORIZATION,
    TRANSACTION,
    UPDATE
} pop3_state;


typedef struct pop3_client {
    state blocking_state;
    pop3_state pop3_state;
    struct parser *input;
    buffer* write_buffer;
} pop3_client;

typedef struct socket_handler {
    int socket;
    void *data;
    int (*handler)(void *);
} socket_handler;


#endif