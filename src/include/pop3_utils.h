#ifndef POP_UTILS_H_
#define POP_UTILS_H_

#include <stdio.h>
#include <stdbool.h>
#include "queue.h"
#include "buffer.h"
#include "parser.h"
#include "server.h"
#include "command_utils.h"

#define POP3_WRITING_BUFFER_SIZE 4096
#define COMMAND_COUNT 10
#define MAX_LINE 4096

//values for state_t
#define AUTH_PRE_USER 0x01
#define AUTH_POST_USER 0x02
#define TRANSACTION 0x04
#define UPDATE 0x08

#define MAX_LISTING_SIZE 64
#define INITIAL_LISTING_COUNT 10

//values for command_type_t
#define INVALID 0
#define NOOP 1
#define USER 2
#define PASS 3
#define QUIT 4
#define STAT 5
#define LIST 6
#define RETR 7
#define DELE 8
#define RSET 9
#define CAPA 10
#define GREETING 11 

#define RETR_STATE(command) ((retr_state_t*)((command)->meta_data))

typedef struct pop3_client pop3_client;

typedef struct retr_state_t {
    int emailfd; // email file descriptor
    int multiline_state; // buffer stuffing detector
    bool finished_line; // finished writing 512 line
    bool greeting_done; // greeting done
    bool final_dot;
    FILE* email_stream; // email file stream (to pclose)
} retr_state_t;

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
    char* user_maildir;
    size_t emails_count;
    bool closing;
} pop3_client;

// devuelve -1 si hubo un error, loguea el error solo
int handle_pop3_client(void *index, bool can_read, bool can_write);
int accept_pop3_connection(void *index, bool can_read, bool can_write);
void free_client_pop3(int index);

email_metadata_t* get_email_at_index(pop3_client* state, size_t index);

void log_emails(email_metadata_t *emails, size_t c);
#endif
