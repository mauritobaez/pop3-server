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

#define RECV_BUFFER_SIZE 512

command_t *handle_noop(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_pass_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_invalid_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_greeting_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_quit_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_stat_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_list_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_retr_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_rset_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_dele_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
command_t *handle_capa(command_t *command_state, buffer_t buffer, client_info_t *client_state);


void free_event(struct parser_event *event, bool free_arguments);
void free_pop3_client(pop3_client *client);

command_info commands[COMMAND_COUNT] = {
    {.name = "NOOP", .command_handler = (command_handler)&handle_noop, .type = NOOP, .valid_states = TRANSACTION},
    {.name = "USER", .command_handler = (command_handler)&handle_user_command, .type = USER, .valid_states = AUTH_PRE_USER},
    {.name = "PASS", .command_handler = (command_handler)&handle_pass_command, .type = PASS, .valid_states = AUTH_POST_USER},
    {.name = "QUIT", .command_handler = (command_handler)&handle_quit_command, .type = QUIT, .valid_states = AUTH_PRE_USER | AUTH_POST_USER | TRANSACTION},
    {.name = "STAT", .command_handler = (command_handler)&handle_stat_command, .type = STAT, .valid_states = TRANSACTION},
    {.name = "LIST", .command_handler = (command_handler)&handle_list_command, .type = LIST, .valid_states = TRANSACTION},
    {.name = "RETR", .command_handler = (command_handler)&handle_retr_command, .type = RETR, .valid_states = TRANSACTION},
    {.name = "DELE", .command_handler = (command_handler)&handle_dele_command, .type = DELE, .valid_states = TRANSACTION},
    {.name = "RSET", .command_handler = (command_handler)&handle_rset_command, .type = RSET, .valid_states = TRANSACTION},
    {.name = "CAPA", .command_handler = (command_handler)&handle_capa, .type = CAPA, .valid_states = AUTH_PRE_USER | AUTH_POST_USER | TRANSACTION}};

int handle_pop3_client(void *index, bool can_read, bool can_write)
{
    int i = *(int *)index;
    socket_handler *socket = &sockets[i];
    pop3_client* pop3_client_info = socket->client_info.pop3_client_info;

    if (can_write)
    {
        int sent_bytes = send_from_socket_buffer(i);
        if(sent_bytes == -1)   return -1;
        if(sent_bytes == -2)   goto close_client;

        add_sent_bytes(sent_bytes);
        
        if (buffer_available_chars_count(socket->writing_buffer) == 0 && pop3_client_info->closing)
        {
            log(DEBUG, "Socket %d - closing session\n", i);
            goto close_client;
        }
        
        // ahora que hicimos lugar en el buffer, intentamos resolver el comando que haya quedado pendiente
        if (pop3_client_info->pending_command != NULL)
        {
            command_t *old_pending_command = pop3_client_info->pending_command;
            pop3_client_info->pending_command = old_pending_command->command_handler(old_pending_command, socket->writing_buffer, &(socket->client_info));
            socket->try_write = true;
            free(old_pending_command);
        }
    }

    struct parser *parser = pop3_client_info->parser_state;

    if (can_read && !pop3_client_info->closing)
    {
        int ans = recv_to_parser(i, pop3_client_info->parser_state, RECV_BUFFER_SIZE);
        if(ans == -1)   LOG_AND_RETURN(ERROR, "Error reading from pop3 client", -1);
        if(ans == -2)   goto close_client;
    }

    if (pop3_client_info->pending_command == NULL && !pop3_client_info->closing)
    {
        struct parser_event *event = get_last_event(parser);
        if (event != NULL)
        {
            bool found_command = false;
            if (event->args[0] != NULL)
            {
                str_to_upper(event->args[0]);
                for (int i = 0; i < COMMAND_COUNT && !found_command; i++)
                {
                    if (((commands[i].valid_states & pop3_client_info->current_state) > 0) && strcmp(event->args[0], commands[i].name) == 0)
                    {
                        found_command = true;
                        command_t command_state = {
                            .type = commands[i].type,
                            .command_handler = commands[i].command_handler,
                            .args[0] = event->args[1],
                            .args[1] = event->args[2],
                            .answer = NULL,
                            .answer_alloc = false,
                            .index = 0,
                            .meta_data = NULL
                        };

                        log(DEBUG, "Command %s received with args %s and %s\n", event->args[0], event->args[1], event->args[2]);

                        pop3_client_info->pending_command = command_state.command_handler(&command_state, socket->writing_buffer, &(socket->client_info));
                        socket->try_write = true;
                        free_event(event, false);
                    }
                }
            }
            if (!found_command)
            {
                command_t command_state = {
                    .type = INVALID,
                    .command_handler = (command_handler)&handle_invalid_command,
                    .args[0] = NULL,
                    .args[1] = NULL,
                    .answer = NULL,
                    .answer_alloc = false,
                    .index = 0,
                    .meta_data = NULL,
                };
                log(DEBUG, "Invalid command %s received\n", event->args[0]);
                pop3_client_info->pending_command = command_state.command_handler(&command_state, socket->writing_buffer, &(socket->client_info));
                socket->try_write = true;
                free_event(event, true);
            }
        }
    }
    return 0;

close_client:
    return -1;
}

int accept_pop3_connection(void *index, bool can_read, bool can_write)
{
    if (can_read)
    {
        int sock_index = *(int *)index;
        socket_handler *socket_state = &sockets[sock_index];

        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_socket = accept(socket_state->fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_socket < 0)
        {
            LOG_AND_RETURN(ERROR, "Error accepting pop3 connection", 0);
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
                sockets[i].free_client = &free_client_pop3;
                sockets[i].client_info.pop3_client_info = calloc(1, sizeof(pop3_client));
                sockets[i].client_info.pop3_client_info->current_state = AUTH_PRE_USER;
                sockets[i].client_info.pop3_client_info->parser_state = set_up_parser();
                sockets[i].client_info.pop3_client_info->closing = false;
                sockets[i].client_info.pop3_client_info->selected_user = NULL;
                sockets[i].writing_buffer = buffer_init(POP3_WRITING_BUFFER_SIZE);
                sockets[i].last_interaction = time(NULL);
                sockets[i].passive = false;
                current_socket_count += 1;
                add_connection_metric();
                // GREETING
                command_t greeting_state = {
                    .type = GREETING,
                    .command_handler = (command_handler)&handle_greeting_command,
                    .args[0] = NULL,
                    .args[1] = NULL,
                    .answer = NULL,
                    .answer_alloc = false,
                    .index = 0,
                    .meta_data = NULL
                    };

                sockets[i].client_info.pop3_client_info->pending_command = handle_greeting_command(&greeting_state, sockets[i].writing_buffer, &(sockets[i].client_info));
                sockets[i].try_write = true;
                return 1;
            }
        }
    }
    return 0;
}


command_t *handle_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state_)
{
    pop3_client* client_state = client_state_->pop3_client_info;
    char *answer = NULL;
    if (command_state->answer == NULL)
    {
        /* lógica de USER para saber cual es answer */
        char *username = command_state->args[0];
        queue_t list = global_config.users;
        if (username != NULL)
        {
            iterator_to_begin(list);
            while (iterator_has_next(list))
            {
                user_t *user = iterator_next(list);
                if (strcmp(user->username, username) == 0 && !user->removed)
                {
                    client_state->selected_user = user;
                    client_state->current_state = AUTH_POST_USER;
                    answer = USER_OK_MSG;
                    break;
                }
            }
        }
        answer = (client_state->current_state == AUTH_POST_USER) ? answer : USER_ERR_MSG;
    }
    return handle_simple_command(command_state, buffer, answer);
}

command_t *handle_retr_write_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    if (command_state->answer == NULL)
    { // STARTUP
        command_state->answer = malloc(MAX_LINE + 1);
        RETR_STATE(command_state)->multiline_state = 2;
        RETR_STATE(command_state)->final_dot = false;
        strncpy(command_state->answer, RETR_OK_MSG, RETR_OK_MSG_LENGTH);
    }
    if (!RETR_STATE(command_state)->greeting_done) //Poner el mensaje inicial
    {
        command_state->index += buffer_write_and_advance(buffer, command_state->answer + command_state->index, RETR_OK_MSG_LENGTH - command_state->index - 1);
        if (command_state->index >= RETR_OK_MSG_LENGTH-1)
        {
            RETR_STATE(command_state)->greeting_done = true;
            RETR_STATE(command_state)->finished_line = true;
            command_state->index = 0;
        }
        return command_state;
    }
    if(RETR_STATE(command_state)->emailfd == -1 && !RETR_STATE(command_state)->final_dot && command_state->index == 0){ //Termino de escribir entonces copio el ultimo \r\n . \r\n
        strncpy(command_state->answer, (RETR_STATE(command_state)->multiline_state == 2) ? FINAL_MESSAGE_RETR : FINAL_MESSAGE_RETR_PADDED, MAX_LINE);
        RETR_STATE(command_state)->final_dot = true;
    }
    // if emailfd == -1, it has finished reading
    if (RETR_STATE(command_state)->emailfd != -1 && RETR_STATE(command_state)->finished_line)
    {
        ssize_t nbytes = read(RETR_STATE(command_state)->emailfd, command_state->answer, MAX_LINE);
        command_state->answer[nbytes] = '\0';
        log(DEBUG, "Read %ld bytes from emailfd", nbytes);
        if (nbytes == 0 || nbytes < MAX_LINE)
        {
            close(RETR_STATE(command_state)->emailfd);
            RETR_STATE(command_state)->emailfd = -1;
        }
        else
        {
            RETR_STATE(command_state)->finished_line = false;
            command_state->index = 0;
        }
    }
    size_t remaining_bytes_to_write = strlen(command_state->answer) - command_state->index;
    size_t written_bytes = 0;
    bool has_written = true;
    // byte-stuffing \r\n.\r\n
    for (size_t i = 0; i < remaining_bytes_to_write && has_written; i += 1)
    {
        char written_character = *(command_state->answer + command_state->index + i);
        if (!(RETR_STATE(command_state)->final_dot)) {
            if (written_character == '\r')
            {
                RETR_STATE(command_state)->multiline_state = 1;
            }
            else if (RETR_STATE(command_state)->multiline_state == 1 && written_character == '\n')
            {
                RETR_STATE(command_state)->multiline_state = 2;
            }
            else if (RETR_STATE(command_state)->multiline_state == 2 && written_character == '.')
            {   
                has_written = buffer_write_and_advance(buffer, ".", 1);
                if(has_written){
                    RETR_STATE(command_state)->multiline_state = 0;
                }
            }
            else
            {
                RETR_STATE(command_state)->multiline_state = 0;
            }
        }
        has_written = buffer_write_and_advance(buffer, &written_character, 1);
        written_bytes += has_written;
    }

    if (written_bytes >= remaining_bytes_to_write)
    {
        RETR_STATE(command_state)->finished_line = true;
        command_state->index = 0;
        // Me aseguro que se escriba lo ultimo
        if(RETR_STATE(command_state)->emailfd == -1 && RETR_STATE(command_state)->final_dot){ 
            free_command(command_state);
            return NULL;
        }
    }
    else
    {
        command_state->index += written_bytes;
    }
    return command_state;
}

command_t *handle_retr_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    pop3_client* pop3_state = client_state->pop3_client_info;
    command_t *command = calloc(1,sizeof(command_t));
    if (RETR_STATE(command_state) == NULL || RETR_STATE(command_state)->emailfd == 0)
    {
        if (command_state->args[0] == NULL)
        {
            free(command);
            return handle_simple_command(command_state, buffer, RETR_ERR_MISS_MSG);
        }
        else
        {
            int index = atoi(command_state->args[0]);
            if (index <= 0)
            {
                free(command);
                return handle_simple_command(command_state, buffer, INVALID_NUMBER_ARGUMENT);
            }
            email_metadata_t *email = get_email_at_index(pop3_state, index - 1);
            if (email == NULL)
            {
                free(command);
                return handle_simple_command(command_state, buffer, RETR_ERR_FOUND_MSG);
            }
            //No deberia romperse porque ya se valido que el archivo exista cuando lo indexamos
            int emailfd = open_email_file(pop3_state, email->filename);
            int flags = fcntl(emailfd, F_GETFL, 0);
            fcntl(emailfd, F_SETFL, flags | O_NONBLOCK);
            command->answer = NULL;
            command->meta_data = malloc(sizeof(retr_state_t));
            RETR_STATE(command)->emailfd = emailfd;
            command->index = 0;
            RETR_STATE(command)->finished_line = false;
            RETR_STATE(command)->greeting_done = false;
            RETR_STATE(command)->multiline_state = 0;
            RETR_STATE(command)->final_dot = false;
            add_email_read();
            log(INFO, "User: %s retrieved email %s",client_state->pop3_client_info->selected_user->username, email->filename)
        }
    }
    else
    {
        command->index = command_state->index;
        command->answer = command_state->answer;
        command->meta_data = command_state->meta_data;
    }
    command->args[0] = command_state->args[0];
    command->args[1] = command_state->args[1];
    command->answer_alloc = true;
    command->command_handler = command_state->command_handler;
    command->type = RETR;
    return handle_retr_write_command(command, buffer, client_state);
}

command_t *handle_pass_command(command_t *command_state, buffer_t buffer, client_info_t *client_state_)
{
    pop3_client* client_state = client_state_->pop3_client_info;
    char *answer = NULL;
    if (command_state->answer == NULL)
    {
        char *password = command_state->args[0];
        if (password != NULL)
        {
            if (strcmp(password, client_state->selected_user->password) == 0)
            {
                if (!client_state->selected_user->locked)
                {
                    answer = PASS_OK_MSG;
                    client_state->current_state = TRANSACTION;
                    client_state->selected_user->locked = true;
                    add_loggedin_user();
                    char *user_maildir = join_path(global_config.maildir, client_state->selected_user->username);
                    client_state->user_maildir = user_maildir;
                    client_state->emails = get_file_info(user_maildir, &(client_state->emails_count));
                    log(DEBUG, "retrieved emails: %ld\n", client_state->emails_count);
                    log(INFO, "User: %s logged in\n", client_state->selected_user->username);
                }
                else
                {
                    answer = PASS_ERR_LOCK_MSG;
                }
            }
            else
            {
                answer = PASS_ERR_MSG;
            }
        }
        else
        {
            answer = PASS_ERR_MSG;
        }
    }
    if (client_state->current_state != TRANSACTION)
    {
        client_state->selected_user = NULL;
        client_state->current_state = AUTH_PRE_USER;
    }
    return handle_simple_command(command_state, buffer, answer);
}

command_t *handle_quit_command(command_t *command_state, buffer_t buffer, client_info_t *client_state_)
{
    pop3_client* client_state = client_state_->pop3_client_info;
    client_state->closing = true;
    char *answer = QUIT_MSG;
    if (client_state->current_state & TRANSACTION)
    {
        client_state->selected_user->locked = false;
        client_state->current_state = UPDATE;
        bool error=false;

        int new_emails_count = client_state->emails_count;
        for(size_t i = 0; i < client_state->emails_count && !error; i++){
            if(client_state->emails[i].deleted){
                char *user_maildir = client_state->user_maildir;
                char *email_path = join_path(user_maildir, client_state->emails[i].filename);
                if( remove(email_path) == -1){
                    log(ERROR, "Error deleting email %s\n", email_path);
                    error=true;
                }else{
                    log(INFO, "User: %s deleted email %s\n",client_state->selected_user->username,email_path);
                    new_emails_count--;
                    // deleted email metric
                    add_email_removed();
                }
                free(email_path);
            }
        }
        // Aca va la logica de eliminar emails cuando se termina la sesion
        int answer_length = QUIT_AUTHENTICATED_MSG_LENGTH + MAX_DIGITS_INT + ((error)? QUIT_UNAUTHENTICATED_MSG_ERROR_LENGTH:0);
        command_state->answer = malloc(answer_length * sizeof(char));
        command_state->answer_alloc = true;
        int written_chars = snprintf(command_state->answer,answer_length, QUIT_AUTHENTICATED_MSG ,new_emails_count);
        if(error){
            snprintf(command_state->answer + written_chars,QUIT_UNAUTHENTICATED_MSG_ERROR_LENGTH, QUIT_UNAUTHENTICATED_MSG_ERROR);
        }

        add_successful_quit();
        return handle_simple_command(command_state, buffer, NULL);
    }
    return handle_simple_command(command_state, buffer, answer);
}

command_t *handle_stat_command(command_t *command_state, buffer_t buffer, client_info_t *client_state_)
{
    pop3_client* client_state = client_state_->pop3_client_info;
    if (command_state->answer != NULL)
    {
        return handle_simple_command(command_state, buffer, NULL);
    }
    size_t non_deleted_email_count = 0;
    size_t total_octets = 0;
    for (size_t i = 0; i < client_state->emails_count; i += 1)
    {
        if (!client_state->emails[i].deleted)
        {
            non_deleted_email_count += 1;
            total_octets += client_state->emails[i].octets;
        }
    }
    char *answer = malloc(STAT_OK_MSG_LENGTH);
    snprintf(answer, STAT_OK_MSG_LENGTH, STAT_OK_MSG, non_deleted_email_count, total_octets);
    command_state->answer_alloc = true;
    command_state->answer = answer;
    return handle_simple_command(command_state, buffer, NULL);
}

command_t *handle_list_command(command_t *command_state, buffer_t buffer, client_info_t *client_state_)
{
    pop3_client* client_state = client_state_->pop3_client_info;
    if (command_state->answer != NULL)
    {
        return handle_simple_command(command_state, buffer, NULL);
    }
    if (command_state->args[0] != NULL)
    {
        int index = atoi(command_state->args[0]);
        if (index <= 0)
        {
            return handle_simple_command(command_state, buffer, INVALID_NUMBER_ARGUMENT);
        }
        email_metadata_t *email_data = get_email_at_index(client_state, index - 1);
        if (email_data == NULL)
        {
            return handle_simple_command(command_state, buffer, NON_EXISTANT_EMAIL_MSG);
        }
        char *listing_response = malloc(MAX_LISTING_SIZE * 2);
        command_state->answer_alloc = true;
        command_state->answer = listing_response;
        log(DEBUG, "index: %d, octets: %ld\n\n", index, email_data->octets);
        int bytes = snprintf(listing_response, MAX_LISTING_SIZE, OCTETS_FORMAT_MSG, index, email_data->octets);
        if (bytes == MAX_LISTING_SIZE)
        {
            // No se si aca hacemos algo porq seguro salio cortado
            return handle_simple_command(command_state, buffer, LISTING_SIZE_ERR_MSG);
        }
        return handle_simple_command(command_state, buffer, NULL);
    }
    else
    {
        size_t non_deleted_email_count = 0;
        size_t total_octets = 0;
        for (size_t i = 0; i < client_state->emails_count; i += 1)
        {
            if (!client_state->emails[i].deleted)
            {
                non_deleted_email_count += 1;
                total_octets += client_state->emails[i].octets;
            }
        }
        int current_answer_index = 0;
        int current_answer_size = sizeof(char *) * MAX_LISTING_SIZE * INITIAL_LISTING_COUNT;
        command_state->answer_alloc = true;
        char *listing_response = malloc(current_answer_size);
        current_answer_index += snprintf(listing_response, MAX_LISTING_SIZE, OK_LIST_RESPONSE, non_deleted_email_count, total_octets);

        for (size_t i = 0; i < client_state->emails_count; i += 1)
        {
            if (!client_state->emails[i].deleted)
            {
                current_answer_index += snprintf(listing_response + current_answer_index, MAX_LISTING_SIZE, LISTING_RESPONSE_FORMAT, i + 1, client_state->emails[i].octets);
                if ((current_answer_size - current_answer_index) < MAX_LISTING_SIZE)
                {
                    current_answer_size *= 2;
                    listing_response = realloc(listing_response, current_answer_size);
                }
            }
        }
        command_state->answer_alloc = true;
        command_state->answer = listing_response;
        snprintf(listing_response + current_answer_index, MAX_LISTING_SIZE, "%s", SEPARATOR);
        return handle_simple_command(command_state, buffer, NULL);
    }
}

command_t *handle_greeting_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    return handle_simple_command(command_state, buffer, GREETING_MSG);
}

command_t *handle_invalid_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    return handle_simple_command(command_state, buffer, INVALID_MSG);
}

command_t *handle_noop(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    return handle_simple_command(command_state, buffer, NOOP_MSG);
}

command_t *handle_capa(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    return handle_simple_command(command_state, buffer, CAPA_MSG);
}

command_t *handle_rset_command(command_t *command_state, buffer_t buffer, client_info_t *client_state_)
{
    pop3_client* client_state = client_state_->pop3_client_info;
    for (size_t i = 0; i < client_state->emails_count; i += 1)
    {
        client_state->emails[i].deleted = false;
    }
    return handle_simple_command(command_state, buffer, RSET_MSG);
}

command_t *handle_dele_command(command_t *command_state, buffer_t buffer, client_info_t *client_state_)
{
    pop3_client* client_state = client_state_->pop3_client_info;
    if (command_state->args[0] == NULL)
    {
        return handle_simple_command(command_state, buffer, INVALID_NUMBER_ARGUMENT);
    }
    int message_index = atoi(command_state->args[0]);
    if (message_index <= 0)
    {
        return handle_simple_command(command_state, buffer, INVALID_NUMBER_ARGUMENT);
    }
    if ((size_t)message_index > client_state->emails_count) {
        return handle_simple_command(command_state, buffer, NON_EXISTANT_EMAIL_MSG);
    }
    email_metadata_t* email = &(client_state->emails[message_index - 1]);
    char response[MAX_LINE];
    if (email->deleted) {
        snprintf(response, MAX_LINE, DELETED_ALREADY_MSG, message_index);
    } else {
        email->deleted = true;
        snprintf(response, MAX_LINE, DELETED_MSG, message_index);
    }
    return handle_simple_command(command_state, buffer, response);
}

void free_pop3_client(pop3_client *client)
{
    if (client->current_state & TRANSACTION)
    {
        log(INFO, "User: %s logged out\n", client->selected_user->username);
        remove_loggedin_user();
        client->selected_user->locked = false;
    }
    parser_destroy(client->parser_state);
    for (size_t i = 0; i < client->emails_count; i += 1)
    { //Si no inicio sesion emails_count = 0 entonces no entra
        free(client->emails[i].filename);
    }
    free(client->emails);
    free_command(client->pending_command);
    free(client->user_maildir);
    free(client);
}

void free_client_pop3(int index)
{
    remove_connection_metric();
    free_pop3_client(sockets[index].client_info.pop3_client_info);
}

void log_emails(email_metadata_t *emails, size_t c)
{
    for (size_t i = 0; i < c; i += 1)
    {
        log(DEBUG, "email %s octets: %lu\n", emails[i].filename, emails[i].octets);
    }
}

// indice empezando de 0
email_metadata_t *get_email_at_index(pop3_client *state, size_t index)
{
    bool found = false;
    size_t i;
    for (i = 0; i < state->emails_count; i += 1)
    {
        if (!state->emails[i].deleted)
        {
            if (index == i)
            {
                found = true;
                break;
            }
        }
    }
    return (found) ? &(state->emails[i]) : NULL;
}
