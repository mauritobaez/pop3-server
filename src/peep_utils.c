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
//Macro para hacer el return de una respuesta negativa con su codigo de error
#define RETURN_NEGATIVE_RESPONSE(command_state, buffer, error_code)        \
    do                                                                     \
    {                                                                      \
        char neg_resp[SHORT_RESP_LENGTH] = {0};                            \
        snprintf(neg_resp, SHORT_RESP_LENGTH, NEG_RESP, (error_code));     \
        return handle_simple_command((command_state), (buffer), neg_resp); \
    } while (0)

//Macro para hacer el return de una respuesta positiva(+)
#define RETURN_POSITIVE_RESPONSE(command_state, buffer) return handle_simple_command((command_state), (buffer), OK)
//Macro para hacer el return de una respuesta positiva con solo un entero
#define RETURN_POSITIVE_RESPONSE_INTEGER(command_state, buffer, integer) \
    do                                                                   \
    {                                                                    \
        char ans[MAX_LINE_LENGTH] = {0};                                 \
        snprintf(ans, MAX_LINE_LENGTH, OK_INT, (integer));               \
        return handle_simple_command((command_state), (buffer), ans);    \
    } while (0)

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
command_t *handle_capa_command(command_t *command_state, buffer_t buffer, client_info_t *client_state);
//Array de comandos validos y sus handlers
command_info peep_commands[PEEP_COMMAND_COUNT] = {
    {.name = "a", .command_handler = (command_handler)&handle_authenticate_command, .type = C_AUTHENTICATE, .valid_states = AUTHENTICATION},
    {.name = "q", .command_handler = (command_handler)&handle_qquit_command, .type = C_QUIT, .valid_states = AUTHENTICATION | AUTHENTICATED},
    {.name = "u+", .command_handler = (command_handler)&handle_add_user_command, .type = C_ADD_USER, .valid_states = AUTHENTICATED},
    {.name = "u-", .command_handler = (command_handler)&handle_delete_user_command, .type = C_DELETE_USER, .valid_states = AUTHENTICATED},
    {.name = "u?", .command_handler = (command_handler)&handle_show_users_command, .type = C_SHOW_USERS, .valid_states = AUTHENTICATED},
    {.name = "c=", .command_handler = (command_handler)&handle_set_max_connections_command, .type = C_SET_MAX_CONNECTIONS, .valid_states = AUTHENTICATED},
    {.name = "c?", .command_handler = (command_handler)&handle_show_max_connections_command, .type = C_SHOW_MAX_CONNECTIONS, .valid_states = AUTHENTICATED},
    {.name = "m=", .command_handler = (command_handler)&handle_set_maildir_command, .type = C_SET_MAILDIR, .valid_states = AUTHENTICATED},
    {.name = "m?", .command_handler = (command_handler)&handle_show_maildir_command, .type = C_SHOW_MAILDIR, .valid_states = AUTHENTICATED},
    {.name = "t=", .command_handler = (command_handler)&handle_set_timeout_command, .type = C_SET_TIMEOUT, .valid_states = AUTHENTICATED},
    {.name = "t?", .command_handler = (command_handler)&handle_show_timeout_command, .type = C_SHOW_TIMEOUT, .valid_states = AUTHENTICATED},
    {.name = "rb?", .command_handler = (command_handler)&handle_show_retrieved_bytes_command, .type = C_SHOW_RETRIEVED_BYTES, .valid_states = AUTHENTICATED},
    {.name = "re?", .command_handler = (command_handler)&handle_show_retrieved_emails_count_command, .type = C_SHOW_RETRIEVED_EMAILS_COUNT, .valid_states = AUTHENTICATED},
    {.name = "xe?", .command_handler = (command_handler)&handle_show_removed_emails_count_command, .type = C_SHOW_REMOVED_EMAILS_COUNT, .valid_states = AUTHENTICATED},
    {.name = "cc?", .command_handler = (command_handler)&handle_show_curr_connection_count_command, .type = C_SHOW_CURR_CONNECTION_COUNT, .valid_states = AUTHENTICATED},
    {.name = "cu?", .command_handler = (command_handler)&handle_show_curr_logged_in_command, .type = C_SHOW_CURR_LOGGED_IN, .valid_states = AUTHENTICATED},
    {.name = "hc?", .command_handler = (command_handler)&handle_show_hist_connection_count_command, .type = C_SHOW_HIST_CONNECTION_COUNT, .valid_states = AUTHENTICATED},
    {.name = "hu?", .command_handler = (command_handler)&handle_show_hist_logged_in_count_command, .type = C_SHOW_HIST_LOGGED_IN_COUNT, .valid_states = AUTHENTICATED},
    {.name = "cap?", .command_handler = (command_handler)&handle_capa_command, .type = C_CAPABILITIES, .valid_states = AUTHENTICATION | AUTHENTICATED}};
//Funcion auxiliar para saber la cant de argumentos presents en el comando
uint8_t get_arg_count(command_t *command_state)
{
    if (command_state->args[0] == NULL)
        return 0;
    if (command_state->args[1] == NULL)
        return 1;
    else
        return 2;
}
//Funcion principal que se encarga de escribir y leer del socket, y de parsear los comandos llamando al handler correspondiente
int handle_peep_client(void *index, bool can_read, bool can_write)
{
    int i = *(int *)index;
    socket_handler *socket = &sockets[i];
    peep_client *peep_client_info = socket->client_info.peep_client_info;

    if (can_write)
    {   //Si hay bytes para enviar, los enviamos hasta lo que nos de
        int remaining_bytes = send_from_socket_buffer(i);
        if (remaining_bytes == -1)
            return -1;
        if (remaining_bytes == -2)
            goto close_peep_client;
        //Si no hay mas bytes para enviar, y el cliente esta cerrando la sesion, cerramos la sesion
        if (buffer_available_chars_count(socket->writing_buffer) == 0 && peep_client_info->closing)
        {
            log(DEBUG, "Socket %d - closing session\n", i);
            goto close_peep_client;
        }

        //Si no hay mas bytes para enviar, y hay un comando pendiente, lo ejecutamos
        if (peep_client_info->pending_command != NULL)
        {
            command_t *old_pending_command = peep_client_info->pending_command;
            peep_client_info->pending_command = old_pending_command->command_handler(old_pending_command, socket->writing_buffer, &(socket->client_info));
            socket->try_write = true;
            free(old_pending_command);
        }
    }

    struct parser *parser = peep_client_info->parser_state;
    //Si podemos leer, leemos y se lo pasamos al parser
    if (can_read && !peep_client_info->closing)
    {
        int ans = recv_to_parser(i, peep_client_info->parser_state, MAX_COMMAND_LENGTH);
        if (ans == -1)
            LOG_AND_RETURN(ERROR, "Error reading from peep client", -1);
        if (ans == -2)
            goto close_peep_client;
    }

    if (peep_client_info->pending_command == NULL && !peep_client_info->closing)
    {   //Si no habia un comando pendiente y el cliente no esta cerrando la sesion, agarramos el ultimo comando parseado y buscamos un match en el array
        struct parser_event *event = get_last_event(parser);
        if (event != NULL)
        {
            bool found_command = false;
            if (event->args[0] != NULL)
            {
                for (int command_index = 0; command_index < PEEP_COMMAND_COUNT && !found_command; command_index++)
                {   //Revisamos que el comando este en el estado correcto y que el nombre del comando sea correcto
                    if (((peep_commands[command_index].valid_states & peep_client_info->state) > 0) && strcmp(event->args[0], peep_commands[command_index].name) == 0)
                    {
                        found_command = true;
                        command_t command_state = {
                            .type = peep_commands[command_index].type,
                            .command_handler = peep_commands[command_index].command_handler,
                            .args[0] = event->args[1],
                            .args[1] = event->args[2],
                            .answer = NULL,
                            .answer_alloc = false,
                            .index = 0,
                            .meta_data = NULL,
                            .free_metadata = NULL,
                            };

                        log(DEBUG, "Command %s received with args %s and %s\n", event->args[0], event->args[1], event->args[2]);
                        //Llamamos al handler del comando y si no termino se lo asignamos como comando pendiente
                        peep_client_info->pending_command = command_state.command_handler(&command_state, socket->writing_buffer, &(socket->client_info));
                        socket->try_write = true;
                        free_event(event, false);
                    }
                }
            }
            //Si no encontramos el comando, mandamos un comando desconocido
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
                    .free_metadata = NULL,
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
    return -1;
}
//Funcion que se encarga de aceptar una conexion de un cliente peep
int accept_peep_connection(void *index, bool can_read, bool can_write)
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
            LOG_AND_RETURN(ERROR, "Error accepting peep connection", 0);
        }

        socket_handler *free_socket = find_next_free_socket();
        free_socket->fd = client_socket;
        free_socket->occupied = true;
        free_socket->handler = (int (*)(void *, bool, bool)) & handle_peep_client;
        free_socket->try_read = true;
        free_socket->try_write = false;
        free_socket->free_client = &free_peep_client;
        free_socket->client_info.peep_client_info = malloc(sizeof(peep_client));
        free_socket->client_info.peep_client_info->state = AUTHENTICATION;
        free_socket->client_info.peep_client_info->pending_command = NULL;
        free_socket->client_info.peep_client_info->parser_state = set_up_parser();
        free_socket->client_info.peep_client_info->closing = false;
        free_socket->writing_buffer = buffer_init(PEEP_WRITING_BUFFER_SIZE);
        free_socket->last_interaction = 0;
        free_socket->passive = false;
        current_socket_count += 1;
        return 1;
    }
    return 0;
}
//Funcion que se encarga de liberar la memoria de un cliente peep
void free_peep_client(int index)
{
    peep_client *client = sockets[index].client_info.peep_client_info;
    parser_destroy(client->parser_state);
    free_command(client->pending_command);
    free(client);
}
//Funcion que se encarga de manejar un comando desconocido
command_t *handle_unknown_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    RETURN_NEGATIVE_RESPONSE(command_state, buffer, 0);
}

// a
command_t *handle_authenticate_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    if (get_arg_count(command_state) < 2)
    {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }
    if (strcmp(command_state->args[0], global_config.peep_admin.username) == 0 && strcmp(command_state->args[1], global_config.peep_admin.password) == 0)
    {
        client_state->peep_client_info->state = AUTHENTICATED;
        RETURN_POSITIVE_RESPONSE(command_state, buffer);
    }
    else
    {
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
    if (get_arg_count(command_state) < 1)
    {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }
    int new_max_connections = atoi(command_state->args[0]);
    if (new_max_connections < MIN_CONNECTION_LIMIT || new_max_connections > MAX_CONNECTION_LIMIT)
    {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 5);
    }
    global_config.max_connections = new_max_connections;
    return handle_simple_command(command_state, buffer, OK);
}
//Funcion auxiliar que se encarga de ver si existe un usuario y lo retorna
user_t *check_user_exists(char *username)
{
    queue_t user_list = global_config.users;
    if (username != NULL)
    {
        iterator_to_begin(user_list);
        while (iterator_has_next(user_list))
        {
            user_t *user = iterator_next(user_list);
            if (strcmp(user->username, username) == 0)
                return user;
        }
    }
    return NULL;
}

// u+
command_t *handle_add_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    if (get_arg_count(command_state) < 2)
    {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }

    char *username = command_state->args[0];
    char *password = command_state->args[1];
    user_t *user;
    if ((user = check_user_exists(username)) != NULL)
    {
        if (user->removed == false)
            RETURN_NEGATIVE_RESPONSE(command_state, buffer, 3);
        free(user->password);
    }
    else
    {
        user = malloc(sizeof(user_t));
        user->username = malloc(strlen(username) + 1);
        strcpy(user->username, username);
        user->locked = false;
        enqueue(global_config.users, user);
    }

    user->password = malloc(strlen(password) + 1);
    strcpy(user->password, password);
    user->removed = false;

    return handle_simple_command(command_state, buffer, OK);
}

// u-
command_t *handle_delete_user_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    if (get_arg_count(command_state) < 1)
    {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }

    user_t *user;
    if ((user = check_user_exists(command_state->args[0])) != NULL)
    {
        user->removed = true;
        return handle_simple_command(command_state, buffer, OK);
    }

    RETURN_NEGATIVE_RESPONSE(command_state, buffer, 4);
}

// u?
command_t *handle_show_users_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    if (command_state->answer != NULL) {
        return handle_simple_command(command_state, buffer, NULL);
    }
    queue_t list = global_config.users;
    size_t user_count = 0;
    iterator_to_begin(list);
    while (iterator_has_next(list))
    {
        user_t *user = iterator_next(list);
        user_count += !user->removed;
    }
    char *ans = malloc(MAX_LINE_LENGTH * (user_count + 1));
    command_state->answer_alloc = true;
    int index = snprintf(ans, MAX_LINE_LENGTH, OK_INT, user_count);
    iterator_to_begin(list);
    while (iterator_has_next(list))
    {
        user_t *user = iterator_next(list);
        if (!user->removed)
        {
            index += snprintf(ans + index, MAX_LINE_LENGTH, LINE_RESP, user->username);
        }
    }
    command_state->answer = ans;
    return handle_simple_command(command_state, buffer, NULL);
}

// c?
command_t *handle_show_max_connections_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    size_t response = global_config.max_connections;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state, buffer, response);
}

// m=
command_t *handle_set_maildir_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    if (get_arg_count(command_state) < 1)
    {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }
    if (!path_is_directory(command_state->args[0]))
    {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 6);
    }
    size_t length = strlen(command_state->args[0]) + 1;
    char *maildir = malloc(length);
    strncpy(maildir, command_state->args[0], length);
    free(global_config.maildir);
    global_config.maildir = maildir;
    return handle_simple_command(command_state, buffer, OK);
}

// m?
command_t *handle_show_maildir_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    char *response = global_config.maildir;
    char ans[MAX_LINE_LENGTH] = {0};
    snprintf(ans, MAX_LINE_LENGTH, OK_STRING, response);
    return handle_simple_command(command_state, buffer, ans);
}

// t=
command_t *handle_set_timeout_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    if (get_arg_count(command_state) < 1)
    {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 1);
    }
    int new_timeout = atoi(command_state->args[0]);
    if (new_timeout < MIN_TIMEOUT_VALUE || new_timeout > MAX_TIMEOUT_VALUE)
    {
        RETURN_NEGATIVE_RESPONSE(command_state, buffer, 7);
    }
    global_config.timeout = new_timeout;
    return handle_simple_command(command_state, buffer, OK);
}

// t?
command_t *handle_show_timeout_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    size_t response = global_config.timeout;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state, buffer, response);
}

// rb?
command_t *handle_show_retrieved_bytes_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    size_t response = metrics.sent_bytes;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state, buffer, response);
}

// re?
command_t *handle_show_retrieved_emails_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    size_t response = metrics.emails_read;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state, buffer, response);
}

// xe?
command_t *handle_show_removed_emails_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    size_t response = metrics.emails_removed;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state, buffer, response);
}

// cc?
command_t *handle_show_curr_connection_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    size_t response = metrics.current_concurrent_pop3_connections;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state, buffer, response);
}

// cu?
command_t *handle_show_curr_logged_in_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    queue_t list = global_config.users;
    size_t user_count = 0;
    if(command_state->answer != NULL){
        return handle_simple_command(command_state, buffer, NULL);
    }
    iterator_to_begin(list);
    while (iterator_has_next(list))
    {
        user_t *user = iterator_next(list);
        user_count += !user->removed && user->locked;
    }
    char *ans = malloc(MAX_LINE_LENGTH * (user_count + 1));
    command_state->answer_alloc = true;
    int index = snprintf(ans, MAX_LINE_LENGTH, OK_INT, user_count);
    iterator_to_begin(list);
    while (iterator_has_next(list))
    {
        user_t *user = iterator_next(list);
        if (!user->removed && user->locked)
        {
            index += snprintf(ans + index, MAX_LINE_LENGTH, LINE_RESP, user->username);
        }
    }
    command_state->answer = ans;
    return handle_simple_command(command_state, buffer, NULL);
}

// hc?
command_t *handle_show_hist_connection_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    size_t response = metrics.total_pop3_connections;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state, buffer, response);
}

// hu?
command_t *handle_show_hist_logged_in_count_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    size_t response = metrics.historic_loggedin_users;
    RETURN_POSITIVE_RESPONSE_INTEGER(command_state, buffer, response);
}

// cap?
command_t *handle_capa_command(command_t *command_state, buffer_t buffer, client_info_t *client_state)
{
    return handle_simple_command(command_state, buffer, PEEP_CAPA);
}
