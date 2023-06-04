#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>

#include "parser.h"
#include "parser_utils.h"
#include "logger.h"
#include "client_status.h"
#include "util.h"

#define MAXPENDING 5
#define POP3_PORT "110"
#define OTHER_PASSIVE_PORT "140"
#define BUFSIZE 1000

#define MAX_SOCKETS 1000

#define COMMAND_COUNT 10
static char addrBuffer[1000];

//TODO: dudoso el array de structs
socket_handler sockets[MAX_SOCKETS] = {0};
unsigned int current_socket_count = 1;

void log_socket(socket_handler socket) {
    log(DEBUG, "socket: %d, occupied: %d, try_read: %d, try_write: %d", socket.fd, socket.occupied, socket.try_read, socket.try_write);
}

typedef struct command_t {
    void (*command_handler)(command_t* command_state);
    FILE* file;
    command_type_t type;
}command_t;

typedef struct command_info
{
    char* name
    void (*command_handler)(command_t* command_state);
    
} command_info;

command_info commands[COMMAND_COUNT] = {};

int handle_pop3_client(void *index, bool can_read, bool can_write) {
    int i = *(int*) index;
    socket_handler* socket = &sockets[i];
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
}

void free_client_socket(int socket) {
    
    for (int i = 0; i < MAX_SOCKETS; i += 1)
    {
        if (sockets[i].fd == socket)
        {
            sockets[i].occupied = false;
            current_socket_count -= 1;
            break;
        }
    }
    int ans = close(socket);
    log(DEBUG, "closing socket %d %d %s", socket, ans, strerror(errno));
}

socket_handler* get_socket_handler(int socket) {
    for (int i = 0; i < MAX_SOCKETS; i += 1)
    {
        if (sockets[i].fd == socket)
        {
            return &sockets[i];
        }
    }
    log(FATAL, "Socket %d not found\n", socket);
}




int setup_passive_socket(char *socket_num)
{
    struct addrinfo addr_criteria;
    memset(&addr_criteria, 0, sizeof(addr_criteria));
    addr_criteria.ai_family = AF_UNSPEC;
    addr_criteria.ai_flags = AI_PASSIVE;
    addr_criteria.ai_socktype = SOCK_STREAM;
    addr_criteria.ai_protocol = IPPROTO_TCP;

    struct addrinfo *serv_addr;
    int ret_val = getaddrinfo(NULL, POP3_PORT, &addr_criteria, &serv_addr);
    if (ret_val != 0)
    {
        log(FATAL, "getaddrinfo() failed %s\n", gai_strerror(ret_val));
        return -1;
    }
    int serv_sock = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol);

    if (serv_sock < 0)
    {
        log(FATAL, "socket() failed %s : %s\n", printAddressPort(&addr_criteria, addrBuffer), strerror(errno));
        freeaddrinfo(serv_addr);
        return -1;
    }

    if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        log(FATAL, "setsockopt failed %s", strerror(errno));
        goto error;
    }

    log(DEBUG, "debug servs_sock %d\n", serv_sock);
    if (bind(serv_sock, serv_addr->ai_addr, serv_addr->ai_addrlen) < 0)
    {
        log(FATAL, "bind failed %s\n", strerror(errno));
        goto error;
    }
    if ((listen(serv_sock, MAXPENDING) != 0))
    {
        log(FATAL, "listen failed %s\n", strerror(errno));
        goto error;
    }

    return serv_sock;
error:
    freeaddrinfo(serv_addr);
    close(serv_sock);
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
                sockets[i].handler = (int (*)(void *, bool, bool)) & handle_tcp_echo_client;
                sockets[i].try_write = true;
                sockets[i].try_read = false;
                current_socket_count += 1;
                return 0;
            }
        }
        
    }
    return -1;
}

int main(int argc, char *argv[])
{
    sockets[0].fd = setup_passive_socket(POP3_PORT);
    //TODO: chequear que funcione para IPv4 e IPv6
    sockets[0].handler = (int (*)(void *, bool, bool)) & accept_pop3_connection;
    sockets[0].occupied = true;
    sockets[0].try_write = false;
    sockets[0].try_read = true;
    
    while (1)
    {
        unsigned int total_poll_fds = current_socket_count;
        log(DEBUG, "current socket-size %d\n", current_socket_count);
        if(total_poll_fds == 0 || total_poll_fds > MAX_SOCKETS) {
            log(FATAL, "unexpected error: invalid socket count (%d)", total_poll_fds);
        }
        struct pollfd pfds[total_poll_fds];
        unsigned socket_index[total_poll_fds];
        

        for (unsigned int j = 0, socket_num = 0; j < MAX_SOCKETS && socket_num < total_poll_fds; j += 1)
        {
            if (sockets[j].occupied)
            {
                socket_index[socket_num] = j;
                pfds[socket_num].fd = sockets[j].fd;
                pfds[socket_num].events = 0;
                if (sockets[j].try_read) {
                    pfds[socket_num].events |= POLLIN;
                }
                if (sockets[j].try_write) {
                    pfds[socket_num].events |= POLLOUT;
                }
                socket_num += 1;
            }
        }

        if (poll(pfds, total_poll_fds, -1) < 0)
        {
            log(ERROR, "Poll failed() %s", strerror(errno));
        }

        for (unsigned int i = 0; i < total_poll_fds; i += 1)
        {
            if (pfds[i].revents == 0)
                continue;

            log(DEBUG, "Socket %d - revents = %d", i, pfds[i].revents);
            
            if (pfds[i].revents & POLLERR || pfds[i].revents & POLLNVAL)
            {
                log(FATAL, "revents error %s\n", strerror(errno));
            }
            if (pfds[i].revents & POLLHUP)
            {
                free_client_socket(pfds[i].fd);
            }
            if (pfds[i].revents & POLLIN || pfds[i].revents & POLLOUT)
            {
                sockets[socket_index[i]].handler(&socket_index[i], pfds[i].revents & POLLIN, pfds[i].revents & POLLOUT);
            }
        }
    }
}

/*
    .handler = &authorization_handler;
    -> .handler = &transaction_handler;
*/

// struct parser_definition parser_def = parser_utils_strcmpi("foo");
// struct parser_definition parser_def2 = parser_utils_strcmpi("bar");

// struct parser *parser_instance = parser_init(parser_no_classes(), &parser_def);
// struct parser *par2 = parser_init(parser_no_classes(), &parser_def2);
// char *text = "bar";

// joined_parser_t unified = join_parsers(2, parser_instance, par2);

// for (int i = 0; i < 3; i += 1)
// {
//     struct parser_event *event = feed_joined_parser(unified, text[i]);

//     print_event(&event[0]);
//     print_event(&event[1]);
//     free(event);
// }

// text = "foo";
// reset_joined_parsers(unified);
// for (int i = 0; i < 3; i += 1)
// {
//     struct parser_event *event = feed_joined_parser(unified, text[i]);

//     print_event(&event[0]);
//     print_event(&event[1]);
//     free(event);
// }
// destroy_joined_parsers(unified);
// parser_utils_strcmpi_destroy(&parser_def);
// parser_utils_strcmpi_destroy(&parser_def2);