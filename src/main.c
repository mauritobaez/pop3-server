#include <poll.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "socket_utils.h"
#include "pop3_utils.h"
#include "logger.h"
#include "server.h"

#define POP3_PORT "1110"

server_config global_config;

static bool done = false;

static void
sigterm_handler(const int signal) {
    log(INFO, "signal %d, cleaning up and exiting\n",signal);
    done = true;
}


int main(int argc, char *argv[])
{
    global_config = get_server_config(argc, argv);
    print_config(global_config);

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);
    sockets[0].fd = setup_passive_socket(POP3_PORT);
    //TODO: chequear que funcione para IPv4 e IPv6
    sockets[0].handler = (int (*)(void *, bool, bool)) & accept_pop3_connection;
    sockets[0].occupied = true;
    sockets[0].try_write = false;
    sockets[0].try_read = true;
    
    while (!done)
    {
        unsigned int total_poll_fds = current_socket_count;
        log(DEBUG, "current socket-count: %d\n", current_socket_count);
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

            char* debug_revents[] = {   "NONE",     "POLLIN",   "POLLPRI",  0, 
                                        "POLLOUT",  0,          0,          0,
                                        "POLLERR",  0,          0,          0,
                                        0,          0,          0,          0,
                                        "POLLHUP",  0,          0,          0,
                                        0,          0,          0,          0,
                                        0,          0,          0,          0,
                                        0,          0,          0,          0,
                                        "POLLNVAL"};

            log(DEBUG, "Socket %d - revents = %s", i, debug_revents[pfds[i].revents]);
            
            if (pfds[i].revents & POLLERR || pfds[i].revents & POLLNVAL)
            {
                log(FATAL, "revents error %s\n", strerror(errno));
            }
            if (pfds[i].revents & POLLHUP)
            {
                log(ERROR, "user hangup %s\n", strerror(errno));
                free_client_socket(pfds[i].fd);
            } else if (pfds[i].revents & POLLIN || pfds[i].revents & POLLOUT)
            {
                if (sockets[socket_index[i]].handler(&socket_index[i], pfds[i].revents & POLLIN, pfds[i].revents & POLLOUT) == -1) {
                    free_client_socket(pfds[i].fd);
                }
            }
        }
    }
}