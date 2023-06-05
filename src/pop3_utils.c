#include "pop3_utils.h"

#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "logger.h"
#include "socket_utils.h"
#include "queue.h"

command_t* handle_noop(command_t* command_state, buffer_t);

command_info commands[COMMAND_COUNT] = {
    {.name = "NOOP", .command_handler = (command_t* (*)(command_t *, buffer_t)) & handle_noop}
};

int handle_pop3_client(void *index, bool can_read, bool can_write) {
    int i = *(int*) index;
    socket_handler* socket = &sockets[i];
    if(can_write) {
        size_t nbytes = buffer_available_chars_count(socket->writing_buffer);
        char* message = malloc(nbytes+1);
        size_t read_bytes = buffer_read(socket->writing_buffer, message, nbytes); 
        ssize_t sent_bytes = send(socket->fd, message, read_bytes, 0);
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
            }
        }
    }
    if(can_read) {
        char* message = malloc(512);
        ssize_t received_bytes = recv(socket->fd, message, 512, 0);


        /*
            ir leyendo de la entrada con recv y mandarselo al parser
            preguntarle al parser qué comandos enteros hay
            foreach(comando de los eventos del parser) {
                switch(strmcp(comando, coso))
                    comando_handler();

            }
        */

        command_t* command_state = handle_noop(NULL, socket->writing_buffer);
        if(command_state != NULL) {
            enqueue(socket->pop3_client_info->pending_commands, (void*) command_state);
        }
        socket->try_write = command_state == NULL || command_state->index > 0;
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
        command = command_state;
    }

    size_t length = strlen(command->answer) - command->index;
    size_t written_bytes = buffer_write_and_advance(buffer, command->answer, length);
    if(written_bytes >= length) {
        return NULL;
    }
    command->index = written_bytes;
    return command;
}