#ifndef POP_UTILS_H_
#define POP_UTILS_H_

#include <stdbool.h>
#include "queue.h"
#include "buffer.h"

#define COMMAND_COUNT 1
#define BUFFER_SIZE 1024

typedef struct command_t command_t;

typedef enum {NOOP} command_type_t;

struct command_t {
    command_t* (*command_handler)(command_t* command_state, buffer_t buffer);
    //FILE* file;
    command_type_t type;
    char* answer;
    unsigned int index;
};

typedef struct command_info
{
    char* name;
    command_t* (*command_handler)(command_t* command_state, buffer_t buffer);
    
} command_info;

typedef enum
{
    AUTHORIZATION,
    TRANSACTION,
    UPDATE
} pop3_state;

typedef struct pop3_client {
    pop3_state current_state;
    queue_t pending_commands;
} pop3_client;



int handle_pop3_client(void *index, bool can_read, bool can_write);
int accept_pop3_connection(void *index, bool can_read, bool can_write);

#endif