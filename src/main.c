#include <poll.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "socket_utils.h"
#include "pop3_utils.h"
#include "logger.h"




#define POP3_PORT "110"



int main(int argc, char *argv[])
{
    sockets[0].fd = setup_passive_socket(POP3_PORT);
    //TODO: chequear que funcione para IPv4 e IPv6
    sockets[0].handler = (int (*)(void *, bool, bool)) & accept_pop3_connection;
    sockets[0].occupied = true;
    sockets[0].try_write = false;
    sockets[0].try_read = true;
    
    while (1)
    {
        unsigned int total_poll_fds = current_socket_count;
        log(DEBUG, "current socket-size %d\n", current_socket_count);
        if(total_poll_fds == 0 || total_poll_fds > MAX_SOCKETS) {
            log(FATAL, "unexpected error: invalid socket count (%d)", total_poll_fds);
        }
        struct pollfd pfds[total_poll_fds];
        unsigned socket_index[total_poll_fds];
        

        for (unsigned int j = 0, socket_num = 0; j < MAX_SOCKETS && socket_num < total_poll_fds; j += 1)
        {
            if (sockets[j].occupied)
            {
                socket_index[socket_num] = j;
                pfds[socket_num].fd = sockets[j].fd;
                pfds[socket_num].events = 0;
                if (sockets[j].try_read) {
                    pfds[socket_num].events |= POLLIN;
                }
                if (sockets[j].try_write) {
                    pfds[socket_num].events |= POLLOUT;
                }
                socket_num += 1;
            }
        }

        if (poll(pfds, total_poll_fds, -1) < 0)
        {
            log(ERROR, "Poll failed() %s", strerror(errno));
        }

        for (unsigned int i = 0; i < total_poll_fds; i += 1)
        {
            if (pfds[i].revents == 0)
                continue;

            log(DEBUG, "Socket %d - revents = %d", i, pfds[i].revents);
            
            if (pfds[i].revents & POLLERR || pfds[i].revents & POLLNVAL)
            {
                log(FATAL, "revents error %s\n", strerror(errno));
            }
            if (pfds[i].revents & POLLHUP)
            {
                free_client_socket(pfds[i].fd);
            }
            if (pfds[i].revents & POLLIN || pfds[i].revents & POLLOUT)
            {
                sockets[socket_index[i]].handler(&socket_index[i], pfds[i].revents & POLLIN, pfds[i].revents & POLLOUT);
            }
        }
    }
}

/*
    .handler = &authorization_handler;
    -> .handler = &transaction_handler;
*/

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