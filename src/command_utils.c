#include "command_utils.h"
#include "logger.h"
#include "string.h"

/*  Simple command siempre es llamado por otro comando, por lo que tenes que tener en cuenta
    el caso en que no termine de escribir y volver a llamar a simple command de forma correcta.
    Adentro se hace un malloc, sirve para poder volver a llamar al comando que llamo a simple command
    para que vuelva a intentar escribir, si se vuelve a llamar se encarga despues de liberar el viejo y
    el nuevo malloc si se termina de escribir en esa llamda.
    Si haces un malloc sobre el answer, pasarlo por state->answer y poner alloc_answer = true.
    Si no usas simple command para escribir, debes hacer un malloc del command_state en c da llamada y liberarlo
    si terminas de escribir, sino deberias copiar al nuevo malloc lo necesario para seguir escribiendo en la siguiente llamada */
command_t *handle_simple_command(command_t *command_state, buffer_t buffer, char *answer)
{
    command_t *command = calloc(1,sizeof(command_t));
    if (command == NULL)
        LOG_AND_RETURN(FATAL, "Error handling command", command);
    if (command_state->answer == NULL)
    {
        if (answer == NULL)
        {
            log(FATAL, "Unexpected error \n%s", "");
        }
        size_t length = strlen(answer);
        command->answer = malloc(length + 1);
        command->answer_alloc = true;
        if (command->answer == NULL)
            LOG_AND_RETURN(FATAL, "Error handling command", command);
        strncpy(command->answer, answer, length + 1);
    }
    else
    {
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
    if (written_bytes >= remaining_bytes_to_write)
    {
        free_command(command);
        return NULL;
    }
    command->index += written_bytes;
    return command;
}

void free_command(command_t *command)
{
    if (command == NULL)
        return;
    free(command->args[0]);
    free(command->args[1]);
    if (command->answer_alloc)
    {
        free(command->answer);
    }
    if(command->meta_data != NULL)
    {
        free(command->meta_data);
    }
    free(command);
}