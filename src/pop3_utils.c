#include "pop3_utils.h"

#include <stdbool.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#include "logger.h"
#include "socket_utils.h"

typedef struct command_t command_t;

struct command_t {
    void (*command_handler)(command_t* command_state);
    //FILE* file;
    //command_type_t type;
};

typedef struct command_info
{
    char* name;
    void (*command_handler)(command_t* command_state);
    
} command_info;

command_info commands[COMMAND_COUNT];

int handle_pop3_client(void *index, bool can_read, bool can_write) {

    int sock_index = *(int*) index;
    if (can_write) {
        socket_handler* status = &sockets[sock_index];
        send(status->fd, "hola", 5, 0);
        status->try_write = false;
        log(DEBUG, "closed client %d", status->fd);
        free_client_socket(status->fd);
    }

    //int i = *(int*) index;
    //socket_handler* socket = &sockets[i];
    //|---> commands to process (cola command_t)
    /*
        struct command_t {
            command_type_t (enum) type;
            file* file;
            char answer; = malloc()

            long index;
        }
    */
    if(can_write) {
        /*
            leer del buffer e ir mandando con send() hasta que no pueda más
            if( se vacio el buffer)
                try_write = false;

            proceso todos los comandos que queden
            foreach(comando de commands to process (hago peek)) {
                switch(strmcp(comando, coso))
                    if(hay espacio en buffer)
                        hago dequeue
                        comando_handler();
                    else
                        break;

            }
        */
    }
    if(can_read) {
        /*
            ir leyendo de la entrada con recv y mandarselo al parser
            preguntarle al parser qué comandos enteros hay
            foreach(comando de los eventos del parser) {
                switch(strmcp(comando, coso))
                    if(espacio en buffer > 512)
                        comando_handler();
                    else
                        enqueue(commands to process);

            }

            comando_handler(command_t* command_state) {
                
                //al final
                try_write = true;
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
                sockets[i].try_write = true;
                sockets[i].try_read = false;
                current_socket_count += 1;
                return 0;
            }
        }
        
    }
    return -1;
}