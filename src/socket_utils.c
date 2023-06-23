#include "socket_utils.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <limits.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "logger.h"
#include "pop3_utils.h"
#include "peep_utils.h"
#include "util.h"

socket_handler sockets[MAX_SOCKETS] = {0};
unsigned int current_socket_count = PASSIVE_SOCKET_COUNT;

// Creo un socket pasivo TCP que acepta tanto IPv6 como IPv4
int setup_passive_socket(char *socket_num)
{
    unsigned port;
    char *end = 0;
    const long sl = strtol(socket_num, &end, 10);
    if (end == socket_num || '\0' != *end || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) || sl < 0 || sl > USHRT_MAX)
    {
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
    if (bind(serv_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        log(FATAL, "bind failed %s\n", strerror(errno));
        goto error;
    }
    if ((listen(serv_sock, MAXPENDING) != 0))
    {
        log(FATAL, "listen failed %s\n", strerror(errno));
        goto error;
    }
    int flags = fcntl(serv_sock, F_GETFL);
    fcntl(serv_sock, F_SETFL, flags | O_NONBLOCK);
    return serv_sock;

error:
    close(serv_sock);
    return -1;
}

// Intento un send a partir del contenido en writing-buffer
int send_from_socket_buffer(int socket_index)
{

    socket_handler *socket = &sockets[socket_index];
    size_t bytes_to_read = buffer_available_chars_count(socket->writing_buffer);
    char message[bytes_to_read + 1];
    message[bytes_to_read] = '\0';

    size_t read_bytes = buffer_read(socket->writing_buffer, message, bytes_to_read);
    ssize_t sent_bytes = send(socket->fd, message, read_bytes, MSG_NOSIGNAL);

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

// La llamada a este tiene que ser precedida por una aseguración de que hay suficientes opciones de conexiones.
socket_handler* find_next_free_socket() {
    for (int i = 0; i < MAX_SOCKETS; i += 1) {
        if (!sockets[i].occupied) {
            return &sockets[i];
        }
    }
    return NULL;
}

// otorga lo recibido del socket al parser.
int recv_to_parser(int socket_index, struct parser *parser, size_t recv_buffer_size)
{
    socket_handler *socket = &sockets[socket_index];
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
            snprintf(error_log, ERROR_LOG_LENGTH, "Socket %d - connection lost\n", socket->fd);
            LOG_AND_RETURN(INFO, error_log, -2);
        }
    }

    log(DEBUG, "Received %ld bytes from socket %d", received_bytes, socket->fd);

    for (int i = 0; i < received_bytes; i += 1)
    {   //Se manda cada caracter que se tenga en el buffer de entrada al parser. 
        //Y cuando se termina un comando lo guarda en la lista de eventos del parser
        if (parser_feed(parser, message[i])->finished)
        {
            finish_event_item(parser);
        }
    }
    free(message);
    return received_bytes;
}

// Libera la data del cliente y despues libera informacion del socket.
void free_client_socket(int socket)
{
    log(DEBUG, "Freeing socket number %d\n", socket);
    for (int i = 0; i < MAX_SOCKETS; i += 1)
    {
        if (sockets[i].occupied && sockets[i].fd == socket)
        {
            if (!sockets[i].passive)
            {
                sockets[i].free_client(i);
            }
            sockets[i].occupied = false;
            if (sockets[i].writing_buffer != NULL)
                buffer_free(sockets[i].writing_buffer);
            current_socket_count -= 1;
            break;
        }
    }
    int ans = close(socket);
    log(DEBUG, "closing socket %d %d %s", socket, ans, strerror(errno));
}

void log_socket(socket_handler socket)
{
    log(DEBUG, "socket: %d, occupied: %d, try_read: %d, try_write: %d", socket.fd, socket.occupied, socket.try_read, socket.try_write);
}
