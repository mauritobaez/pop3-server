#include <sys/socket.h>
#include "logger.h"
#include "peep_utils.h"
#include "socket_utils.h"
#include "command_parser.h"
#include "server.h"


command_t *example(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_unknown_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_authenticate_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_quit_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_show_max_connections_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
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
    {.name = "a",   .command_handler = (command_handler)&handle_a_command, .type = C_AUTHENTICATE, .valid_states = AUTHENTICATION},
    {.name = "q",   .command_handler = (command_handler)&example, .type = C_QUIT, .valid_states = AUTHENTICATION | AUTHENTICATED},
    {.name = "u+",  .command_handler = (command_handler)&example, .type = C_ADD_USER, .valid_states = AUTHENTICATED},
    {.name = "u-",  .command_handler = (command_handler)&example, .type = C_DELETE_USER, .valid_states = AUTHENTICATED},
    {.name = "u?",  .command_handler = (command_handler)&example, .type = C_SHOW_USERS, .valid_states = AUTHENTICATED},
    {.name = "c=",  .command_handler = (command_handler)&example, .type = C_SET_MAX_CONNECTIONS, .valid_states = AUTHENTICATED},
    {.name = "c?",  .command_handler = (command_handler)&example, .type = C_SHOW_MAX_CONNECTIONS, .valid_states = AUTHENTICATED},
    {.name = "m=",  .command_handler = (command_handler)&example, .type = C_SET_MAILDIR, .valid_states = AUTHENTICATED},
    {.name = "m?",  .command_handler = (command_handler)&example, .type = C_SHOW_MAILDIR, .valid_states = AUTHENTICATED},
    {.name = "t=",  .command_handler = (command_handler)&example, .type = C_SET_TIMEOUT, .valid_states = AUTHENTICATED},
    {.name = "t?",  .command_handler = (command_handler)&example, .type = C_SHOW_TIMEOUT, .valid_states = AUTHENTICATION},
    {.name = "rb?", .command_handler = (command_handler)&example, .type = C_SHOW_RETRIEVED_BYTES, .valid_states = AUTHENTICATION | AUTHENTICATED},
    {.name = "re?", .command_handler = (command_handler)&example, .type = C_SHOW_RETRIEVED_EMAILS_COUNT, .valid_states = AUTHENTICATED},
    {.name = "xe?", .command_handler = (command_handler)&example, .type = C_SHOW_REMOVED_EMAILS_COUNT, .valid_states = AUTHENTICATED},
    {.name = "cc?", .command_handler = (command_handler)&example, .type = C_SHOW_CURR_CONNECTION_COUNT, .valid_states = AUTHENTICATED},
    {.name = "cu?", .command_handler = (command_handler)&example, .type = C_SHOW_CURR_LOGGED_IN, .valid_states = AUTHENTICATED},
    {.name = "hc?", .command_handler = (command_handler)&example, .type = C_SHOW_HIST_CONNECTION_COUNT, .valid_states = AUTHENTICATED},
    {.name = "hu?", .command_handler = (command_handler)&example, .type = C_SHOW_HIST_LOGGED_IN_COUNT, .valid_states = AUTHENTICATED},
    {.name = "h?",  .command_handler = (command_handler)&example, .type = C_HELP, .valid_states = AUTHENTICATED | AUTHENTICATED}
};


int handle_peep_client(void *index, bool can_read, bool can_write) {
    int i = *(int *)index;
    socket_handler *socket = &sockets[i];
    peep_client* peep_client_info = socket->client_info.peep_client_info;

    if (can_write)
    {
        int remaining_bytes = send_from_socket_buffer(i);
        if(remaining_bytes == -1)   return -1;
        if(remaining_bytes == -2)   goto close_peep_client;
        
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

    if (can_read)
    {
        int ans = recv_to_parser(i, peep_client_info->parser_state, MAX_COMMAND_LENGTH);
        if(ans == -1)   LOG_AND_RETURN(ERROR, "Error reading from peep client", -1);
        if(ans == -2)   goto close_peep_client;
    }

    if (peep_client_info->pending_command == NULL)
    {
        struct parser_event *event = get_last_event(parser);
        if (event != NULL)
        {
            bool found_command = false;
            if (event->args[0] != NULL)
            {
                str_to_upper(event->args[0]);
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
    free_peep_client(i);
    return -1;
}

int accept_peep_connection(void *index, bool can_read, bool can_write) {
    if (can_read)
    {
        int sock_index = *(int *)index;
        socket_handler *socket_state = &sockets[sock_index];

        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        if (current_socket_count == MAX_SOCKETS)
        {
            return -1;
        }
        int client_socket = accept(socket_state->fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_socket < 0)
        {
            LOG_AND_RETURN(ERROR, "Error accepting pop3 connection", -1);
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
                sockets[i].client_info.peep_client_info = calloc(1, sizeof(peep_client));
                sockets[i].client_info.peep_client_info->state = AUTHENTICATION;
                sockets[i].client_info.peep_client_info->pending_command = NULL;
                sockets[i].client_info.peep_client_info->parser_state = set_up_parser();
                sockets[i].writing_buffer = buffer_init(PEEP_WRITING_BUFFER_SIZE);
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
    if(client->pending_command != NULL) free(client->pending_command);
    free(client);
}

command_t *example(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    return handle_simple_command(command_state, buffer, "+");
}

command_t *handle_unknown_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    return handle_simple_command(command_state, buffer, "+");
}


command_t *handle_authenticate_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{

}


command_t *handle_quit_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{

}


command_t *handle_show_max_connections_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{

}


command_t *handle_add_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{

}


command_t *handle_delete_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{

}


command_t *handle_show_users_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{

}

command_t *handle_show_max_connections_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{

}

command_t *handle_set_maildir_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}

command_t *handle_show_maildir_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}
command_t *handle_set_timeout_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}

command_t *handle_show_timeout_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}
command_t *handle_show_retrieved_bytes_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}
command_t *handle_show_retrieved_emails_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}

command_t *handle_show_removed_emails_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}

command_t *handle_show_curr_connection_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}
command_t *handle_show_curr_logged_in_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}
command_t *handle_show_hist_connection_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}
command_t *handle_show_hist_logged_in_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}
command_t *handle_help_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) 
{
    
}
