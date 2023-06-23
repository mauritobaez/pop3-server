#include <poll.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#include "socket_utils.h"
#include "pop3_utils.h"
#include "peep_utils.h"
#include "logger.h"
#include "server.h"
#include "directories.h"

static bool done = false;


static void sigchild_handler(const int signal)
{
    int status;
    waitpid(-1, &status, WNOHANG);
}

static void
sigterm_handler(const int signal)
{
    log(INFO, "signal %d, cleaning up and exiting\n", signal);
    done = true;
}

int main(int argc, char *argv[])
{
    // No usamos stdin
    close(STDIN_FILENO);

    // Comenzamos el sistema de metricas
    start_metrics();

    // Obtenemos las configuraciones de servers
    global_config = get_server_config(argc, argv);
    print_config(global_config);

    // Signals
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);
    signal(SIGCHLD, sigchild_handler);

    // signal(SIGCHLD, sigchild_handler);

    // Setupeamos los handlers de sockets pasivos.
    sockets[0].fd = setup_passive_socket(global_config.pop3_port);
    sockets[0].handler = (int (*)(void *, bool, bool)) & accept_pop3_connection;
    sockets[0].occupied = true;
    sockets[0].try_write = false;
    sockets[0].try_read = true;
    sockets[0].last_interaction = 0;
    sockets[0].passive = true;
    sockets[1].fd = setup_passive_socket(global_config.peep_port);
    sockets[1].handler = (int (*)(void *, bool, bool)) & accept_peep_connection;
    sockets[1].occupied = true;
    sockets[1].try_write = false;
    sockets[1].try_read = true;
    sockets[1].last_interaction = 0;
    sockets[1].passive = true;

    char *debug_revents[] = {"NONE", "POLLIN", "POLLPRI", 0,
                             "POLLOUT", 0, 0, 0,
                             "POLLERR", 0, 0, 0,
                             0, 0, 0, 0,
                             "POLLHUP", 0, 0, 0,
                             0, 0, 0, 0,
                             0, 0, 0, 0,
                             0, 0, 0, 0,
                             "POLLNVAL"};

    while (!done)
    {
        unsigned int total_poll_fds = current_socket_count;
        log(DEBUG, "current socket-count: %d\n", current_socket_count);
        if (total_poll_fds == 0 || total_poll_fds > MAX_SOCKETS)
        {
            log(FATAL, "unexpected error: invalid socket count (%d)", total_poll_fds);
        }
        // File descriptors a monitorear
        struct pollfd pfds[total_poll_fds];

        // Socket index a la cual corresponde el filedescriptor monitoreado
        unsigned socket_index[total_poll_fds];

        time_t now = time(NULL);
        // Itero por el array de sockets buscando file descriptors. Debido a que es un array estatico, pueden haber espacios en el array
        for (unsigned int j = 0, socket_num = 0; j < MAX_SOCKETS && socket_num < total_poll_fds; j += 1)
        {
            if (sockets[j].occupied)
            {
                // Desconecto el socket por timeout dependiendo de su ultimo tiempo de interacción.
                if (global_config.timeout > 0 && sockets[j].last_interaction != 0 && sockets[j].last_interaction + (time_t)global_config.timeout < now)
                {
                    log(DEBUG, "Closing connection of client %d due to exceeded timeout\n", j);
                    free_client_socket(sockets[j].fd);
                    total_poll_fds--;
                }
                else
                {
                    socket_index[socket_num] = j;
                    pfds[socket_num].fd = sockets[j].fd;
                    pfds[socket_num].events = 0;
                    // Si el socket es pasivo y llegamos a la cantidad de conexiones maxima, no monitorearlos
                    if (!(sockets[j].passive && (current_socket_count - PASSIVE_SOCKET_COUNT) >= global_config.max_connections))
                    {
                        // handler de socket decide si esta esperando lectura o escritura.
                        if (sockets[j].try_read)
                        {
                            pfds[socket_num].events |= POLLIN;
                        }
                        if (sockets[j].try_write)
                        {
                            pfds[socket_num].events |= POLLOUT;
                        }
                    }
                    socket_num += 1;
                }
            }
        }

        if (poll(pfds, total_poll_fds, global_config.timeout == 0 ? -1 : (long)global_config.timeout * 1000) < 0)
        {
            log(INFO, "poll error %s", strerror(errno));
        }

        now = time(NULL);
        for (unsigned int i = 0; i < total_poll_fds; i += 1)
        {
            if (pfds[i].revents == 0)
                continue;

            log(DEBUG, "%s Socket %d (of fd %d) - revents = %s", sockets[socket_index[i]].passive ? "Passive" : "Active", i, pfds[i].fd, debug_revents[pfds[i].revents]);

            if (pfds[i].revents & POLLERR || pfds[i].revents & POLLNVAL)
            {
                log(ERROR, "revents error %s\n", strerror(errno));
            }
            // Si al momento de monitorear se desconecto la conexion, libero el socket de cliente.
            if (pfds[i].revents & POLLHUP)
            {
                log(ERROR, "user hangup %s\n", strerror(errno));
                free_client_socket(pfds[i].fd);
            }
            else if (pfds[i].revents & POLLIN || pfds[i].revents & POLLOUT)
            {
                if (sockets[socket_index[i]].last_interaction != 0)
                {
                    sockets[socket_index[i]].last_interaction = now;
                }
                if (sockets[socket_index[i]].handler(&socket_index[i], pfds[i].revents & POLLIN, pfds[i].revents & POLLOUT) == -1)
                {
                    free_client_socket(pfds[i].fd);
                }
            }
        }
    }

    log(INFO, "%s", "Shutting down server\n");
    for (unsigned int i = 0; i < MAX_SOCKETS; i++)
    {
        if (sockets[i].occupied)
        {
            free_client_socket(sockets[i].fd);
        }
    }
    free_server_config(global_config);
}
