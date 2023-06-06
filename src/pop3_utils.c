#include "pop3_utils.h"

#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "logger.h"
#include "socket_utils.h"
#include "queue.h"
#include "command_parser.h"
#include "util.h"


command_t* handle_noop(command_t* command_state, buffer_t buffer);
command_t* handle_user_command(command_t* command_state, buffer_t buffer);
command_t* handle_invalid_command(command_t* command_state, buffer_t buffer);

void free_command (command_t* command);
void free_event (struct parser_event* event, bool free_arguments);

command_info commands[COMMAND_COUNT] = {
    {.name = "NOOP", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = NOOP},
    {.name = "USER", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_user_command, .type = USER},
    {.name = "PASS", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = PASS},
    {.name = "QUIT", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = QUIT},
    {.name = "STAT", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = STAT},
    {.name = "LIST", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = LIST},
    {.name = "RETR", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = RETR},
    {.name = "DELE", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = DELE},
    {.name = "RSET", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = RSET},
    {.name = "CAPA", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = CAPA}
};

int handle_pop3_client(void *index, bool can_read, bool can_write) {
    int i = *(int*) index;
    socket_handler* socket = &sockets[i];

    if(can_write) {
        size_t bytes_to_read = buffer_available_chars_count(socket->writing_buffer);
        char* message = malloc(bytes_to_read + 1);
        if(message == NULL) return -1;

        size_t read_bytes = buffer_read(socket->writing_buffer, message, bytes_to_read); 
        ssize_t sent_bytes = send(socket->fd, message, read_bytes, 0);
        free(message);
        if(sent_bytes < 0) return -1;
        
        buffer_advance_read(socket->writing_buffer, sent_bytes);
        
        //si ya vaciamos todo el buffer de salida, le decimos al selector que no nos despierte para escribir
        if(buffer_available_chars_count(socket->writing_buffer) == 0) {
            socket->try_write = false;
        }
        
        //ahora que hicimos lugar en el buffer, intentamos resolver el comando que haya quedado pendiente
        if(socket->pop3_client_info->pending_command != NULL) {
            command_t* old_pending_command = socket->pop3_client_info->pending_command;
            socket->pop3_client_info->pending_command = old_pending_command->command_handler(old_pending_command, socket->writing_buffer);
            socket->try_write = true;
            free(old_pending_command);
        }
    }

    struct parser* parser = socket->pop3_client_info->parser_state;

    if(can_read) {
        char* message = malloc(sizeof(char)* 512);
        if(message == NULL) return -1;
        ssize_t received_bytes = recv(socket->fd, message, 512, 0);
        if(received_bytes < 0) {
            free(message);
            return -1;
        }
        
        log(DEBUG,"Received %ld bytes from socket %d", received_bytes, socket->fd);
        
    
        for(int i = 0;  i < received_bytes ; i += 1) {
            if(parser_feed(parser, message[i])->finished) {
                finish_event_item(parser);
            }
        }
        free(message);
    }

    if(socket->pop3_client_info->pending_command == NULL) {
       struct parser_event* event = get_last_event(parser);
        if(event != NULL) {
            bool found_command = false;
            str_to_upper(event->args[0]);
            for(int i = 0; i < COMMAND_COUNT && !found_command; i++) {
                if(strcmp(event->args[0], commands[i].name) == 0) {
                    found_command = true;
                    command_t command_state = {
                        .type = commands[i].type, 
                        .command_handler = commands[i].command_handler, 
                        .args[0] = event->args[1],
                        .args[1] = event->args[2],
                        .answer = NULL,
                        .index = 0
                        };
                
                    log(DEBUG, "Command %s received with args %s and %s\n", event->args[0], event->args[1], event->args[2]);

                    socket->pop3_client_info->pending_command = command_state.command_handler(&command_state, socket->writing_buffer);
                    socket->try_write = true;
                    free_event(event, false);
                }
            }
            if(!found_command) {
                command_t command_state = {
                        .type = INVALID, 
                        .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_invalid_command, 
                        .args[0] = NULL,
                        .args[1] = NULL,
                        .answer = NULL,
                        .index = 0
                        };
                log(DEBUG, "Invalid command %s received\n", event->args[0]);
                socket->pop3_client_info->pending_command = command_state.command_handler(&command_state, socket->writing_buffer);
                socket->try_write = true;
                free_event(event, true);
            }
        }         
    }
    return 0;
}

int accept_pop3_connection(void *index, bool can_read, bool can_write)
{
    if (can_read) {
        int sock_index = *(int *)index;
        socket_handler *socket_state = &sockets[sock_index];

        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        if (current_socket_count == MAX_SOCKETS) {
            return -1;
        }
        int client_socket = accept(socket_state->fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_socket < 0)
        {
            log(ERROR, "accept failed %s\n", strerror(errno));
            return -1;
        }

        // todo: hacerlo su porpia funcion
        for (int i = 0; i < MAX_SOCKETS; i += 1)
        {
            if (!sockets[i].occupied)
            {
                log(DEBUG, "accepted client socket: %d\n", client_socket);
                sockets[i].fd = client_socket;
                sockets[i].occupied = true;
                sockets[i].handler = (int (*)(void *, bool, bool)) & handle_pop3_client;
                sockets[i].try_write = false;
                sockets[i].try_read = true;
                sockets[i].pop3_client_info = malloc(sizeof(pop3_client));
                sockets[i].pop3_client_info->current_state = AUTHORIZATION;
                sockets[i].pop3_client_info->pending_command = NULL;
                sockets[i].pop3_client_info->parser_state = set_up_parser();
                sockets[i].writing_buffer = buffer_init(BUFFER_SIZE);
                    
                current_socket_count += 1;
                return 0;
            }
        }
        
    }
    return -1;
}

command_t* handle_simple_command(command_t* command_state, buffer_t buffer, char* answer) {
    command_t* command = malloc(sizeof(command_t));
    if(command_state->answer == NULL) {
        if(answer == NULL) {
            log(FATAL, "Unexpected error \n%s", "");
        }
        command->answer = answer;
    } else {
        command->answer = command_state->answer;
    }
    command->command_handler = command_state->command_handler;
    command->type = command_state->type;
    command->index = command_state->index;
    command->args[0] = command_state->args[0];
    command->args[1] = command_state->args[1];
    log(DEBUG, "Writing %s to socket", command->answer + command->index);

    size_t remaining_bytes_to_write = strlen(command->answer) - command->index;
    size_t written_bytes = buffer_write_and_advance(buffer, command->answer + command->index, remaining_bytes_to_write);
    if(written_bytes >= remaining_bytes_to_write) {
        free_command(command);
        return NULL;
    }
    command->index += written_bytes;
    return command;
}

command_t* handle_user_command(command_t* command_state, buffer_t buffer) {
    char* answer = NULL;
    if(command_state->answer == NULL) {
        /* lógica de USER para saber cual es answer */
        answer = "HOLA MANOLA :)\n";
    }
    return handle_simple_command(command_state, buffer, answer);
}

command_t* handle_invalid_command(command_t* command_state, buffer_t buffer) {
    return handle_simple_command(command_state, buffer, "-ERR invalid command\r\n");
}

command_t* handle_noop(command_t* command_state, buffer_t buffer) {
    return handle_simple_command(command_state, buffer, "+OK\r\n");
}

void free_command (command_t* command) {
    if(command == NULL)
        return;
    if(command->args[0] != NULL)
        free(command->args[0]);
    if(command->args[1] != NULL)
        free(command->args[1]);
    free(command);
}

void free_event (struct parser_event* event, bool free_arguments) {
    if(free_arguments) {
        if(event->args[1] != NULL)
            free(event->args[1]);
        if(event->args[2] != NULL)
            free(event->args[2]);
    }
    free(event->args[0]);
    free(event);
}