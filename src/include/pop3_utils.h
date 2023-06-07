#ifndef POP_UTILS_H_
#define POP_UTILS_H_

#include <stdbool.h>
#include "queue.h"
#include "buffer.h"
#include "parser.h"

#define COMMAND_COUNT 10
#define BUFFER_SIZE 1024
#define OK_MSG "+OK"
#define GREETING_MSG "POP3 preparado perra <pampero.itba.edu.ar>"

#define AUTH_PRE_USER 0x01
#define AUTH_POST_USER 0x02
#define TRANSACTION 0x04
#define UPDATE 0x08

typedef uint8_t state_t;
typedef struct command_t command_t;
typedef command_t* (*command_handler)(command_t *, buffer_t, state_t *);


typedef enum {INVALID,NOOP,USER,PASS,QUIT,STAT,LIST,RETR,DELE,RSET,CAPA,GREETING} command_type_t;

struct command_t {
    command_t* (*command_handler)(command_t* command_state, buffer_t buffer, state_t* new_state);
    //FILE* file;
    command_type_t type;
    char* answer;
    unsigned int index;
    char* args[2];
};

typedef struct command_info
{
    char* name;
    command_t* (*command_handler)(command_t* command_state, buffer_t buffer, state_t* new_state);
    command_type_t type;
    state_t valid_states;
} command_info;


typedef struct pop3_client {
    state_t current_state;
    command_t* pending_command;
    struct parser* parser_state;
} pop3_client;


// devuelve -1 si hubo un error, loguea el error solo
int handle_pop3_client(void *index, bool can_read, bool can_write);
int accept_pop3_connection(void *index, bool can_read, bool can_write);

#endif