#include <sys/socket.h>
#include <stdlib.h>
#include "logger.h"
#include "peep_utils.h"
#include "socket_utils.h"
#include "command_parser.h"
#include "server.h"
#include "peep_responses.h"
#include "queue.h"
#include "directories.h"

#define RETURN_NEGATIVE_RESPONSE(command_state, buffer, error_code) \
    do {\
    char neg_resp[SHORT_RESP_LENGTH] = {0};\
    snprintf(neg_resp, SHORT_RESP_LENGTH, NEG_RESP, (error_code));\
    return handle_simple_command((command_state), (buffer), neg_resp);\
    } while(0)

#define RETURN_POSITIVE_RESPONSE(command_state, buffer) return handle_simple_command((command_state), (buffer), OK)

#define RETURN_POSITIVE_RESPONSE_INTEGER(command_state, buffer, integer) \
    do {\
    char ans[MAX_LINE_LENGTH] = {0};\
    snprintf(ans, MAX_LINE_LENGTH, OK_INT, (integer));\
    return handle_simple_command((command_state), (buffer), ans);\
    } while(0)

command_t *handle_unknown_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_authenticate_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_qquit_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_set_max_connections_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_add_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_delete_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_users_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_max_connections_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_set_maildir_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_maildir_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_set_timeout_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_timeout_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_retrieved_bytes_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_retrieved_emails_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_removed_emails_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_curr_connection_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_curr_logged_in_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_hist_connection_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_hist_logged_in_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_help_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);


command_info peep_commands[PEEP_COMMAND_COUNT] = {
    {.name = "a",   .command_handler = (command_handler)&handle_authenticate_command, .type = C_AUTHENTICATE, .valid_states = AUTHENTICATION},
    {.name = "q",   .command_handler = (command_handler)&handle_qquit_command, .type = C_QUIT, .valid_states = AUTHENTICATION | AUTHENTICATED},
    {.name = "u+",  .command_handler = (command_handler)&handle_add_user_command, .type = C_ADD_USER, .valid_states = AUTHENTICATED},
    {.name = "u-",  .command_handler = (command_handler)&handle_delete_user_command, .type = C_DELETE_USER, .valid_states = AUTHENTICATED},
    {.name = "u?",  .command_handler = (command_handler)&handle_show_users_command, .type = C_SHOW_USERS, .valid_states = AUTHENTICATED},
    {.name = "c=",  .command_handler = (command_handler)&handle_set_max_connections_command, .type = C_SET_MAX_CONNECTIONS, .valid_states = AUTHENTICATED},
    {.name = "c?",  .command_handler = (command_handler)&handle_show_max_connections_command, .type = C_SHOW_MAX_CONNECTIONS, .valid_states = AUTHENTICATED},
    {.name = "m=",  .command_handler = (command_handler)&handle_set_maildir_command, .type = C_SET_MAILDIR, .valid_states = AUTHENTICATED},
    {.name = "m?",  .command_handler = (command_handler)&handle_show_maildir_command, .type = C_SHOW_MAILDIR, .valid_states = AUTHENTICATED},
    {.name = "t=",  .command_handler = (command_handler)&handle_set_timeout_command, .type = C_SET_TIMEOUT, .valid_states = AUTHENTICATED},
    {.name = "t?",  .command_handler = (command_handler)&handle_show_timeout_command, .type = C_SHOW_TIMEOUT, .valid_states = AUTHENTICATED},
    {.name = "rb?", .command_handler = (command_handler)&handle_show_retrieved_bytes_command, .type = C_SHOW_RETRIEVED_BYTES, .valid_states = AUTHENTICATED},
    {.name = "re?", .command_handler = (command_handler)&handle_show_retrieved_emails_count_command, .type = C_SHOW_RETRIEVED_EMAILS_COUNT, .valid_states = AUTHENTICATED},
    {.name = "xe?", .command_handler = (command_handler)&handle_show_removed_emails_count_command, .type = C_SHOW_REMOVED_EMAILS_COUNT, .valid_states = AUTHENTICATED},
    {.name = "cc?", .command_handler = (command_handler)&handle_show_curr_connection_count_command, .type = C_SHOW_CURR_CONNECTION_COUNT, .valid_states = AUTHENTICATED},
    {.name = "cu?", .command_handler = (command_handler)&handle_show_curr_logged_in_command, .type = C_SHOW_CURR_LOGGED_IN, .valid_states = AUTHENTICATED},
    {.name = "hc?", .command_handler = (command_handler)&handle_show_hist_connection_count_command, .type = C_SHOW_HIST_CONNECTION_COUNT, .valid_states = AUTHENTICATED},
    {.name = "hu?", .command_handler = (command_handler)&handle_show_hist_logged_in_count_command, .type = C_SHOW_HIST_LOGGED_IN_COUNT, .valid_states = AUTHENTICATED},
    {.name = "h?",  .command_handler = (command_handler)&handle_help_command, .type = C_HELP, .valid_states = AUTHENTICATION | AUTHENTICATED}
};

uint8_t get_arg_count(command_t* command_state) {
    if(command_state->args[0] == NULL)  return 0;
    if(command_state->args[1] == NULL)  return 1;
    else  return 2;
} 


int handle_peep_client(void *index, bool can_read, bool can_write) {
    int i = *(int *)index;
    socket_handler *socket = &sockets[i];
    peep_client* peep_client_info = socket->client_info.peep_client_info;

    if (can_write)
    {
        int remaining_bytes = send_from_socket_buffer(i);
        if(remaining_bytes == -1)   return -1;
        if(remaining_bytes == -2)   goto close_peep_client;

        if (buffer_available_chars_count(socket->writing_buffer) == 0 && peep_client_info->closing)
        {
            log(INFO, "Socket %d - closing session\n", i);
            goto close_peep_client;
        }
        
        // ahora que hicimos lugar en el buffer, intentamos resolver el comando que haya quedado pendiente
        if (peep_client_info->pending_command != NULL)
        {
            command_t *old_pending_command = peep_client_info->pending_command;
            peep_client_info->pending_command = old_pending_command->command_handler(old_pending_command, socket->writing_buffer, &(socket->client_info));
            socket->try_write = true;
            free(old_pending_command);
        }
    }

    struct parser *parser = peep_client_info->parser_state;

    if (can_read  && !peep_client_info->closing)
    {
        int ans = recv_to_parser(i, peep_client_info->parser_state, MAX_COMMAND_LENGTH);
        if(ans == -1)   LOG_AND_RETURN(ERROR, "Error reading from peep client", -1);
        if(ans == -2)   goto close_peep_client;
    }

    if (peep_client_info->pending_command == NULL && !peep_client_info->closing)
    {
        struct parser_event *event = get_last_event(parser);
        if (event != NULL)
        {
            bool found_command = false;
            if (event->args[0] != NULL)
            {
                for (int i = 0; i < PEEP_COMMAND_COUNT && !found_command; i++)
                {
                    if (((peep_commands[i].valid_states & peep_client_info->state) > 0) && strcmp(event->args[0], peep_commands[i].name) == 0)
                    {
                        found_command = true;
                        command_t command_state = {
                            .type = peep_commands[i].type,
                            .command_handler = peep_commands[i].command_handler,
                            .args[0] = event->args[1],
                            .args[1] = event->args[2],
                            .answer = NULL,
                            .answer_alloc = false,
                            .index = 0,
                            .meta_data = NULL
                        };

                        log(DEBUG, "Command %s received with args %s and %s\n", event->args[0], event->args[1], event->args[2]);

                        peep_client_info->pending_command = command_state.command_handler(&command_state, socket->writing_buffer, &(socket->client_info));
                        socket->try_write = true;
                        free_event(event, false);
                    }
                }
            }
            if (!found_command)
            {
                command_t command_state = {
                    .type = C_UNKNOWN,
                    .command_handler = (command_handler)&handle_unknown_command,
                    .args[0] = NULL,
                    .args[1] = NULL,
                    .answer = NULL,
                    .answer_alloc = false,
                    .index = 0,
                    .meta_data = NULL,
                };
                log(DEBUG, "Unknown command %s received\n", event->args[0]);
                peep_client_info->pending_command = command_state.command_handler(&command_state, socket->writing_buffer, &(socket->client_info));
                socket->try_write = true;
                free_event(event, true);
            }
        }
    }
    return 0;

close_peep_client:
    //TODO:dejamos esto o lo sacamos
    return -1;
}

int accept_peep_connection(void *index, bool can_read, bool can_write) {
    if (can_read)
    {
        int sock_index = *(int *)index;
        socket_handler *socket_state = &sockets[sock_index];

        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        if ((current_socket_count - PASSIVE_SOCKET_COUNT) >= global_config.max_connections)
        {
            return -1;
        }
        int client_socket = accept(socket_state->fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_socket < 0)
        {
            LOG_AND_RETURN(ERROR, "Error accepting peep connection", -1);
        }

        for (int i = 0; i < MAX_SOCKETS; i += 1)
        {
            if (!sockets[i].occupied)
            {
                log(DEBUG, "accepted client socket: %d\n", i);
                sockets[i].fd = client_socket;
                sockets[i].occupied = true;
                sockets[i].handler = (int (*)(void *, bool, bool)) & handle_peep_client;
                sockets[i].try_read = true;
                sockets[i].try_write = false;
                sockets[i].free_client = &free_peep_client;
                sockets[i].client_info.peep_client_info = malloc(sizeof(peep_client));
                sockets[i].client_info.peep_client_info->state = AUTHENTICATION;
                sockets[i].client_info.peep_client_info->pending_command = NULL;
                sockets[i].client_info.peep_client_info->parser_state = set_up_parser();
                sockets[i].client_info.peep_client_info->closing = false;
                sockets[i].writing_buffer = buffer_init(PEEP_WRITING_BUFFER_SIZE);
                sockets[i].last_interaction = 0;
                current_socket_count += 1;
                return 0;
            }
        }
    }
    return -1;
}

void free_peep_client(int index)
{
    peep_client* client = sockets[index].client_info.peep_client_info;
    parser_destroy(client->parser_state);
    free_command(client->pending_command);
    free(client);
}

command_t *handle_unknown_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    RETURN_NEGATIVE_RESPONSE(command_state, buffer, 0);
}

// a
command_t *handle_authenticate_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    if(get_arg_count(command_state) < 2) {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }
    if(strcmp(command_state->args[0], global_config.peep_admin.username) == 0 && strcmp(command_state->args[1], global_config.peep_admin.password) == 0) {
        client_state->peep_client_info->state = AUTHENTICATED;
        RETURN_POSITIVE_RESPONSE(command_state, buffer);
    }
    else {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 2);
    }
}

// q
command_t *handle_qquit_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    client_state->peep_client_info->closing = true;
    RETURN_POSITIVE_RESPONSE(command_state, buffer);
}

// c=
command_t *handle_set_max_connections_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    if(get_arg_count(command_state) < 1) {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }
    int new_max_connections = atoi(command_state->args[0]);
    if(new_max_connections < MIN_CONNECTION_LIMIT || new_max_connections > MAX_CONNECTION_LIMIT) {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 5);
    }
    global_config.max_connections = new_max_connections;
    return handle_simple_command(command_state, buffer, OK);
}


user_t * check_user_exists(char* username) 
{
    queue_t user_list = global_config.users;
    user_t *user;
    if(username!=NULL) {
        iterator_to_begin(user_list);
        while (iterator_has_next(user_list))
        {
            user = iterator_next(user_list);
            if (strcmp(user->username, username) == 0)
                return user;
        }
    }
    return NULL;
}

// u+
command_t *handle_add_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    if(get_arg_count(command_state) <2) {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }
    
    char* username = command_state->args[0];
    if(check_user_exists(username)!=NULL)
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 3);

    char* password = command_state->args[1];
    // TODO: ver que la password tenga sentido?
    user_t* new_user = malloc(sizeof(user_t));
    new_user->username = malloc(strlen(username) + 1);
    strcpy(new_user->username, username);
    new_user->password = malloc(strlen(password) + 1);
    strcpy(new_user->password, password);
    new_user->locked = false;
    new_user->removed = false;
    enqueue(global_config.users, new_user);

    return handle_simple_command(command_state, buffer, OK);
}

// u-
command_t *handle_delete_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    if(get_arg_count(command_state) <1) {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }

    user_t* user;
    if((user = check_user_exists(command_state->args[0]))!=NULL) {
        user->removed=true;
        return handle_simple_command(command_state, buffer, OK);
    }

    RETURN_NEGATIVE_RESPONSE(command_state, buffer, 4);
}

// u?
command_t *handle_show_users_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    queue_t list = global_config.users;
    size_t user_count=0;
    iterator_to_begin(list);
    while (iterator_has_next(list))
    {
        user_t *user = iterator_next(list);
        user_count += !user->removed; 
    }
    char* ans = malloc(MAX_LINE_LENGTH * (user_count+1));
    command_state->answer_alloc = true;
    int index = snprintf(ans, MAX_LINE_LENGTH, OK_INT, user_count);
    iterator_to_begin(list);
    while (iterator_has_next(list))
    {
        user_t *user = iterator_next(list);
        if(!user->removed){
            index += snprintf(ans + index, MAX_LINE_LENGTH, LINE_RESP, user->username);
        }
    }
    return handle_simple_command(command_state, buffer, ans);
}

// c?
command_t *handle_show_max_connections_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    size_t response =  metrics.max_concurrent_pop3_connections;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state,buffer,response);
}

// m=
command_t *handle_set_maildir_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    if(get_arg_count(command_state) < 1) {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }
    if(!path_is_directory(command_state->args[0])) {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 6);
    }
    size_t length = strlen(command_state->args[0]) + 1;
    char* maildir = malloc(length);
    strncpy(maildir, command_state->args[0], length);
    free(global_config.maildir);
    global_config.maildir = maildir;
    return handle_simple_command(command_state, buffer, OK);
}

// m?
command_t *handle_show_maildir_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    char* response = global_config.maildir;
    char ans[MAX_LINE_LENGTH] = {0};
    snprintf(ans, MAX_LINE_LENGTH, OK_STRING, response);
    return handle_simple_command(command_state, buffer, ans);
}

// t=
command_t *handle_set_timeout_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    if(get_arg_count(command_state) < 1) {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }
    int new_timeout = atoi(command_state->args[0]);
    if(new_timeout < MIN_TIMEOUT_VALUE || new_timeout > MAX_TIMEOUT_VALUE) {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 7);
    }
    global_config.timeout = new_timeout;
    return handle_simple_command(command_state, buffer, OK); 
}

// t?
command_t *handle_show_timeout_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    size_t response = global_config.timeout;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state,buffer,response);   
}

// rb?
command_t *handle_show_retrieved_bytes_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    size_t response = metrics.sent_bytes;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state,buffer,response);    
}

// re?
command_t *handle_show_retrieved_emails_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    size_t response = metrics.emails_read;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state,buffer,response);    
}

// xe?
command_t *handle_show_removed_emails_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{   
    size_t response = metrics.emails_removed;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state,buffer,response);     
}

// cc?
command_t *handle_show_curr_connection_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    size_t response = metrics.current_concurrent_pop3_connections;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state,buffer,response);      
}

// cu?
command_t *handle_show_curr_logged_in_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    queue_t list = global_config.users;
    size_t user_count=0;
    iterator_to_begin(list);
    while (iterator_has_next(list))
    {
        user_t *user = iterator_next(list);
        user_count += !user->removed && user->locked; 
    }
    char* ans = malloc(MAX_LINE_LENGTH * (user_count+1));
    command_state->answer_alloc = true;
    int index = snprintf(ans, MAX_LINE_LENGTH, OK_INT, user_count);
    iterator_to_begin(list);
    while (iterator_has_next(list))
    {
        user_t *user = iterator_next(list);
        if(!user->removed && user->locked){
            index += snprintf(ans + index, MAX_LINE_LENGTH, LINE_RESP, user->username);
        }
    }
    return handle_simple_command(command_state, buffer, ans);
}

// hc?
command_t *handle_show_hist_connection_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    size_t response = metrics.total_pop3_connections;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state,buffer,response);       
}

// hu?
command_t *handle_show_hist_logged_in_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    size_t response = metrics.historic_loggedin_users;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state,buffer,response); 
}

// h?
command_t *handle_help_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    return handle_simple_command(command_state, buffer, PEEP_HELP);    
}
