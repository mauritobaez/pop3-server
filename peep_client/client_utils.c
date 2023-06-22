#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <stdbool.h>
#include "client_utils.h"
#include "client_messages.h"
#define MAX_ADDR_BUFFER 128

#define TOTAL_ARGUMENTS 4
#define ERROR_COUNT 8

argument_t arguments[TOTAL_ARGUMENTS] = {
    {.argument = "-address", .config = ADDRESS},
    {.argument = "-port", .config = PORT},
    {.argument = "-user", .config = USERNAME},
    {.argument = "-pass", .config = PASSWORD}};
char *error_responses[ERROR_COUNT] = {
    UNKNOWN_COMMAND_RESPONSE,
    INVALID_COMMAND_RESPONSE,
    INVALID_PASSWORD_RESPONSE,
    INVALID_USERNAME_EXISTS_RESPONSE,
    INVALID_USERNAME_NOT_EXISTS_RESPONSE,
    INVALID_MAX_COUNT_RESPONSE,
    INVALID_PATH_RESPONSE,
    INVALID_TIMEOUT_RESPONSE};

char *name_of_config[CONFIG_COUNT] = {
    "address", "port", "user", "pass"};

client_config get_client_config(int argc, char *argv[])
{
    client_config config = {NULL};
    argv = argv + 1;
    argc -= 1;

    if (argc == 1 && strcmp(argv[0], "--help") == 0)
    {
        FATAL_ERROR("Arguments must include: %s %s %s %s\n", name_of_config[0], name_of_config[1], name_of_config[2], name_of_config[3]);
    }

    while (argc >= 2)
    {
        bool found = false;
        for (int i = 0; i < TOTAL_ARGUMENTS && !found; i += 1)
        {
            if ((found = strcmp(argv[0], arguments[i].argument)) == 0)
            {
                config.values[arguments[i].config] = argv[1];
            }
        }
        if (!found)
        {
            FATAL_ERROR("%s is not a valid argument\n", argv[0]);
        }
        argv += 2;
        argc -= 2;
    }
    for (int i = 0; i < CONFIG_COUNT; i += 1)
    {
        if (config.values[i] == NULL)
        {
            FATAL_ERROR("argument %s not set! Use --help in order to see needed arguments\n", name_of_config[i]);
        }
    }
    return config;
}

int setup_tcp_client_socket(const char *host, const char *port)
{

    struct addrinfo addr_criteria;                    // Criteria for address match
    memset(&addr_criteria, 0, sizeof(addr_criteria)); // Zero out structure
    addr_criteria.ai_family = AF_INET6;               // v4 or v6 is OK
    addr_criteria.ai_socktype = SOCK_STREAM;          // Only streaming sockets
    addr_criteria.ai_protocol = IPPROTO_TCP;          // Only TCP protocol
    addr_criteria.ai_flags = AI_NUMERICSERV | AI_V4MAPPED;

    // Get address(es)
    struct addrinfo *server_addr; // Holder for returned list of server addrs
    int rtnVal = getaddrinfo(host, port, &addr_criteria, &server_addr);
    if (rtnVal != 0)
    {
        FATAL_ERROR("getaddrinfo() failed %s", gai_strerror(rtnVal))
        return -1;
    }

    int sock = -1;
    for (struct addrinfo *addr = server_addr; addr != NULL && sock == -1; addr = addr->ai_next)
    {
        // Create a reliable, stream socket using TCP
        sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock >= 0)
        {
            errno = 0;
            // Establish the connection to the server
            if (connect(sock, addr->ai_addr, addr->ai_addrlen) != 0)
            {
                FATAL_ERROR("can't connect %s", strerror(errno));
                close(sock); // Socket connection failed; try next address
                sock = -1;
            }
        }
    }

    freeaddrinfo(server_addr);
    return sock;
}
int handle_server_response(FILE *server_fd, bool last_command_was_multiline)
{
    char first_line[STD_BUFFER_SIZE + 1] = {0};
    int bytes_read_from_server = 0;
    bytes_read_from_server = read(fileno(server_fd), first_line, STD_BUFFER_SIZE);
    if (bytes_read_from_server == EOF)
    {
        return -1;
    }
    first_line[bytes_read_from_server] = '\0';
    bool positive_response = first_line[0] == '+';
    if (positive_response)
    {
        if (!last_command_was_multiline)
        {
            fprintf(stdout, OK_RESPONSE "%s", first_line + 1);
        }
        else
        {
            int lines_to_read = atoi(first_line + 1);
            if (lines_to_read == 0)
            {
                fprintf(stdout, OK_RESPONSE CRLF);
                return 0;
            }
            char *lines = malloc(STD_BUFFER_SIZE * lines_to_read);
            int response_offset = 0;
            while (response_offset < bytes_read_from_server && first_line[response_offset] != '\n')
            {
                response_offset++;
            }
            response_offset++;
            strncpy(lines, first_line + response_offset, bytes_read_from_server);
            int lines_offset = bytes_read_from_server - response_offset;
            int lines_obtained_in_first_read = 0;
            while (response_offset < bytes_read_from_server)
            {
                if (first_line[response_offset - 1] == '\r' && first_line[response_offset] == '\n')
                {
                    lines_obtained_in_first_read++;
                }
                response_offset++;
            }
            if (lines_obtained_in_first_read == lines_to_read)
            {
                lines[lines_offset] = '\0';
                fprintf(stdout, OK_RESPONSE CRLF "%s", lines);
                free(lines);
                return 0;
            }
            else
            {
                bytes_read_from_server = read(fileno(server_fd), lines + lines_offset, STD_BUFFER_SIZE * lines_to_read);
                if (bytes_read_from_server == EOF)
                {
                    free(lines);
                    return -1;
                }
                lines[lines_offset + bytes_read_from_server] = '\0';
                fprintf(stdout, OK_RESPONSE CRLF "%s", lines);
            }
        }
    }
    else
    {
        int error_code = atoi(first_line + 1);
        if (error_code < 0 || error_code >= ERROR_COUNT)
        {
            FATAL_ERROR("invalid error code %d", error_code);
        }
        fprintf(stdout, "ERROR! %s" CRLF, error_responses[error_code]);
    }
    return 0;
}
