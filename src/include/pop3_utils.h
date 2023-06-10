#ifndef POP_UTILS_H_
#define POP_UTILS_H_

#include <stdbool.h>
#include "queue.h"
#include "buffer.h"
#include "parser.h"
#include "server.h"


#define COMMAND_COUNT 10
#define BUFFER_SIZE 1024
#define OK_MSG "+OK "
#define GREETING_MSG "POP3 preparado <pampero.itba.edu.ar>"
#define SEPARATOR ".\r\n"

#define AUTH_PRE_USER 0x01
#define AUTH_POST_USER 0x02
#define TRANSACTION 0x04
#define UPDATE 0x08

#define MAX_LISTING_SIZE 64
#define INITIAL_LISTING_COUNT 10

typedef uint8_t state_t;
typedef enum {INVALID,NOOP,USER,PASS,QUIT,STAT,LIST,RETR,DELE,RSET,CAPA,GREETING} command_type_t;
typedef struct command_t command_t;
typedef struct pop3_client pop3_client;
typedef command_t* (*command_handler)(command_t *, buffer_t, pop3_client *);
struct command_t {
    command_t* (*command_handler)(command_t* command_state, buffer_t buffer, pop3_client* new_state);
    //FILE* file;
    command_type_t type;
    char* answer;
    bool answer_alloc;
    unsigned int index;
    char* args[2];
};

typedef struct command_info
{
    char* name;
    command_t* (*command_handler)(command_t* command_state, buffer_t buffer, pop3_client* new_state);
    command_type_t type;
    state_t valid_states;
} command_info;

typedef struct email_metadata_t {
    char* filename;
    size_t octets;
    bool deleted;
} email_metadata_t;

typedef struct pop3_client {
    state_t current_state;
    command_t* pending_command;
    struct parser* parser_state;
    user_t* selected_user; // Ptero a struct user en la lista, no hacer free
    email_metadata_t* emails;
    size_t emails_count;
    bool closing;
} pop3_client;


// devuelve -1 si hubo un error, loguea el error solo
int handle_pop3_client(void *index, bool can_read, bool can_write);
int accept_pop3_connection(void *index, bool can_read, bool can_write);
void free_client(int index);

email_metadata_t* get_email_at_index(pop3_client* state, size_t index);

void log_emails(email_metadata_t *emails, size_t c);
#endif