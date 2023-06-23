#include "pop3_utils.h"

#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

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
//Array de comandos disponibles para POP3, todos con su funcion de handler y estado valido
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

//Funcion que maneja la logica de que comando ejecutar, la lectura y escritura de los sockets
int handle_pop3_client(void *index_ptr, bool can_read, bool can_write)
{
    //Agarramos los punteros necesarios para agilizar el codigo
    int index = *(int *)index_ptr;
    socket_handler *socket = &sockets[index];
    pop3_client *pop3_client_info = socket->client_info.pop3_client_info;
    
    if (can_write)
    {   //Si podemos escribir que envie lo que pueda del buffer de salida
        int sent_bytes = send_from_socket_buffer(index);
        if (sent_bytes == -1)
            return -1;
        if (sent_bytes == -2)
            goto close_client;
        //Añadimos a la metrica de bytes enviados
        add_sent_bytes(sent_bytes);
        //Si termino de escribir todo lo que teniamos pendiente y el cliente se quiere desconectar, lo desconectamos
        if (buffer_available_chars_count(socket->writing_buffer) == 0 && pop3_client_info->closing)
        {
            log(DEBUG, "Socket %d - closing session\n", index);
            goto close_client;
        }

        // Ahora que hicimos lugar en el buffer, intentamos resolver el comando que haya quedado pendiente
        if (pop3_client_info->pending_command != NULL)
        {
            command_t *old_pending_command = pop3_client_info->pending_command;
            pop3_client_info->pending_command = old_pending_command->command_handler(old_pending_command, socket->writing_buffer, &(socket->client_info));
            socket->try_write = true; // Si el comando no pudo ser resuelto, lo volvemos a intentar en el proximo ciclo
            free(old_pending_command);
        }
    }

    struct parser *parser = pop3_client_info->parser_state;

    if (can_read && !pop3_client_info->closing)
    {   //Leemos del sockey y se lo pasamos al parser
        int ans = recv_to_parser(index, pop3_client_info->parser_state, RECV_BUFFER_SIZE);
        if (ans == -1)
            LOG_AND_RETURN(ERROR, "Error reading from pop3 client", -1);
        if (ans == -2)
            goto close_client;
    }
    //Si no tenemos un comando pendiente para resolver, intentamos resolver el ultimo que llego a obtener el parser
    if (pop3_client_info->pending_command == NULL && !pop3_client_info->closing)
    {   
        struct parser_event *event = get_last_event(parser); //Los event tienen los datos y argumentos del comando a intentar ejecutar
        if (event != NULL)
        {
            bool found_command = false;
            if (event->args[0] != NULL)
            {   
                str_to_upper(event->args[0]);
                for (int i = 0; i < COMMAND_COUNT && !found_command; i++)
                {   //Iteramos por el array de comandos disponibles para ver si el comando que llego es valido en su nombre y estado actual del cliente
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
                            .meta_data = NULL};

                        log(DEBUG, "Command %s received with args %s and %s\n", event->args[0], event->args[1], event->args[2]);
                        //Llamamos al handler del comando y si se completo devuelve NULL, sino se deja como comando pendiente
                        pop3_client_info->pending_command = command_state.command_handler(&command_state, socket->writing_buffer, &(socket->client_info));
                        socket->try_write = true;
                        free_event(event, false);
                    }
                }
            }
            //No encontramos un comando valido, mandamos un error de comando invalido
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
                socket->try_write = true; //Aun cuando es un error, puede que no pueda escribir entonces es posible que se tenga que postergar
                free_event(event, true);
            }
        }
    }
    return 0;

close_client:
    return -1;
}
//Funcion que se usa para aceptar conexiones entrantes de pop3
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
        //Busca un socket en el array de tal forma que este desocupado y lo ocupa con todo lo necesario para que funcione el cliente de pop3
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
                    .meta_data = NULL};

                sockets[i].client_info.pop3_client_info->pending_command = handle_greeting_command(&greeting_state, sockets[i].writing_buffer, &(sockets[i].client_info));
                sockets[i].try_write = true; //Se encola el primer comando que es el greeting command
                return 1;
            }
        }
    }
    return 0;
}
//Funcion que itera por la lista de usuarios validos del servidor y si encuentra un match lo marca como candidato a login
command_t *handle_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    pop3_client *pop3_state = client_state->pop3_client_info;
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
                    pop3_state->selected_user = user;
                    pop3_state->current_state = AUTH_POST_USER;
                    answer = USER_OK_MSG;
                    break;
                }
            }
        }
        answer = (pop3_state->current_state == AUTH_POST_USER) ? answer : USER_ERR_MSG;
    }
    return handle_simple_command(command_state, buffer, answer);
}

command_t *handle_retr_write_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    retr_state_t *state = (retr_state_t *)((command_state)->meta_data);
    if (command_state->answer == NULL)
    { // STARTUP
        command_state->answer = malloc(MAX_LINE + 1);
        state->multiline_state = 2;
        state->final_dot = false;
        strncpy(command_state->answer, RETR_OK_MSG, RETR_OK_MSG_LENGTH);
    }
    if (!state->greeting_done) // Poner el mensaje inicial
    {
        command_state->index += buffer_write_and_advance(buffer, command_state->answer + command_state->index, RETR_OK_MSG_LENGTH - command_state->index - 1);
        if (command_state->index >= RETR_OK_MSG_LENGTH - 1)
        {
            state->greeting_done = true;
            state->finished_line = true;
            command_state->index = 0;
        }
        return command_state;
    }
    if (state->emailfd == -1 && !state->final_dot && command_state->index == 0)
    { // Termino de escribir entonces copio el ultimo \r\n . \r\n
        strncpy(command_state->answer, (state->multiline_state == 2) ? FINAL_MESSAGE_RETR : FINAL_MESSAGE_RETR_PADDED, MAX_LINE);
        state->final_dot = true;
    }
    // if emailfd == -1, it has finished reading
    if (state->emailfd != -1 && state->finished_line)
    {
        ssize_t nbytes = read(state->emailfd, command_state->answer, MAX_LINE);
        if (nbytes >= 0)
        {
            command_state->answer[nbytes] = '\0';
            log(DEBUG, "Read %ld bytes from emailfd", nbytes);
            if (nbytes == 0 || nbytes < MAX_LINE)
            {
                close(state->emailfd);
                pclose(state->email_stream);
                int status;
                waitpid(-1, &status, 0);
                state->emailfd = -1;
            }
            else
            {
                state->finished_line = false;
                command_state->index = 0;
            }
        }
        else
        {
            if (errno != EAGAIN)
                log(ERROR, "Read error %s", strerror(errno));
            command_state->answer[0] = '\0';
        }
    }
    size_t remaining_bytes_to_write = strlen(command_state->answer) - command_state->index;
    size_t written_bytes = 0;
    bool has_written = true;
    // byte-stuffing \r\n.\r\n
    for (size_t i = 0; i < remaining_bytes_to_write && has_written; i += 1)
    {
        char written_character = *(command_state->answer + command_state->index + i);
        if (!(state->final_dot))
        {
            if (written_character == '\r')
            {
                state->multiline_state = 1;
            }
            else if (state->multiline_state == 1 && written_character == '\n')
            {
                state->multiline_state = 2;
            }
            else if (state->multiline_state == 2 && written_character == '.')
            {
                has_written = buffer_write_and_advance(buffer, ".", 1);
                if (has_written)
                {
                    state->multiline_state = 0;
                }
            }
            else
            {
                state->multiline_state = 0;
            }
        }
        has_written = buffer_write_and_advance(buffer, &written_character, 1);
        written_bytes += has_written;
    }

    if (written_bytes >= remaining_bytes_to_write)
    {
        state->finished_line = true;
        command_state->index = 0;
        // Me aseguro que se escriba lo ultimo
        if (state->emailfd == -1 && state->final_dot)
        {
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
    pop3_client *pop3_state = client_state->pop3_client_info;
    command_t *command = calloc(1, sizeof(command_t));
    retr_state_t *retr_state = (retr_state_t *)command_state->meta_data;
    if (retr_state == NULL || retr_state->emailfd == 0)
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
            FILE *filestream = open_email_file(pop3_state, email->filename);
            int emailfd = fileno(filestream);
            int flags = fcntl(emailfd, F_GETFL, 0);
            fcntl(emailfd, F_SETFL, flags | O_NONBLOCK);
            command->answer = NULL;
            retr_state_t *metadata = malloc(sizeof(retr_state_t));

            metadata->emailfd = emailfd;
            command->index = 0;
            metadata->finished_line = false;
            metadata->greeting_done = false;
            metadata->multiline_state = 0;
            metadata->final_dot = false;
            metadata->email_stream = filestream;

            command->meta_data = metadata;
            command->free_metadata = (free_metadata_handler)&free_retr_state;
            add_email_read();
            log(INFO, "User: %s retrieved email %s", client_state->pop3_client_info->selected_user->username, email->filename)
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
//Funcion que en base al posible usuario elegido para el login revisa si matchea la password, dando acceso al usuario
command_t *handle_pass_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    pop3_client *pop3_state = client_state->pop3_client_info;
    char *answer = NULL;
    if (command_state->answer == NULL)
    {
        char *password = command_state->args[0];
        if (password != NULL)
        {
            if (strcmp(password, pop3_state->selected_user->password) == 0)
            {
                if (!pop3_state->selected_user->locked) //Revisamos si alguien no esta logueado con el usuario y obtenemos el lock
                {
                    answer = PASS_OK_MSG;
                    pop3_state->current_state = TRANSACTION;
                    pop3_state->selected_user->locked = true;
                    add_loggedin_user(); //Marcamos como metrica los usuarios logueados
                    char *user_maildir = join_path(global_config.maildir, pop3_state->selected_user->username);
                    pop3_state->emails = get_emails_at_directory(user_maildir, &(pop3_state->emails_count));//Indexamos los emails del usuario
                    free(user_maildir);
                    log(DEBUG, "retrieved emails: %ld\n", pop3_state->emails_count);
                    log(INFO, "User: %s logged in\n", pop3_state->selected_user->username);
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
    if (pop3_state->current_state != TRANSACTION)
    {   //Si no se logueo correctamente, volvemos al estado de autenticacion sin un usuario seleccionado
        pop3_state->selected_user = NULL;
        pop3_state->current_state = AUTH_PRE_USER;
    }
    return handle_simple_command(command_state, buffer, answer);
}
//Funcion que se encarga de manejar el quit command ya sea eliminando los emails marcados con DELE 
command_t *handle_quit_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    pop3_client *pop3_client = client_state->pop3_client_info;
    pop3_client->closing = true;
    char *answer = QUIT_MSG;
    if(command_state->answer != NULL){
        return handle_simple_command(command_state, buffer,NULL);
    }
    if (pop3_client->current_state & TRANSACTION) //Si se llamo no estando en transaction entonces no se estaba loggeado
    {
        pop3_client->selected_user->locked = false;
        pop3_client->current_state = UPDATE;
        bool error = false;

        int new_emails_count = pop3_client->emails_count; //Borramos los emails marcados con deleted
        for (size_t i = 0; i < pop3_client->emails_count && !error; i++)
        {
            if (pop3_client->emails[i].deleted)
            {
                char *email_path = pop3_client->emails[i].filename;
                if (remove(email_path) == -1)
                {
                    log(ERROR, "Error deleting email %s\n", email_path);
                    error = true;
                }
                else
                {
                    log(INFO, "User: %s deleted email %s\n", pop3_client->selected_user->username, email_path);
                    new_emails_count--;
                    // deleted email metric
                    add_email_removed();
                }
            }
        }
        int answer_length = QUIT_AUTHENTICATED_MSG_LENGTH + MAX_DIGITS_INT + ((error) ? QUIT_UNAUTHENTICATED_MSG_ERROR_LENGTH : 0);
        command_state->answer = malloc(answer_length * sizeof(char));
        command_state->answer_alloc = true;
        int written_chars = snprintf(command_state->answer, answer_length, QUIT_AUTHENTICATED_MSG, new_emails_count);
        if (error)
        {
            snprintf(command_state->answer + written_chars, QUIT_UNAUTHENTICATED_MSG_ERROR_LENGTH, QUIT_UNAUTHENTICATED_MSG_ERROR);
        }

        add_successful_quit();
        return handle_simple_command(command_state, buffer, NULL);
    }
    return handle_simple_command(command_state, buffer, answer);
}
//Simplemente se encarga de manejar el STAT command, no tenindo en cuenta los emails que se eliminaron con DELE
command_t *handle_stat_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    pop3_client *pop3_state = client_state->pop3_client_info;
    if (command_state->answer != NULL)
    {
        return handle_simple_command(command_state, buffer, NULL);
    }
    size_t non_deleted_email_count = 0;
    size_t total_octets = 0;
    for (size_t i = 0; i < pop3_state->emails_count; i += 1)
    {
        if (!pop3_state->emails[i].deleted)
        {
            non_deleted_email_count += 1;
            total_octets += pop3_state->emails[i].octets;
        }
    }
    char *answer = malloc(STAT_OK_MSG_LENGTH);
    snprintf(answer, STAT_OK_MSG_LENGTH, STAT_OK_MSG, non_deleted_email_count, total_octets);
    command_state->answer_alloc = true;
    command_state->answer = answer;
    return handle_simple_command(command_state, buffer, NULL);
}

command_t *handle_listings_command(command_t *command_state, buffer_t buffer, client_info_t *client_state) {
    command_state = copy_command(command_state);
    pop3_client *pop3_state = client_state->pop3_client_info;
    if (command_state->meta_data == NULL) {
        command_state->meta_data = calloc(1, sizeof(list_state_t));
        command_state->free_metadata = (free_metadata_handler)&free_list_state;
        command_state->answer = malloc(MAX_LISTING_SIZE * sizeof(char));
        command_state->index = 0;
        command_state->answer_alloc = true;
    }
    list_state_t* metadata = command_state->meta_data;
    if (command_state->index == 0) {
        if (!metadata->greeting_done) {
            size_t non_deleted_email_count = 0;
            size_t total_octets = 0;
            for (size_t i = 0; i < pop3_state->emails_count; i += 1)
            {
                if (!pop3_state->emails[i].deleted)
                {
                    non_deleted_email_count += 1;
                    total_octets += pop3_state->emails[i].octets;
                }
            }
            metadata->greeting_done = true;
            snprintf(command_state->answer, MAX_LISTING_SIZE, OK_LIST_RESPONSE, non_deleted_email_count, total_octets);
        } else {
            if (metadata->listing_shown == pop3_state->emails_count) {
                snprintf(command_state->answer, MAX_LISTING_SIZE, "%s", SEPARATOR);
                metadata->finish = true;
            } 
            for (size_t i = metadata->listing_shown; i < pop3_state->emails_count; i += 1)
            {
                if (!pop3_state->emails[metadata->listing_shown].deleted)
                {
                    snprintf(command_state->answer, MAX_LISTING_SIZE, LISTING_RESPONSE_FORMAT, i + 1, pop3_state->emails[i].octets);
                    metadata->listing_shown += 1;
                    break;
                } else {
                    metadata->listing_shown += 1;
                }
            }
        }
    }
    command_t *response = handle_simple_response(command_state, buffer, NULL);
    if (command_state->index == 0 && metadata->finish) {
        free_command(command_state);
        return NULL;
    }
    return response;
}

//Funcion que se encarga de listar los emails con los tamaños adecuados, salteando aquellos marcados con DELE
command_t *handle_list_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    pop3_client *pop3_state = client_state->pop3_client_info;
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
        email_metadata_t *email_data = get_email_at_index(pop3_state, index - 1);
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
        command_state->command_handler = &handle_listings_command;
        return handle_listings_command(command_state, buffer, client_state);
    }
}
//Los siguientes comandos son siempre strings estaticos entonces se usa simple command para el handling
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
//Funcion que se encarga de manejar el comando RSET, marcando todos los emails como no borrados
command_t *handle_rset_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    pop3_client *pop3_state = client_state->pop3_client_info;
    if(command_state->answer == NULL){
    
        for (size_t i = 0; i < pop3_state->emails_count; i += 1)
        {
            pop3_state->emails[i].deleted = false;
        }
    }
    return handle_simple_command(command_state, buffer, RSET_MSG);
}
//Funcion que se encarga de manejar el comando DELE, marcando el email como borrado
command_t *handle_dele_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    pop3_client *pop3_state = client_state->pop3_client_info;
    if(command_state->answer != NULL){
        return handle_simple_command(command_state, buffer, NULL);
    }
    if (command_state->args[0] == NULL)
    {
        return handle_simple_command(command_state, buffer, INVALID_NUMBER_ARGUMENT);
    }
    int message_index = atoi(command_state->args[0]);
    if (message_index <= 0)
    {
        return handle_simple_command(command_state, buffer, INVALID_NUMBER_ARGUMENT);
    }
    if ((size_t)message_index > pop3_state->emails_count)
    {
        return handle_simple_command(command_state, buffer, NON_EXISTANT_EMAIL_MSG);
    }
    email_metadata_t *email = &(pop3_state->emails[message_index - 1]);
    char response[MAX_LINE];
    if (email->deleted)
    {
        snprintf(response, MAX_LINE, DELETED_ALREADY_MSG, message_index);
    }
    else
    {
        email->deleted = true;
        snprintf(response, MAX_LINE, DELETED_MSG, message_index);
    }
    return handle_simple_command(command_state, buffer, response);
}
//Funcion que se encarga de liberar el estado de un cliente
void free_pop3_client(pop3_client *client)
{
    if (client->selected_user != NULL) {
        log(INFO, "User: %s logged out\n", client->selected_user->username);
        remove_loggedin_user();
    }
    if (client->current_state & TRANSACTION)
    {//Si estaba en transaction libera el lock del usuario, se hace aca y no en quit por si hay un error
        client->selected_user->locked = false;
    }
    parser_destroy(client->parser_state);
    for (size_t i = 0; i < client->emails_count; i += 1)
    { // Si no inicio sesion emails_count = 0 entonces no entra
        free(client->emails[i].filename);
    }
    free(client->emails);
    if (client->pending_command != NULL && client->pending_command->meta_data != NULL)
    {
        client->pending_command->free_metadata(client->pending_command->meta_data);
    }
    //Libera si esta un comando pendiente
    free_command(client->pending_command);

    free(client);
}

void free_client_pop3(int index)
{
    remove_connection_metric();

    free_pop3_client(sockets[index].client_info.pop3_client_info);
}

// indice empezando de 0
//Funcion auxiliar para manejar el sistema de numeracion de los list y retr
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

void free_retr_state(retr_state_t *metadata)
{
    if (metadata->emailfd != -1)
    {
        close(metadata->emailfd);
        pclose(metadata->email_stream);
    }
    free(metadata);
}

void free_list_state(list_state_t *metadata)
{
    free(metadata);
}