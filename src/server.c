#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <unistd.h>

#include "server.h"
#include "directories.h"
#include "logger.h"

#define PATH_MAX 512
#define TOTAL_ARGUMENTS 7
#define POP3_PORT "1110"
#define PEEP_PORT "2110"
#define DEFAULT_PEEP_ADMIN "root"

server_metrics metrics;
server_config global_config;

int handle_user(int argc, char *arg[], server_config *config);
int handle_mail(int argc, char *arg[], server_config *config);
int handle_peep_admin(int argc, char *arg[], server_config *config);
int handle_pop3_port(int argc, char *arg[], server_config *config);
int handle_peep_port(int argc, char *arg[], server_config *config);
int handle_transform_command(int argc, char *arg[], server_config *config);
int handle_timeout(int argc, char *arg[], server_config *config);

argument_t arguments[TOTAL_ARGUMENTS] = {
    {.argument = "--user", .handler = handle_user, .minified_argument = "-u"},
    {.argument = "--mailbox", .handler = handle_mail, .minified_argument = "-m"},
    {.argument = "--peep-admin", .handler = handle_peep_admin, .minified_argument = "-a"},
    {.argument = "--pop3-port", .handler = handle_pop3_port, .minified_argument = "-p"},
    {.argument = "--peep-port", .handler = handle_peep_port, .minified_argument = NULL},
    {.argument = "--transform", .handler = handle_transform_command, .minified_argument = "-t"},
    {.argument = "--timeout", .handler = handle_timeout, .minified_argument = NULL},
};

// Acepta multiples llamados a --user
int handle_user(int argc, char *arg[], server_config *config)
{
    // Tira error si no hay argumento delante de --user
    if (argc == 0)
    {
        log(FATAL, "No matching property for argument: %s\n", "-u");
        return -1;
    }
    const char *delimiter = ":";
    char *rest = arg[0];
    char *token = strtok_r(rest, delimiter, &rest);
    if (token == NULL)
    {
        log(FATAL, "Format for %s is <user>:<password>\n", "-u");
        return -1;
    }
    user_t *user = malloc(sizeof(user_t));
    user->username = malloc(strlen(token) + 1);
    strcpy(user->username, token);
    token = strtok_r(rest, delimiter, &rest);
    if (token == NULL)
    {
        free(user->username);
        free(user);
        log(FATAL, "Format for %s is <user>:<password>\n", "-u");
        return -1;
    }
    user->password = malloc(strlen(token) + 1);
    strcpy(user->password, token);
    user->locked = false;
    user->removed = false;
    enqueue(config->users, user);
    return 1;
}

int handle_timeout(int argc, char *arg[], server_config *config) {
    if (argc == 0) 
    {
        log(FATAL, "No matching property for argument: %s", "--timeout");
    }
    int timeout = atoi(arg[0]);
    if (timeout < 0) {
        log(FATAL, "Not a valid timeout value: %d", timeout);
    }
    config->timeout = timeout;
    return 1;
}

int handle_transform_command(int argc, char *arg[], server_config *config)
{
    if (argc == 0)
    {
        log(FATAL, "No matching property for argument: %s\n", "--transform");
    }
    size_t command_length = strlen(arg[0]) + 1;
    config->transform_program = malloc(command_length);
    strncpy(config->transform_program, arg[0], command_length);
    return 1;
}

int handle_mail(int argc, char *arg[], server_config *config)
{
    if (argc == 0)
    {
        log(FATAL, "No matching property for argument: %s\n", "-m");
    }
    if (!path_is_directory(arg[0]))
    {
        log(FATAL, "Not a valid path: %s\n", "-m");
    }
    size_t maildir_length = strlen(arg[0]);
    config->maildir = malloc(maildir_length + 1);
    strncpy(config->maildir, arg[0], maildir_length);
    config->maildir[maildir_length] = '\0';
    log(INFO, "Maildir %s\n", arg[0]);
    return 1;
}

int handle_peep_admin(int argc, char *arg[], server_config *config)
{
    if (argc == 0)
    {
        log(FATAL, "No matching property for argument: %s\n", "--peep-admin");
        return -1;
    }
    const char *delimiter = ":";
    char *rest = arg[0];
    char *token = strtok_r(rest, delimiter, &rest);
    if (token == NULL)
    {
        log(FATAL, "Format for %s is <user>:<password>\n", "--peep-admin");
        return -1;
    }
    user_t user;
    user.username = malloc(strlen(token) + 1);
    strcpy(user.username, token);
    token = strtok_r(rest, delimiter, &rest);
    if (token == NULL)
    {
        free(user.username);
        log(FATAL, "Format for %s is <user>:<password>\n", "--peep-admin");
        return -1;
    }
    user.password = malloc(strlen(token) + 1);
    strcpy(user.password, token);
    config->peep_admin.username = user.username;
    config->peep_admin.password = user.password;
    return 1;
}

int handle_pop3_port(int argc, char *arg[], server_config *config)
{
    if (argc == 0)
    {
        log(FATAL, "No matching property for argument: %s\n", "--pop3-port");
    }
    int port = atoi(arg[0]);
    if (port < 0 || port > 65535)
    {
        log(FATAL, "Invalid port: %s\n", arg[0]);
    }
    config->pop3_port = malloc(strlen(arg[0]) + 1);
    strcpy(config->pop3_port, arg[0]);
    return 1;
}

int handle_peep_port(int argc, char *arg[], server_config *config)
{
    if (argc == 0)
    {
        log(FATAL, "No matching property for argument: %s\n", "--peep-port");
    }
    int port = atoi(arg[0]);
    if (port < 0 || port > 65535)
    {
        log(FATAL, "Invalid port: %s\n", arg[0]);
    }
    config->peep_port = malloc(strlen(arg[0]) + 1);
    strcpy(config->peep_port, arg[0]);
    return 1;
}

// Create defaults in case of NULL configs
server_config create_defaults(server_config config)
{
    if (config.maildir == NULL)
    {
        log(INFO, "Setting default maildir\n%s", "");
        config.maildir = malloc(sizeof(char) * PATH_MAX);
        if (getcwd(config.maildir, PATH_MAX) == NULL)
        {
            log(FATAL, "Error reading cwd: %s\n", strerror(errno));
        }
    }
    if (config.peep_admin.password == NULL)
    {
        config.peep_admin.password = malloc(sizeof(DEFAULT_PEEP_ADMIN));
        strcpy(config.peep_admin.password, DEFAULT_PEEP_ADMIN);
    }
    if (config.peep_admin.username == NULL)
    {
        config.peep_admin.username = malloc(sizeof(DEFAULT_PEEP_ADMIN));
        strcpy(config.peep_admin.username, DEFAULT_PEEP_ADMIN);
    }

    if (config.pop3_port == NULL)
    {
        config.pop3_port = malloc(sizeof(POP3_PORT));
        strcpy(config.pop3_port, POP3_PORT);
    }
    if (config.peep_port == NULL)
    {
        config.peep_port = malloc(sizeof(PEEP_PORT));
        strcpy(config.peep_port, PEEP_PORT);
    }
    return config;
}

// De no utilizar --peep-admin, las credenciales serán root:root
server_config get_server_config(int argc, char *argv[])
{
    server_config config = {
        .max_connections = MAX_CONNECTION_LIMIT,
        .timeout = 0,
        .users = create_queue(),
        .maildir = NULL,
        .peep_admin = (user_t){
            .username = NULL,
            .password = NULL},
        .pop3_port = NULL,
        .peep_port = NULL,
        .transform_program = NULL,
    };
    argv = argv + 1;
    argc -= 1;
    while (argc > 0)
    {
        bool found = false;
        for (int i = 0; i < TOTAL_ARGUMENTS && !found; i += 1)
        {
            found = strcmp(argv[0], arguments[i].argument) == 0 || (arguments[i].minified_argument != NULL && strcmp(argv[0], arguments[i].minified_argument) == 0);
            if (found)
            {
                int arguments_consumed = arguments[i].handler(argc - 1, argv + 1, &config);
                argc -= arguments_consumed;
                argv += arguments_consumed;
            }
        }
        if (!found)
        {
            log(FATAL, "%s is not a valid argument\n", argv[0]);
        }
        argv += 1;
        argc -= 1;
    }
    return create_defaults(config);
}

void print_config(server_config config)
{
    log(INFO, "MAX CONNECTIONS %ld, TIMEOUT %ld\n", config.max_connections, config.timeout);
    iterator_to_begin(config.users);

    while (iterator_has_next(config.users))
    {
        user_t *user = iterator_next(config.users);
        log(INFO, "USER: %s, PASSWORD: %s\n", user->username, user->password);
    }
    log(INFO, "MAILDIR: %s\n", config.maildir);
    log(INFO, "POP3-PORT: %s\n", config.pop3_port);
    log(INFO, "PEEP-PORT: %s\n", config.peep_port);
    log(INFO, "PEEP ADMIN: %s:%s\n", config.peep_admin.username, config.peep_admin.password);
}

void free_server_config(server_config config)
{
    while (!is_empty(config.users))
    {
        user_t *user = dequeue(config.users);
        free(user->password);
        free(user->username);
        free(user);
    }
    free(config.users);
    free(config.peep_admin.username);
    free(config.peep_admin.password);
    free(config.maildir);
    free(config.pop3_port);
    free(config.peep_port);
    free(config.transform_program);
}

void start_metrics()
{
    metrics.current_concurrent_pop3_connections = 0;
    metrics.emails_read = 0;
    metrics.emails_removed = 0;
    metrics.successful_quit = 0;
    metrics.sent_bytes = 0;
    metrics.total_pop3_connections = 0;
    metrics.current_loggedin_users = 0;
    metrics.historic_loggedin_users = 0;
}

void add_connection_metric()
{
    metrics.total_pop3_connections += 1;
    metrics.current_concurrent_pop3_connections += 1;
}

void remove_connection_metric()
{
    metrics.current_concurrent_pop3_connections -= 1;
}

void add_sent_bytes(size_t bytes)
{
    metrics.sent_bytes += bytes;
}

void add_email_read()
{
    metrics.emails_read += 1;
}
void add_email_removed()
{
    metrics.emails_removed += 1;
}

void add_successful_quit()
{
    metrics.successful_quit += 1;
}

void add_loggedin_user()
{
    metrics.current_loggedin_users += 1;
    if (metrics.current_loggedin_users > metrics.historic_loggedin_users)
    {
        metrics.historic_loggedin_users = metrics.current_loggedin_users;
    }
}

void remove_loggedin_user()
{
    metrics.current_loggedin_users -= 1;
}
