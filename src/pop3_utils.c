#include "pop3_utils.h"

#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "directories.h"
#include "logger.h"
#include "socket_utils.h"
#include "queue.h"
#include "command_parser.h"
#include "util.h"
#include "server.h"
#include "responses.h"

#define LOG_AND_RETURN(error_level, error_message, return_value) do {\
    log((error_level), "%s - %s\n", (error_message), strerror(errno));\
    return (return_value);\
} while (0)


command_t* handle_noop(command_t* command_state, buffer_t buffer, pop3_client* client_state);
command_t* handle_user_command(command_t* command_state, buffer_t buffer, pop3_client* client_state);
command_t* handle_pass_command(command_t* command_state, buffer_t buffer, pop3_client* client_state);
command_t* handle_invalid_command(command_t* command_state, buffer_t buffer, pop3_client* client_state);
command_t* handle_greeting_command(command_t* command_state, buffer_t buffer, pop3_client* client_state);
command_t* handle_quit_command(command_t* command_state, buffer_t buffer, pop3_client* client_state);
command_t* handle_stat_command(command_t* command_state, buffer_t buffer, pop3_client* client_state);
command_t* handle_list_command(command_t* command_state, buffer_t buffer, pop3_client* client_state);
command_t* handle_retr_command(command_t* command_state, buffer_t buffer, pop3_client* client_state);

void free_command (command_t* command);
void free_event (struct parser_event* event, bool free_arguments);
void free_pop3_client(pop3_client* client);

server_config global_config;

command_info commands[COMMAND_COUNT] = {
    {.name = "NOOP", .command_handler = (command_handler) & handle_noop,          .type = NOOP,   .valid_states = TRANSACTION},
    {.name = "USER", .command_handler = (command_handler) & handle_user_command,  .type = USER,   .valid_states = AUTH_PRE_USER},
    {.name = "PASS", .command_handler = (command_handler) & handle_pass_command,  .type = PASS,   .valid_states = AUTH_POST_USER},
    {.name = "QUIT", .command_handler = (command_handler) & handle_quit_command,  .type = QUIT,   .valid_states = AUTH_PRE_USER | AUTH_POST_USER | TRANSACTION},
    {.name = "STAT", .command_handler = (command_handler) & handle_stat_command,  .type = STAT,   .valid_states = TRANSACTION},
    {.name = "LIST", .command_handler = (command_handler) & handle_list_command,  .type = LIST,   .valid_states = TRANSACTION},
    {.name = "RETR", .command_handler = (command_handler) & handle_retr_command,  .type = RETR,   .valid_states = TRANSACTION},
    {.name = "DELE", .command_handler = (command_handler) & handle_noop,          .type = DELE,   .valid_states = TRANSACTION},
    {.name = "RSET", .command_handler = (command_handler) & handle_noop,          .type = RSET,   .valid_states = TRANSACTION},
    {.name = "CAPA", .command_handler = (command_handler) & handle_noop,          .type = CAPA,   .valid_states = AUTH_PRE_USER | AUTH_POST_USER | TRANSACTION}
};

int handle_pop3_client(void *index, bool can_read, bool can_write) {
    int i = *(int*) index;
    socket_handler* socket = &sockets[i];

    if(can_write) {
        size_t bytes_to_read = buffer_available_chars_count(socket->writing_buffer);
        char* message = malloc(bytes_to_read + 1);
        if(message == NULL) LOG_AND_RETURN(ERROR, "Error writing to pop3 client", -1);

        size_t read_bytes = buffer_read(socket->writing_buffer, message, bytes_to_read); 
        ssize_t sent_bytes = send(socket->fd, message, read_bytes, 0);
        free(message);
        if(sent_bytes < 0) {
            log(INFO, "Socket %d - unknown error\n", i);
            goto close_client;
        }
        
        buffer_advance_read(socket->writing_buffer, sent_bytes);
        
        //si ya vaciamos todo el buffer de salida, le decimos al selector que no nos despierte para escribir
        if(buffer_available_chars_count(socket->writing_buffer) == 0) {
            socket->try_write = false;
            if(socket->pop3_client_info->closing){
                log(INFO, "Socket %d - closing session\n", i);
                goto close_client;
            }
        }
        //ahora que hicimos lugar en el buffer, intentamos resolver el comando que haya quedado pendiente
        if(socket->pop3_client_info->pending_command != NULL) {
            command_t* old_pending_command = socket->pop3_client_info->pending_command;
            socket->pop3_client_info->pending_command = old_pending_command->command_handler(old_pending_command, socket->writing_buffer, socket->pop3_client_info);
            socket->try_write = true;
            free(old_pending_command);
        }
    }

    struct parser* parser = socket->pop3_client_info->parser_state;

    if(can_read && !socket->pop3_client_info->closing) {
        // todo: magic number
        char* message = malloc(sizeof(char) * 512);
        if(message == NULL) LOG_AND_RETURN(ERROR, "Error reading from pop3 client", -1);
        ssize_t received_bytes = recv(socket->fd, message, 512, 0);
        // Si recv devuelve 0 es porque se desconecto.
        if(received_bytes <= 0) {
            free(message);
            if(received_bytes < 0)
                LOG_AND_RETURN(ERROR, "Error reading from pop3 client", -1);
            else {
                log(INFO, "Socket %d - connection lost\n", i);
                goto close_client;
            }
        }
        
        log(DEBUG,"Received %ld bytes from socket %d", received_bytes, socket->fd);
        
    
        for(int i = 0;  i < received_bytes ; i += 1) {
            if(parser_feed(parser, message[i])->finished) {
                finish_event_item(parser);
            }
        }
        free(message);
    }

    if(socket->pop3_client_info->pending_command == NULL && !socket->pop3_client_info->closing) {
       struct parser_event* event = get_last_event(parser);
        if(event != NULL) {
            bool found_command = false;
            if(event->args[0] != NULL) {
                str_to_upper(event->args[0]);
                for(int i = 0; i < COMMAND_COUNT && !found_command; i++) {
                    if(((commands[i].valid_states & socket->pop3_client_info->current_state) > 0) && strcmp(event->args[0], commands[i].name) == 0) {
                        found_command = true;
                        command_t command_state = {
                            .type = commands[i].type, 
                            .command_handler = commands[i].command_handler, 
                            .args[0] = event->args[1],
                            .args[1] = event->args[2],
                            .answer = NULL,
                            .answer_alloc = false,
                            .index = 0,
                            .retr_state.emailfd=0,
                            .retr_state.finished_line=false,
                            .retr_state.multiline_state = 0,
                            };
                    
                        log(DEBUG, "Command %s received with args %s and %s\n", event->args[0], event->args[1], event->args[2]);

                        socket->pop3_client_info->pending_command = command_state.command_handler(&command_state, socket->writing_buffer, socket->pop3_client_info);
                        socket->try_write = true;
                        free_event(event, false);
                    }
                }
            }
            if(!found_command) {
                command_t command_state = {
                        .type = INVALID, 
                        .command_handler = (command_handler) & handle_invalid_command, 
                        .args[0] = NULL,
                        .args[1] = NULL,
                        .answer = NULL,
                        .answer_alloc = false,
                        .index = 0,
                        };
                log(DEBUG, "Invalid command %s received\n", event->args[0]);
                socket->pop3_client_info->pending_command = command_state.command_handler(&command_state, socket->writing_buffer, socket->pop3_client_info);
                socket->try_write = true;
                free_event(event, true);
            }
        }         
    }
    return 0;

    close_client:
        if (sockets[i].pop3_client_info->current_state & TRANSACTION) {
            sockets[i].pop3_client_info->selected_user->locked = false;
        }
        // MANAGE STATE
        free_client(i);
        return -1;
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
            LOG_AND_RETURN(ERROR, "Error accepting pop3 connection", -1);
        }

        for (int i = 0; i < MAX_SOCKETS; i += 1)
        {
            if (!sockets[i].occupied)
            {
                log(DEBUG, "accepted client socket: %d\n", i);
                sockets[i].fd = client_socket;
                sockets[i].occupied = true;
                sockets[i].handler = (int (*)(void *, bool, bool)) & handle_pop3_client;
                sockets[i].try_read = true;
                sockets[i].pop3_client_info = calloc(1, sizeof(pop3_client));
                sockets[i].pop3_client_info->current_state = AUTH_PRE_USER;
                sockets[i].pop3_client_info->parser_state = set_up_parser();
                sockets[i].pop3_client_info->closing =false;
                sockets[i].pop3_client_info->selected_user = NULL;
                sockets[i].writing_buffer = buffer_init(BUFFER_SIZE);
                current_socket_count += 1;

                // GREETING
                command_t greeting_state = {
                    .type = GREETING,
                    .command_handler = (command_handler)& handle_greeting_command,
                    .args[0] = NULL,
                    .args[1] = NULL,
                    .answer = NULL,
                    .answer_alloc = false,
                    .index = 0
                };

                sockets[i].pop3_client_info->pending_command = handle_greeting_command(&greeting_state, sockets[i].writing_buffer, sockets[i].pop3_client_info);
                sockets[i].try_write = true;
                return 0;
            }
        }
        
    }
    return -1;
}
/*  Simple command siempre es llamado por otro comando, por lo que tenes que tener en cuenta
    el caso en que no termine de escribir y volver a llamar a simple command de forma correcta.
    Adentro se hace un malloc, sirve para poder volver a llamar al comando que llamo a simple command
    para que vuelva a intentar escribir, si se vuelve a llamar se encarga despues de liberar el viejo y
    el nuevo malloc si se termina de escribir en esa llamda.
    Si haces un malloc sobre el answer, pasarlo por state->answer y poner alloc_answer = true.
    Si no usas simple command para escribir, debes hacer un malloc del command_state en c da llamada y liberarlo 
    si terminas de escribir, sino deberias copiar al nuevo malloc lo necesario para seguir escribiendo en la siguiente llamada */
command_t* handle_simple_command(command_t* command_state, buffer_t buffer, char* answer) {
    command_t* command = malloc(sizeof(command_t));
    if(command == NULL) LOG_AND_RETURN(FATAL, "Error handling command", command);
    if(command_state->answer == NULL) {
        if(answer == NULL) {
            log(FATAL, "Unexpected error \n%s", "");
        }
        size_t length = strlen(answer);
        command->answer = malloc(length + 1);
        command->answer_alloc = true;
        if(command->answer == NULL) LOG_AND_RETURN(FATAL, "Error handling command", command);
        strncpy(command->answer, answer, length + 1);
    } else {
        command->answer = command_state->answer;
        command->answer_alloc = command_state->answer_alloc;
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


command_t* handle_user_command(command_t* command_state, buffer_t buffer, pop3_client* client_state) {
    char* answer = NULL;
    if(command_state->answer == NULL) {
        /* lógica de USER para saber cual es answer */
        char * username = command_state->args[0];
        queue_t list = global_config.users;
        if(username != NULL){
            iterator_to_begin(list);
            while(iterator_has_next(list)){
                user_t* user = iterator_next(list);
                if(strcmp(user->username, username) == 0){
                    client_state->selected_user = user;
                    client_state->current_state = AUTH_POST_USER;
                    answer = USER_OK_MSG;
                    break;
                }
            }
        }
        answer = (client_state->current_state == AUTH_POST_USER)? answer : USER_ERR_MSG;
    }
    return handle_simple_command(command_state, buffer, answer);
}


// TODO: FIX MULTILINE AND BUFFER STUFFING
command_t* handle_retr_write_command(command_t* command_state, buffer_t buffer, pop3_client* client_state) {
    if (command_state->answer == NULL) { //STARTUP
        command_state->answer = malloc(MAX_LINE + 1);
        command_state->retr_state.multiline_state = 0;
        strncpy(command_state->answer, RETR_OK_MSG, RETR_OK_MSG_LENGTH);
        command_state->index = RETR_OK_MSG_LENGTH - 1;
        command_state->answer[MAX_LINE] = '\0';
        command_state->retr_state.finished_line = true;
    }
    // if emailfd == -1, it has finished reading
    if (command_state->retr_state.emailfd != -1 && command_state->retr_state.finished_line) {
        memset(command_state->answer + command_state->index, '\0', MAX_LINE - 2 - command_state->index);
        ssize_t nbytes = read(command_state->retr_state.emailfd, command_state->answer + command_state->index, MAX_LINE - 2);
        if (nbytes == 0 || nbytes < (MAX_LINE - 2)) {
            close(command_state->retr_state.emailfd);
            command_state->retr_state.emailfd = -1;
            snprintf(command_state->answer + command_state->index + nbytes, 7, "%s.%s",CRLF,CRLF);
        } else {
            if (nbytes == (MAX_LINE - 2)) {
                command_state->answer[MAX_LINE - 2] = '\r';
                command_state->answer[MAX_LINE - 1] = '\n';
            }
            command_state->retr_state.finished_line = false;
            command_state->index = 0;
        }
    } 
    
    size_t remaining_bytes_to_write = strlen(command_state->answer) - command_state->index;
    size_t written_bytes = 0;
    bool has_written = true;

    // byte-stuffing \r\n.\r\n
    for (size_t i = 0; i < remaining_bytes_to_write && has_written; i += 1) {
        char written_character = *(command_state->answer + command_state->index + i);
        if (written_character == '\r') {
            command_state->retr_state.multiline_state = 1;
        } else if (command_state->retr_state.multiline_state == 1 && written_character == '\n') {
            command_state->retr_state.multiline_state = 2;
        } else if (command_state->retr_state.multiline_state == 2 && written_character == '.' && command_state->retr_state.emailfd != -1) {
            has_written = buffer_write_and_advance(buffer, ".", 1);
            written_bytes += has_written;
        } else {
            command_state->retr_state.multiline_state = 0;
        }
        has_written =  buffer_write_and_advance(buffer, &written_character, 1);
        written_bytes += has_written; 
    }
    if (written_bytes >= remaining_bytes_to_write) {
        command_state->retr_state.finished_line = true;
        command_state->index = 0;
        if (command_state->retr_state.emailfd == -1) {
           free_command(command_state);
           return NULL;
        }
    }
    command_state->index += written_bytes;
    return command_state;
}

command_t* handle_retr_command(command_t* command_state, buffer_t buffer, pop3_client* client_state) {
    // blocking version
    command_t* command = malloc(sizeof(command_t));
    if(command_state->retr_state.emailfd == 0){
        if (command_state->args[0] == NULL) {
            return handle_simple_command(command_state, buffer, RETR_ERR_MISS_MSG);
        } else {
            int index = atoi(command_state->args[0]);
            if(index <= 0){
                return handle_simple_command(command_state,buffer, INVALID_NUMBER_ARGUMENT);
            }
            email_metadata_t* email = get_email_at_index(client_state, index - 1);
            if (email == NULL) {
                return handle_simple_command(command_state, buffer, RETR_ERR_FOUND_MSG);
            }
            int emailfd = open_email_file(client_state, email->filename);
            // todo: error opening file
            int flags = fcntl(emailfd, F_GETFL, 0);
            fcntl(emailfd, F_SETFL, flags | O_NONBLOCK);
            command->answer = NULL;
            command->retr_state.emailfd = emailfd;
            command->index = 0;
           
        }
    }else{
        command->index = command_state->index;
        command->answer = command_state->answer;
        command->retr_state.emailfd = command_state->retr_state.emailfd;
    }
    command->args[0] = command_state->args[0];
    command->args[1] = command_state->args[1];
    command->answer_alloc = true;
    command->command_handler = command_state->command_handler;
    command->type = RETR;
    return handle_retr_write_command(command, buffer, client_state);
}



command_t* handle_pass_command(command_t* command_state, buffer_t buffer, pop3_client* client_state) {
    char* answer = NULL;
    if(command_state->answer == NULL) {
        char* password = command_state->args[0];
        if(password != NULL){
            if(strcmp(password,client_state->selected_user->password) == 0){
                if(!client_state->selected_user->locked){
                    answer = PASS_OK_MSG;
                    client_state->current_state = TRANSACTION;
                    client_state->selected_user->locked = true;
                    //TODO: ACA hacer lo de LOCK para el usuario si no esta disponible pones -ERR unable to lock maildrop 
                    //Y cuanod se haga QUIT liberar el lock

                    char *user_maildir = join_path(global_config.maildir, client_state->selected_user->username);
                    client_state->emails = get_file_info(user_maildir, &(client_state->emails_count));
                    log(INFO, "retrieved emails: %ld\n", client_state->emails_count);
                    free(user_maildir);
                }else{
                    answer = PASS_ERR_LOCK_MSG;
                }
            } else {
                answer = PASS_ERR_MSG;
            }
        } else {
            answer = PASS_ERR_MSG;
        }
    }
    if(client_state->current_state != TRANSACTION){
        client_state->selected_user = NULL;
        client_state->current_state = AUTH_PRE_USER;
    }  
    return handle_simple_command(command_state, buffer, answer);
}

command_t* handle_quit_command(command_t* command_state, buffer_t buffer, pop3_client* client_state) {
    client_state->closing = true;
    char* answer = QUIT_MSG;
    if (client_state->current_state & TRANSACTION) {
        client_state->selected_user->locked = false;
        client_state->current_state = UPDATE;

        //Aca va la logica de eliminar emails cuando se termina la sesion
        answer = QUIT_AUTHENTICATED_MSG;
    }
    return handle_simple_command(command_state,buffer,answer);
}

command_t* handle_stat_command(command_t* command_state, buffer_t buffer, pop3_client* client_state){
    if(command_state->answer != NULL){
        return handle_simple_command(command_state,buffer,NULL);
    }
    size_t non_deleted_email_count = 0;
    size_t total_octets = 0;
    for (size_t i = 0; i < client_state->emails_count; i += 1) {
        if (!client_state->emails[i].deleted) {
            non_deleted_email_count += 1;
            total_octets += client_state->emails[i].octets;
        }
    }
    char* answer = malloc(STAT_OK_MSG_LENGTH);
    snprintf(answer, STAT_OK_MSG_LENGTH, STAT_OK_MSG, non_deleted_email_count, total_octets);
    command_state->answer_alloc = true;
    command_state->answer = answer;
    return handle_simple_command(command_state, buffer,NULL);
    
}

command_t* handle_list_command(command_t* command_state, buffer_t buffer, pop3_client* client_state){
    if(command_state->answer != NULL){
        return handle_simple_command(command_state,buffer,NULL);
    }
    if( command_state->args[0] != NULL){
        int index = atoi(command_state->args[0]);
        if(index <= 0){
            return handle_simple_command(command_state,buffer, INVALID_NUMBER_ARGUMENT);
        }
        email_metadata_t* email_data = get_email_at_index(client_state, index - 1);
        if(email_data == NULL){
            return handle_simple_command(command_state,buffer, NON_EXISTANT_EMAIL_MSG);
        }
        char* listing_response = malloc(MAX_LISTING_SIZE * 2);
        command_state->answer_alloc = true;
        command_state->answer = listing_response;
        log(DEBUG, "index: %d, octets: %ld\n\n", index, email_data->octets);
        int bytes = snprintf(listing_response,MAX_LISTING_SIZE,OCTETS_FORMAT_MSG, index, email_data->octets);
        if(bytes == MAX_LISTING_SIZE){
            //No se si aca hacemos algo porq seguro salio cortado
            return handle_simple_command(command_state,buffer, LISTING_SIZE_ERR_MSG);
        }
        return handle_simple_command(command_state,buffer,NULL);
    } else {
        size_t non_deleted_email_count = 0;
        size_t total_octets = 0;
        for (size_t i = 0; i < client_state->emails_count; i += 1) {
            if (!client_state->emails[i].deleted) {
                non_deleted_email_count += 1;
                total_octets += client_state->emails[i].octets;
            }            
        }
        int current_answer_index = 0;
        int current_answer_size = sizeof(char *) * MAX_LISTING_SIZE * INITIAL_LISTING_COUNT;
        command_state->answer_alloc = true;
        char *listing_response = malloc(current_answer_size);
        current_answer_index += snprintf(listing_response, MAX_LISTING_SIZE, OK_LIST_RESPONSE, non_deleted_email_count, total_octets);
        
        size_t listing_index = 1;
        for (size_t i = 0; i < client_state->emails_count; i += 1) {
            if (!client_state->emails[i].deleted) {
                current_answer_index += snprintf(listing_response + current_answer_index, MAX_LISTING_SIZE, LISTING_RESPONSE_FORMAT, listing_index, client_state->emails[i].octets);
                listing_index += 1;
                if ((current_answer_size - current_answer_index) < MAX_LISTING_SIZE) {
                    current_answer_size *= 2;
                    listing_response = realloc(listing_response, current_answer_size);
                }
            }
        }
        command_state->answer_alloc = true;
        command_state->answer = listing_response;
        snprintf(listing_response + current_answer_index, MAX_LISTING_SIZE, "%s", SEPARATOR);
        return handle_simple_command(command_state, buffer,NULL);
    }
}

command_t* handle_greeting_command(command_t* command_state, buffer_t buffer, pop3_client* client_state) {
    return handle_simple_command(command_state, buffer, GREETING_MSG);
}

command_t* handle_invalid_command(command_t* command_state, buffer_t buffer, pop3_client* client_state) {
    return handle_simple_command(command_state, buffer, INVALID_MSG);
}

command_t* handle_noop(command_t* command_state, buffer_t buffer, pop3_client* client_state) {
    return handle_simple_command(command_state, buffer, NOOP_MSG);
}

void free_command (command_t* command) {
    if(command == NULL) return;
    free(command->args[0]);
    free(command->args[1]);
    if(command->answer_alloc){
        free(command->answer);
    }
    free(command);
}

void free_event (struct parser_event* event, bool free_arguments) {
    if(free_arguments) {
        free(event->args[1]);
        free(event->args[2]);
    }
    free(event->args[0]);
    free(event);
}

void free_pop3_client(pop3_client* client) {
    parser_destroy(client->parser_state);
    free(client->emails);
    free(client->pending_command);
    free(client);
}
void free_client(int index){
    free_pop3_client(sockets[index].pop3_client_info);
}

void log_emails(email_metadata_t *emails, size_t c) {
    for (size_t i = 0; i < c; i += 1) {
        log(INFO, "email %s octets: %lu\n", emails[i].filename, emails[i].octets);
    }
}

// indice empezando de 0
email_metadata_t* get_email_at_index(pop3_client* state, size_t index) {
    bool found = false;
    size_t current_index = 0;
    size_t i;
    for (i = 0; i < state->emails_count && !found; i += 1) {
        if (!state->emails[i].deleted) {
            if (index == current_index) {
                found = true;
                break;
            } else {
                current_index += 1;
            }
        }
    }
    return (found) ? &(state->emails[i]) : NULL;
}