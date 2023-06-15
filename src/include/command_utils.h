#ifndef COMMAND_UTILS_H_
#define COMMAND_UTILS_H_

#include <stdbool.h>
#include "buffer.h"
#include "client_status.h"

typedef uint8_t state_t;
typedef uint8_t command_type_t;

typedef struct command_t command_t;
typedef command_t* (*command_handler)(command_t *, buffer_t, client_info_t *);

struct command_t {
    command_t* (*command_handler)(command_t* command_state, buffer_t buffer, client_info_t* new_state);
    command_type_t type;
    char* answer;
    bool answer_alloc;
    unsigned int index;
    char* args[2];
    void* meta_data;
};

typedef struct command_info
{
    char* name;
    command_t* (*command_handler)(command_t* command_state, buffer_t buffer, client_info_t* new_state);
    command_type_t type;
    state_t valid_states;
} command_info;

command_t *handle_simple_command(command_t *command_state, buffer_t buffer, char *answer);
void free_command(command_t *command);

#endif