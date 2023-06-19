#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "client_utils.h"

#define PEEP_PORT_DEFAULT "2110"
#define CRLF "\r\n"

#define MAX_COMMAND_LENGTH 268



int main(int argc, char* argv[]) {
    
    client_config config = get_client_config(argc, argv);

    int socket = setup_tcp_client_socket(config.values[ADDRESS], config.values[PORT]);
    if (socket < 0) {
		FATAL_ERROR("Setting up socket failed: %d",socket);
	}

    FILE* server = fdopen(socket, "r+");

    fprintf(server, "a %s %s\r\n", config.values[USERNAME], config.values[PASSWORD]);

    char answer[32] = {0};

    fgets(answer, 32, server);

    printf("%s:)", answer);
}

