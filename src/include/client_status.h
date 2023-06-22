#ifndef CLIENT_STATUS_H
#define CLIENT_STATUS_H

#include <stdbool.h>
#include <time.h>

#include "buffer.h"
#include "parser.h"

// Información particular por cada cliente. Cada socket tiene uno.
typedef union client_info_t
{
    struct pop3_client *pop3_client_info;
    struct peep_client *peep_client_info;
} client_info_t;

typedef struct socket_handler
{
    // fd de socket
    int fd;
    // handler de socket. Selector se encarga de indicar si puede escribir o no
    int (*handler)(void *state, bool can_read, bool can_write);
    // indica si la posicion actual tiene un handler
    bool occupied;
    // try_read y try_write son usados para indicar a poll si necesito escribir o leer o ambos
    bool try_read;
    bool try_write;
    // ultima instancia en el cual el usuario realizo una interaccion
    time_t last_interaction;
    // estado particular del cliente
    client_info_t client_info;
    // buffer de output que se envia por sockett
    buffer_t writing_buffer;
    // metodo de limpieza de cliente
    void (*free_client)(int socket_index);
    // indica si el handler se encarga de aceptar conexiones TCP
    bool passive;
} socket_handler;

#endif
