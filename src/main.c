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
#define BUFSIZE 1000

#define MAX_SOCKETS 1000

static char addrBuffer[1000];

socket_handler sockets[MAX_SOCKETS] = {0};
int current_socket = 1;

// blocking connection
int handle_tcp_echo_client(int *socket)
{
    log(DEBUG, "closed client %d", *socket);
    for (int i = 0; i < MAX_SOCKETS; i += 1)
    {
        if (sockets[i].fd == *socket)
        {
            sockets[i].occupied = 0;
            current_socket -= 1;
            break;
        }
    }
    // {
    //     char buffer[BUFSIZE]; // Buffer for echo string
    //     // Receive message from client
    //     ssize_t numBytesRcvd = recv(state->socket_state->fd, buffer, BUFSIZE, 0);

    //     if (numBytesRcvd < 0)
    //     {
    //         log(ERROR, "recv() failed", "");
    //         return -1; // TODO definir codigos de error
    //     }

    //     // Send received string and receive again until end of stream
    //     while (numBytesRcvd > 0)
    //     { // 0 indicates end of stream
    //         // Echo message back to client
    //         ssize_t numBytesSent = send(state->socket_state->fd, buffer, numBytesRcvd, 0);
    //         if (numBytesSent < 0)
    //         {
    //             log(ERROR, "send() failed", "");
    //             return -1; // TODO definir codigos de error
    //         }
    //         else if (numBytesSent != numBytesRcvd)
    //         {
    //             log(ERROR, "send() sent unexpected number of bytes ", "");
    //             return -1; // TODO definir codigos de error
    //         }

    //         // See if there is more data to receive
    //         numBytesRcvd = recv(state->sock, buffer, BUFSIZE, 0);
    //         if (numBytesRcvd < 0)
    //         {
    //             log(ERROR, "recv() failed", "");
    //             return -1; // TODO definir codigos de error
    //         }
    //     }

    close(*socket);
    free(socket);
    return 0;
}

int setup_passive_socket()
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
        log(FATAL, "listen failed\n", "");
        goto error;
    }

    return serv_sock;
error:
    freeaddrinfo(serv_addr);
    close(serv_sock);
    return -1;
}

typedef struct accept_data
{
    int server_socket;
} accept_data;

// blocking connection
int accept_pop3_connection(accept_data *accept_data)
{
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int client_socket = accept(accept_data->server_socket, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_socket < 0)
    {
        log(ERROR, "accept failed\n", "");
        return -1;
    }

    int found_free = 0;
    for (int i = 0; i < MAX_SOCKETS && !found_free; i += 1)
    {
        if (!sockets[i].occupied)
        {
            log(DEBUG, "accepted client socket: %d\n", client_socket);
            sockets[i].fd = client_socket;
            sockets[i].occupied = 1;
            sockets[i].handler = (int (*)(void *)) & handle_tcp_echo_client;
            current_socket += 1;
            int *data = malloc(sizeof(int));
            *data = client_socket;
            sockets[i].data = data;
            found_free = 1;
        }
    }
    return client_socket;
}

int main(int argc, char *argv[])
{
    sockets[0].fd = setup_passive_socket();
    accept_data accept_state = {
        .server_socket = sockets[0].fd,
    };

    sockets[0].data = &accept_state;
    sockets[0].handler = (int (*)(void *)) & accept_pop3_connection;
    sockets[0].occupied = 1;
    while (1)
    {
        int npdfs = current_socket;
        log(DEBUG, "current socket-size %d\n", current_socket);
        struct pollfd pdfs[npdfs];

        for (int j = 0, socket_num = 0; j < MAX_SOCKETS; j += 1)
        {
            if (sockets[j].occupied)
            {
                pdfs[socket_num].events = POLLIN | POLLPRI;
                pdfs[socket_num].fd = sockets[j].fd;
                socket_num += 1;
            }
        }

        if (poll(pdfs, npdfs, -1) < 0)
        {
            log(ERROR, "Poll failed()", "");
        }

        for (int i = 0; i < npdfs; i += 1)
        {
            if (pdfs[i].revents == 0)
                continue;
            if (pdfs[i].revents & POLLIN && pdfs[i].revents & POLLOUT)
            {
                log(FATAL, "revents error\n", "");
            }
            if (pdfs[i].revents & POLLIN || pdfs[i].revents & POLLOUT)
            {
                sockets[i].handler(sockets[i].data);
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