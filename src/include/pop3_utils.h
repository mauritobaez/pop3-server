#ifndef POP_UTILS_H_
#define POP_UTILS_H_

#include <stdio.h>
#include <stdbool.h>
#include "queue.h"
#include "buffer.h"
#include "parser.h"
#include "server.h"
#include "command_utils.h"

#define POP3_WRITING_BUFFER_SIZE 4096
#define COMMAND_COUNT 10
#define MAX_LINE 4096

// values for state_t
#define AUTH_PRE_USER 0x01
#define AUTH_POST_USER 0x02
#define TRANSACTION 0x04
#define UPDATE 0x08

#define MAX_LISTING_SIZE 64
#define INITIAL_LISTING_COUNT 10

// values for command_type_t
#define INVALID 0
#define NOOP 1
#define USER 2
#define PASS 3
#define QUIT 4
#define STAT 5
#define LIST 6
#define RETR 7
#define DELE 8
#define RSET 9
#define CAPA 10
#define GREETING 11

typedef struct pop3_client pop3_client;

typedef struct retr_state_t
{
    int emailfd;         // file descriptor del email
    int multiline_state; // estado de multilinea. 0 si no recibio nada, 1 si hay un \r, 2 si hay un \n
    bool finished_line;  // indica si termino de escribir la ultima linea leida
    bool greeting_done;  // indica si ya termino el greetng de RETR
    bool final_dot;      // indica si ya se escribio el .CRLF final
    FILE *email_stream;  // file stream del email, usado para cerrarlo despues
} retr_state_t;

typedef struct email_metadata_t
{
    char *filename; // path al archivo
    size_t octets;  // tamano de archivo
    bool deleted;   // archivo si fue borrado por DELE
} email_metadata_t;

typedef struct pop3_client
{
    state_t current_state;       // Estado actual del cliente,
    command_t *pending_command;  // comando a correr
    struct parser *parser_state; // estado de parser. Pide un nuevo comando si no hay comando para correr.
    user_t *selected_user;       // Ptero a struct user en la lista, no hacer free
    email_metadata_t *emails;    // Lista de emails de usuario si se hizo login
    size_t emails_count;         // cantidad de mails cargados
    bool closing;                // Indico si hay que cerrar la conexion
} pop3_client;

// devuelve -1 si hubo un error, loguea el error solo. Maneja una conexion pop3
int handle_pop3_client(void *index, bool can_read, bool can_write);

// devuelve -1 si hubo un error. Acepta nueva conexion de pop3
int accept_pop3_connection(void *index, bool can_read, bool can_write);

// libera info del cliente en el indice del handler.
void free_client_pop3(int index);

// devuelve NULL si no hay email en el indice adecuado o si esta borrado
email_metadata_t *get_email_at_index(pop3_client *state, size_t index);

#endif
