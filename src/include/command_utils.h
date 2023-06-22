#ifndef COMMAND_UTILS_H_
#define COMMAND_UTILS_H_

#include <stdbool.h>
#include "buffer.h"
#include "client_status.h"

typedef uint8_t state_t;
typedef uint8_t command_type_t;

typedef struct command_t command_t;
typedef command_t *(*command_handler)(command_t *, buffer_t, client_info_t *);

struct command_t
{
    // handler para el comando. Este handler es llamado multiples veces en caso de que el buffer no termine de ser escrito. Si el handler es NULL, el handler es terminado
    command_t *(*command_handler)(command_t *command_state, buffer_t buffer, client_info_t *new_state);
    command_type_t type; // tipo de comando
    char *answer;        // string de respuesta. Aca se guarda el string temporal que tiene que ir en el buffer en caso de que el buffer este lleno.
    bool answer_alloc;   // indica si el answer es un malloc o es static memory
    unsigned int index;  // indice del socket handler
    char *args[2];       // argumentos del comando
    void *meta_data;     // estado actual del comando.
};

typedef struct command_info
{
    char *name; // datos estaticos del comando
    // handler a llamar del comando
    command_t *(*command_handler)(command_t *command_state, buffer_t buffer, client_info_t *new_state);
    command_type_t type;  // tipo de comando
    state_t valid_states; // estados en el cual el comando es valido llamarse.
} command_info;

// comando mas basico. escribe al socket el string de answer.
command_t *handle_simple_command(command_t *command_state, buffer_t buffer, char *answer);
void free_command(command_t *command);

#endif
