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

static char addrBuffer[1000];

// blocking connection
int handle_tcp_echo_client(int clntSocket) {
	char buffer[BUFSIZE]; // Buffer for echo string
	// Receive message from client
	ssize_t numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
	if (numBytesRcvd < 0) {
		log(ERROR, "recv() failed", "");
		return -1;   // TODO definir codigos de error
	}

	// Send received string and receive again until end of stream
	while (numBytesRcvd > 0) { // 0 indicates end of stream
		// Echo message back to client
		ssize_t numBytesSent = send(clntSocket, buffer, numBytesRcvd, 0);
		if (numBytesSent < 0) {
			log(ERROR, "send() failed", "");
			return -1;   // TODO definir codigos de error
		}
		else if (numBytesSent != numBytesRcvd) {
			log(ERROR, "send() sent unexpected number of bytes ", "");
			return -1;   // TODO definir codigos de error
		}

		// See if there is more data to receive
		numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
		if (numBytesRcvd < 0) {
			log(ERROR, "recv() failed", "");
			return -1;   // TODO definir codigos de error
		}
	}

	close(clntSocket);
	return 0;
}

int setup_passive_socket() {
    struct addrinfo addr_criteria;
    memset(&addr_criteria, 0, sizeof(addr_criteria));
    addr_criteria.ai_family = AF_UNSPEC;
    addr_criteria.ai_flags = AI_PASSIVE;
    addr_criteria.ai_socktype = SOCK_STREAM;
    addr_criteria.ai_protocol = IPPROTO_TCP;

    struct addrinfo *serv_addr;
    int ret_val = getaddrinfo(NULL, POP3_PORT, &addr_criteria, &serv_addr);
    if (ret_val != 0) {
        log(FATAL, "getaddrinfo() failed %s\n", gai_strerror(ret_val));
        return -1;
    }
    int serv_sock = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol);

    if (serv_sock < 0) {
        log(FATAL, "socket() failed %s : %s\n", printAddressPort(&addr_criteria, addrBuffer), strerror(errno));
        freeaddrinfo(serv_addr);
        return -1;
    }

    if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        log(FATAL, "setsockopt failed %s", strerror(errno));
        goto error;
    }

    log(DEBUG, "debug servs_sock %d\n", serv_sock);
    if (bind(serv_sock, serv_addr->ai_addr, serv_addr->ai_addrlen) < 0) {
        log(FATAL, "bind failed %s\n", strerror(errno));
        goto error;
    }
    if ((listen(serv_sock, MAXPENDING) != 0)) {
        log(FATAL, "listen failed\n", "");
        goto error;
    }
    
    return serv_sock;
    error:
        freeaddrinfo(serv_addr);
        close(serv_sock);
        return -1;
}

// blocking connection
int accept_pop3_connection(void* serv_sock) {
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int client_socket = accept(*(int *)serv_sock, (struct sockaddr *) &client_addr, &client_addr_len);
    log(DEBUG, "accepted client socket: %d\n", client_socket);

    if (client_socket < 0) {
        log(ERROR, "accept failed\n", "");
        return -1;
    }
    return client_socket;
}

int main(int argc, char *argv[])
{
    socket_handler sockets[1000];
    int current_socket = 1;
    sockets[0].socket = setup_passive_socket();
    sockets[0].data = &sockets[0].socket;
    sockets[0].handler = &accept_pop3_connection;
    struct pollfd *pdfs = calloc(1000, sizeof(struct pollfd));
    if (pdfs == NULL) {
        log(FATAL, "calloc failed", "");
    }
    while (1) {
        for (int j = 0; j < current_socket; j += 1) {
            pdfs[j].events = POLLIN | POLLPRI;
            pdfs[j].fd = sockets[j].socket;
        }

        int ready = poll(pdfs, current_socket, -1);

        if (ready < 0) {
            log(ERROR, "Poll failed()", "");
        }

        for (int i = 0; i < current_socket; i += 1) {
            if (pdfs[i].revents == 0)
                continue;
            if (pdfs[i].revents != POLLIN) {
                log(FATAL, "revents error\n", "");
            }
            sockets[i].handler(sockets[i].data);
        }
    }
}

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