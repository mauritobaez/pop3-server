#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>
#include "client_utils.h"
#define MAX_ADDR_BUFFER 128

#define TOTAL_ARGUMENTS 4

argument_t arguments[TOTAL_ARGUMENTS] = {
    {.argument = "-address", .config = ADDRESS},
    {.argument = "-port", .config = PORT},
    {.argument = "-user", .config = USERNAME},
    {.argument = "-pass", .config = PASSWORD}
};

char* name_of_config[CONFIG_COUNT] = {
    "address", "port", "username", "password"
};

client_config get_client_config (int argc, char* argv[]) {
    client_config config = {NULL};
    argv = argv + 1;
    argc -= 1;
    while (argc >= 2) {
        bool found = false;
        for (int i = 0; i < TOTAL_ARGUMENTS && !found; i += 1) {
            if ((found = strcmp(argv[0], arguments[i].argument) == 0)) {
                config.values[arguments[i].config] = argv[1];
            }
        }
        if (!found) {
            FATAL_ERROR("%s is not a valid argument\n", argv[0]);
        }
        argv += 2;
        argc -= 2;
    }
    for (int i = 0; i < CONFIG_COUNT; i += 1) {
        if (config.values[i] == NULL) {
            FATAL_ERROR("argument %s not set!\n", name_of_config[i]);
        }
    }
    return config;
}


int setup_tcp_client_socket(const char *host,const char *port) {

	
	struct addrinfo addr_criteria;                   // Criteria for address match
	memset(&addr_criteria, 0, sizeof(addr_criteria)); // Zero out structure
	addr_criteria.ai_family = AF_INET6;             // v4 or v6 is OK
	addr_criteria.ai_socktype = SOCK_STREAM;         // Only streaming sockets
	addr_criteria.ai_protocol = IPPROTO_TCP;         // Only TCP protocol
    addr_criteria.ai_flags = AI_NUMERICSERV | AI_V4MAPPED;

	// Get address(es)
	struct addrinfo* server_addr; // Holder for returned list of server addrs
	int rtnVal = getaddrinfo(host, port, &addr_criteria, &server_addr);
	if (rtnVal != 0) {
		FATAL_ERROR("getaddrinfo() failed %s", gai_strerror(rtnVal))
		return -1;
	}

	int sock = -1;
	for (struct addrinfo *addr = server_addr; addr != NULL && sock == -1; addr = addr->ai_next) {
		// Create a reliable, stream socket using TCP
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sock >= 0) {
			errno = 0;
			// Establish the connection to the server
			if ( connect(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
				FATAL_ERROR("can't connect %s", strerror(errno));
				close(sock); 	// Socket connection failed; try next address
				sock = -1;
			}
        }
	}

	freeaddrinfo(server_addr); 
	return sock;
}