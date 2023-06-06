#include "socket_utils.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include "logger.h"
#include "util.h"


static char addrBuffer[1000];
socket_handler sockets[MAX_SOCKETS] = {0};
unsigned int current_socket_count = 1;


int setup_passive_socket(char *socket_num)
{
    struct addrinfo addr_criteria;
    memset(&addr_criteria, 0, sizeof(addr_criteria));
    addr_criteria.ai_family = AF_UNSPEC;
    addr_criteria.ai_flags = AI_PASSIVE;
    addr_criteria.ai_socktype = SOCK_STREAM;
    addr_criteria.ai_protocol = IPPROTO_TCP;

    struct addrinfo *serv_addr;
    int ret_val = getaddrinfo(NULL, socket_num, &addr_criteria, &serv_addr);
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

socket_handler* get_socket_handler_with_fd(int fd) {
    for (int i = 0; i < MAX_SOCKETS; i += 1)
    {
        if (sockets[i].fd == fd)
        {
            return &sockets[i];
        }
    }
    log(FATAL, "Socket %d not found\n", fd);
}

socket_handler* get_socket_handler_at_index(unsigned int index) {
    if(index >= MAX_SOCKETS)
        return NULL;
    return &sockets[index];
}

void free_client_socket(int socket) {
    
    for (int i = 0; i < MAX_SOCKETS; i += 1)
    {
        if (sockets[i].fd == socket)
        {
            sockets[i].occupied = false;
            if(sockets[i].pop3_client_info->pending_command != NULL) {
                //TODO: free command
            }
            if(sockets[i].writing_buffer != NULL)
                buffer_free(sockets[i].writing_buffer);
            current_socket_count -= 1;
            break;
        }
    }
    int ans = close(socket);
    log(DEBUG, "closing socket %d %d %s", socket, ans, strerror(errno));
}

void log_socket(socket_handler socket) {
    log(DEBUG, "socket: %d, occupied: %d, try_read: %d, try_write: %d", socket.fd, socket.occupied, socket.try_read, socket.try_write);
}