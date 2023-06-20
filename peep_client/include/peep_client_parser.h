#ifndef PEEP_CLIENT_PARSER_H
#define PEEP_CLIENT_PARSER_H

#include <ctype.h>
#include <stdbool.h>

typedef enum client_commands {
    AUTHENTICATE,                      //a  auth
    QUIT,                              //q  quit|
    ADD_USER,                          //u+ add user|
    DELETE_USER,                       //u- delete user|
    SHOW_USERS,                        //u? users|
    SET_MAX_CONNECTIONS,               //c= set max connections| REPAIR
    SHOW_MAX_CONNECTIONS,              //c? max connections|
    SET_MAILDIR,                       //m= set maildir|
    SHOW_MAILDIR,                      //m? maildir|
    SET_TIMEOUT,                       //t= set timeout|
    SHOW_TIMEOUT,                      //t? timeout|
    SHOW_RETRIEVED_BYTES,              //rb? retrived bytes|
    SHOW_RETRIEVED_EMAILS_COUNT,       //re? retrived emails|
    SHOW_REMOVED_EMAILS_COUNT,         //xe? removed emails amount|
    SHOW_CURR_CONNECTION_COUNT,        //cc? current connections amount|
    SHOW_CURR_LOGGED_IN,               //cu? current logged amount|
    SHOW_HIST_CONNECTION_COUNT,        //hc? all time connection amount|
    SHOW_HIST_LOGGED_IN_COUNT,         //hu all time logged amount|
    CAPABILITIES,                      //cap? list capabilities|
    UNKNOWN,
    HELP                       
} client_commands;

typedef struct {
    client_commands type;
    char* str_args[2];
} command_info;
#define IS_MULTILINE(command_type) ( (command_type) == SHOW_USERS || (command_type) == SHOW_CURR_LOGGED_IN || (command_type) == CAPABILITIES )
#define MAX_WORDS_COMMAND 4

command_info* parse_user_command(char* command);

#endif