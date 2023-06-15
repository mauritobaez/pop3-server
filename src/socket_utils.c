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
unsigned int current_socket_count = 2;


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

int send_from_socket_buffer(int socket_index) {

    socket_handler *socket = &sockets[socket_index];
    size_t bytes_to_read = buffer_available_chars_count(socket->writing_buffer);
    char *message = malloc(bytes_to_read + 1);
    if (message == NULL)
        LOG_AND_RETURN(ERROR, "Error writing to peep client", -1);
    message[bytes_to_read] = '\0';

    size_t read_bytes = buffer_read(socket->writing_buffer, message, bytes_to_read);
    ssize_t sent_bytes = send(socket->fd, message, read_bytes, MSG_NOSIGNAL);

    free(message);
    if (sent_bytes < 0)
    {
        char error_log[ERROR_LOG_LENGTH] = {0};
        snprintf(error_log, ERROR_LOG_LENGTH, "Socket %d - unknown error\n", socket_index);
        LOG_AND_RETURN(ERROR, error_log, -2);
    }

    buffer_advance_read(socket->writing_buffer, sent_bytes);

    size_t remaining_bytes = buffer_available_chars_count(socket->writing_buffer);

    // si ya vaciamos todo el buffer de salida, le decimos al selector que no nos despierte para escribir
    if (remaining_bytes == 0)
    {
        socket->try_write = false;
    }
    return sent_bytes;
}

int recv_to_parser(int socket_index, struct parser* parser, size_t recv_buffer_size) {
    socket_handler* socket = &sockets[socket_index];
    char *message = malloc(sizeof(char) * recv_buffer_size);
    if (message == NULL)
        return -1;
    ssize_t received_bytes = recv(socket->fd, message, recv_buffer_size, 0);
    // Si recv devuelve 0 es porque se desconecto.
    if (received_bytes <= 0)
    {
        free(message);
        if (received_bytes < 0)
            return -1;
        else
        {
            char error_log[ERROR_LOG_LENGTH] = {0};
            snprintf(error_log, ERROR_LOG_LENGTH, "Socket %d - connection lost\n", socket_index);
            LOG_AND_RETURN(INFO, error_log, -2);
        }
    }

    log(DEBUG, "Received %ld bytes from socket %d", received_bytes, socket->fd);

    for (int i = 0; i < received_bytes; i += 1)
    {
        if (parser_feed(parser, message[i])->finished)
        {
            finish_event_item(parser);
        }
    }
    free(message);
    return received_bytes;
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