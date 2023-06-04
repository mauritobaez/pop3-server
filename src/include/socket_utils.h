#ifndef SOCKET_UTILS_H_
#define SOCKET_UTILS_H_

#include "client_status.h"

#define MAX_SOCKETS 1000
#define MAXPENDING 5

extern socket_handler sockets[];
extern unsigned int current_socket_count;

int setup_passive_socket(char *socket_num);
socket_handler* get_socket_handler_at_index(unsigned int index);
socket_handler* get_socket_handler_with_fd(int fd);
void free_client_socket(int socket);
void log_socket(socket_handler socket);

#endif