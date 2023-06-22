#ifndef SERVER_H
#define SERVER_H

#include <stdlib.h>
#include "queue.h"

#define MIN_TIMEOUT_VALUE 0
#define MAX_TIMEOUT_VALUE 86400

#define MIN_CONNECTION_LIMIT 1
#define MAX_CONNECTION_LIMIT 998 // 1000 - 2

typedef struct server_metrics
{
    size_t total_pop3_connections;
    size_t current_concurrent_pop3_connections;
    size_t sent_bytes;
    size_t emails_read;
    size_t emails_removed;
    size_t successful_quit;
    size_t current_loggedin_users;
    size_t historic_loggedin_users;
} server_metrics;

typedef struct user_t
{
    char *username;
    char *password;
    int locked;
    bool removed;
} user_t;

typedef struct server_config
{
    size_t max_connections;
    size_t timeout;
    queue_t users;
    user_t peep_admin;
    char *pop3_port;
    char *peep_port;
    char *maildir;
    char *transform_program;
} server_config;

// handler de argumento. Despues de matchear por string, consume n cantidad de argumentos. Devuelve los argumentos consumidos.
typedef int (*handle_argument)(int argc, char *arg[], server_config *config);

typedef struct argument_t
{
    char *argument;          // nombre de argumento
    char *minified_argument; // shorthand de argumento
    handle_argument handler; // handler del argumento
} argument_t;

extern server_config global_config;

server_config get_server_config(int argc, char *argv[]);
void free_server_config(server_config config);
void print_config(server_config config);

extern server_metrics metrics;

/*
    Funciones para actualizar las metricas.
*/
void start_metrics();
void add_connection_metric();
void remove_connection_metric();
void add_sent_bytes(size_t bytes);
void add_email_read();
void add_email_removed();
void add_successful_quit();
void add_loggedin_user();
void remove_loggedin_user();

#endif
