#ifndef POP_UTILS_H_
#define POP_UTILS_H_

#include <stdbool.h>

#define COMMAND_COUNT 10

int handle_pop3_client(void *index, bool can_read, bool can_write);
int accept_pop3_connection(void *index, bool can_read, bool can_write);

#endif