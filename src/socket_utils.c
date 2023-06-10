#include "socket_utils.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>  
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "logger.h"
#include "util.h"


socket_handler sockets[MAX_SOCKETS] = {0};
unsigned int current_socket_count = 1;


int setup_passive_socket(char *socket_num)
{
    unsigned port;
    char *end = 0;
    const long sl = strtol(socket_num, &end, 10);
    if (end == socket_num || '\0' != *end || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) || sl < 0 || sl >USHRT_MAX) {
        log(FATAL, "invalid socket %s\n", socket_num);
    }
    port = sl;

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);


    int serv_sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    if (serv_sock < 0)
    {
        log(FATAL, "socket() failed: %s\n", strerror(errno));
        goto error;
    }

    if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        log(FATAL, "setsockopt failed %s", strerror(errno));
        goto error;
    }

    log(DEBUG, "debug servs_sock %d\n", serv_sock);
    if (bind(serv_sock, (struct sockaddr*) &addr, sizeof(addr)) < 0)
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
            //El free de pop3client se hace en el quit
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