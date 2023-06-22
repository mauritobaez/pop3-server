#ifndef SOCKET_UTILS_H_
#define SOCKET_UTILS_H_

#include "client_status.h"

#define MAX_SOCKETS 1000
#define MAXPENDING 5
#define PASSIVE_SOCKET_COUNT 2

extern socket_handler sockets[];
extern unsigned int current_socket_count;

int setup_passive_socket(char *socket_num);
int send_from_socket_buffer(int socket_index);
int recv_to_parser(int socket_index, struct parser *parser, size_t recv_buffer_size);
void free_client_socket(int socket);
void log_socket(socket_handler socket);

#endif
