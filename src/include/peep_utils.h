#ifndef PEEP_UTILS_H
#define PEEP_UTILS_H

#include "command_utils.h"
#include "client_status.h"

#define MAX_COMMAND_LENGTH 268
#define PEEP_WRITING_BUFFER_SIZE 512

//values for state_t
#define AUTHENTICATION 0x01
#define AUTHENTICATED 0x02

#define PEEP_COMMAND_COUNT 19

// values for command_type_t
#define C_AUTHENTICATE                  0     //a
#define C_QUIT                          1     //q
#define C_ADD_USER                      2     //u+
#define C_DELETE_USER                   3     //u-
#define C_SHOW_USERS                    4     //u?
#define C_SET_MAX_CONNECTIONS           5     //c=
#define C_SHOW_MAX_CONNECTIONS          6     //c?
#define C_SET_MAILDIR                   7     //m=
#define C_SHOW_MAILDIR                  8     //m?
#define C_SET_TIMEOUT                   9     //t=
#define C_SHOW_TIMEOUT                  10    //t?
#define C_SHOW_RETRIEVED_BYTES          11    //rb?
#define C_SHOW_RETRIEVED_EMAILS_COUNT   12    //re?
#define C_SHOW_REMOVED_EMAILS_COUNT     13    //xe?
#define C_SHOW_CURR_CONNECTION_COUNT    14    //cc?
#define C_SHOW_CURR_LOGGED_IN           15    //cu?
#define C_SHOW_HIST_CONNECTION_COUNT    16    //hc?
#define C_SHOW_HIST_LOGGED_IN_COUNT     17    //hu?
#define C_CAPABILITIES                  18    //cap?
#define C_UNKNOWN                       19


typedef struct peep_client {
    state_t state;
    struct parser* parser_state;
    command_t* pending_command;
    bool closing;
} peep_client;

int handle_peep_client(void *index, bool can_read, bool can_write);
int accept_peep_connection(void *index, bool can_read, bool can_write);
void free_peep_client(int index);

#endif
