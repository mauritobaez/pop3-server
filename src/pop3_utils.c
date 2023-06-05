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


command_t* handle_noop(command_t* command_state, buffer_t);

command_info commands[COMMAND_COUNT] = {
    {.name = "NOOP", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = NOOP},
    {.name = "USER", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop, .type = USER},
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
        size_t nbytes = buffer_available_chars_count(socket->writing_buffer);
        char* message = malloc(nbytes+1);
        size_t read_bytes = buffer_read(socket->writing_buffer, message, nbytes); 
        ssize_t sent_bytes = send(socket->fd, message, read_bytes, 0);
        //TODO:ERROR HANDLING
        free(message);
        buffer_advance_read(socket->writing_buffer, sent_bytes);
        
        if(buffer_available_chars_count(socket->writing_buffer) == 0) {
            socket->try_write = false;
        }
        
        while(!is_empty(socket->pop3_client_info->pending_commands)) {
            command_t* pending_command = dequeue(socket->pop3_client_info->pending_commands);
            command_t* new_pending_command = pending_command->command_handler(pending_command, socket->writing_buffer);
            socket->try_write = new_pending_command == NULL || (pending_command->index < new_pending_command->index && pending_command->type == new_pending_command->type);
            free(pending_command);

            if(new_pending_command != NULL) {
                push(socket->pop3_client_info->pending_commands, (void*) new_pending_command);
                break; //salgo del while porque si no terminó es porque se llenó el buffer devuelta
            }else{
                for(int i = 1 ; i < 3 ; i += 1){
                    free(pending_command->args[i]);
                }
                free(new_pending_command);
            }
        }
    }
    if(can_read) {
        char* message = malloc(sizeof(char)* 512);
        ssize_t received_bytes = recv(socket->fd, message, 512, 0);
        //TODO: ERROR HANDLING
        log(DEBUG,"Received %d bytes from socket %d", received_bytes, socket->fd);
        
        struct parser* parser = socket->pop3_client_info->parser_state;
    
        for(int i = 0;  i < received_bytes ; i += 1) {
            if(parser_feed(parser, message[i])->finished) {
                finish_event_item(parser);
            }
        }
        free(message);
        struct parser_event* event = get_event_list(parser);
        bool found = false;
        while(event != NULL) {
            for(int i = 0; i < COMMAND_COUNT && !found; i += 1) {
                str_to_upper(event->args[0]);
                if(strcmp(event->args[0], commands[i].name) == 0) {
                    found = true;
                    command_t* command_state = malloc(sizeof(command_t)); //TODO: ver si esta bien usar malloc aca
                    command_state->type = commands[i].type;
                    command_state->command_handler = commands[i].command_handler;
                    command_state->args[0] = event->args[1];
                    command_state->args[1] = event->args[2];
                    command_state->answer = "+OK\r\n";
                    command_state->index = 0;
                    log(DEBUG, "Command %s received with args %s and %s", event->args[0],event->args[1],event->args[2]);
                    command_t* command_to_enque = command_state->command_handler(command_state, socket->writing_buffer);
                    if(command_to_enque != NULL) {
                        enqueue(socket->pop3_client_info->pending_commands, (void*) command_to_enque);
                    }else{
                        for(int i = 1; i < 3; i += 1) {
                            if(event->args[i] != NULL){
                                free(event->args[i]);
                            }
                        }
                    }
                    socket->try_write = command_to_enque == NULL || command_to_enque->index > 0;
                    free(event->args[0]);
                    free(command_state);
                }
            }
            if(!found){
                 //TODO: not a valid command
                    for(int i = 0; i < 3; i += 1) {
                        if(event->args[i] != NULL){
                            free(event->args[i]);
                        }
                    } 
            }
            event = event->next;
        }
        free_event_list(parser);

        /*
            ir leyendo de la entrada con recv y mandarselo al parser
            preguntarle al parser qué comandos enteros hay
            foreach(comando de los eventos del parser) {
                switch(strmcp(comando, coso))
                    comando_handler();

            }
        */
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
                sockets[i].pop3_client_info->pending_commands = create_queue();
                sockets[i].pop3_client_info->parser_state = set_up_parser();
                sockets[i].writing_buffer = buffer_init(BUFFER_SIZE);
                    
                current_socket_count += 1;
                return 0;
            }
        }
        
    }
    return -1;
}

command_t* handle_noop(command_t* command_state, buffer_t buffer) {
    command_t* command = malloc(sizeof(command_t));
    if(command_state == NULL) {
        command->answer = "+OK\r\n";
        command->command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop;
        command->type = NOOP;
        command->index = 0;
    } else {
        command->answer = command_state->answer;
        command->command_handler = command_state->command_handler;
        command->type = command_state->type;
        command->index = command_state->index;
    }
    log(DEBUG, "Writing %s to socket", command->answer);
    size_t length = strlen(command->answer) - command->index;
    size_t written_bytes = buffer_write_and_advance(buffer, command->answer, length);
    if(written_bytes >= length) {
        return NULL;
    }
    command->index = written_bytes;
    return command;
}