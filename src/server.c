#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>

#include "server.h"
#include "directories.h"
#include "logger.h"

/*
-u user:pass -u user:pass
-m <maildir>
*/

#define PATH_MAX 512
#define TOTAL_ARGUMENTS 6
#define POP3_PORT "1110"
#define PEEP_PORT "2110"


server_metrics metrics;
server_config global_config;

int handle_user(int argc, char *arg[], server_config* config);
int handle_mail(int argc, char *arg[], server_config* config);
int handle_peep_admin(int argc, char *arg[], server_config* config);
int handle_pop3_port(int argc, char *arg[], server_config* config);
int handle_peep_port(int argc, char *arg[], server_config* config);
int handle_transform_command(int argc, char *arg[], server_config* config);

argument_t arguments[TOTAL_ARGUMENTS] = {
    {.argument = "-u", .handler = handle_user},
    {.argument = "-m", .handler = handle_mail},
    {.argument = "--peep-admin", .handler = handle_peep_admin},
    {.argument = "--pop3-port", .handler = handle_pop3_port},
    {.argument = "--peep-port", .handler = handle_peep_port},
    {.argument = "--transform", .handler = handle_transform_command}
};

int handle_user(int argc, char *arg[], server_config* config) {
    if (argc == 0) {
        log(FATAL, "No matching property for argument: %s\n", "-u");
    }
    const char *delimiter = ":";
    char *token = strtok(arg[0], delimiter);
    if (token == NULL) {
        log(FATAL, "Format for %s is <user>:<password>\n", "-u");
    }
    user_t* user = malloc(sizeof(user_t));
    user->username = malloc(strlen(token) + 1);
    strcpy(user->username, token);
    token = strtok(NULL, delimiter);
    if (token == NULL) {
        free(user->username);
        free(user);
        log(FATAL, "Format for %s is <user>:<password>\n", "-u");
    }
    user->password = malloc(strlen(token) + 1);
    strcpy(user->password, token);
    user->locked = false;
    user->removed = false;
    enqueue(config->users, user);
    return 1;
}

int handle_transform_command(int argc, char *arg[], server_config* config) {
    if (argc == 0) {
        log(FATAL, "No matching property for argument: %s\n", "--transform");
    }
    size_t command_length = strlen(arg[0]) + 1;
    config->transform_program = malloc(command_length);
    strncpy(config->transform_program, arg[0], command_length);
    return 1;
}

int handle_mail(int argc, char *arg[], server_config* config) {
    if (argc == 0) {
        log(FATAL, "No matching property for argument: %s\n", "-m");
    }
    if (!path_is_directory(arg[0])) {
        log(FATAL, "Not a valid path: %s\n", "-m");
    }
    size_t maildir_length = strlen(arg[0]);
    config->maildir = malloc(maildir_length + 1);
    strncpy(config->maildir, arg[0], maildir_length);
    config->maildir[maildir_length] = '\0';
    return 1;
}

int handle_peep_admin(int argc, char *arg[], server_config* config) {
    if (argc == 0) {
        log(FATAL, "No matching property for argument: %s\n", "-a");
    }
    const char *delimiter = ":";
    char *token = strtok(arg[0], delimiter);
    if (token == NULL) {
        log(FATAL, "Format for %s is <user>:<password>\n", "-a");
    }
    user_t user;
    user.username = malloc(strlen(token) + 1);
    strcpy(user.username, token);
    token = strtok(NULL, delimiter);
    if (token == NULL) {
        free(user.username);
        log(FATAL, "Format for %s is <user>:<password>\n", "-a");
    }
    user.password = malloc(strlen(token) + 1);
    strcpy(user.password, token);
    free(config->peep_admin.username);
    free(config->peep_admin.password);
    config->peep_admin.username = user.username;
    config->peep_admin.password = user.password;
    return 1;
}
int handle_pop3_port(int argc, char *arg[], server_config* config) {
    if (argc == 0) {
        log(FATAL, "No matching property for argument: %s\n", "--pop3-port");
    }
    int port = atoi(arg[0]);
    if (port < 0 || port > 65535) {
        log(FATAL, "Invalid port: %s\n", arg[0]);
    }
    free(config->pop3_port);
    config->pop3_port = malloc(strlen(arg[0]) + 1);
    strcpy(config->pop3_port, arg[0]);
    return 1;
}
int handle_peep_port(int argc, char *arg[], server_config* config) {
    if (argc == 0) {
        log(FATAL, "No matching property for argument: %s\n", "--peep-port");
    }
    int port = atoi(arg[0]);
    if (port < 0 || port > 65535) {
        log(FATAL, "Invalid port: %s\n", arg[0]);
    }
    free(config->peep_port);
    config->peep_port = malloc(strlen(arg[0]) + 1);
    strcpy(config->peep_port, arg[0]);
    return 1;
}

server_config create_defaults(server_config config) {
    if (config.maildir == NULL) {
        config.maildir = malloc(sizeof(char) * PATH_MAX);
        if (getcwd(config.maildir, PATH_MAX) == NULL) {
            log(FATAL, "Error reading cwd: %s\n", strerror(errno));
        }
    }
    return config;
}

// De no utilizar --peep-admin, las credenciales serán root:root
server_config get_server_config(int argc, char *argv[]) {
    char* default_peep_admin = "root";
    unsigned int length = strlen(default_peep_admin) + 1;
    unsigned int pop3_port_length = strlen(POP3_PORT) + 1;
    unsigned int peep_port_length = strlen(PEEP_PORT) + 1;
    server_config config = {
        .max_connections = MAX_CONNECTION_LIMIT,
        .timeout = 0,
        .users = create_queue(),
        .maildir = NULL,
        .peep_admin = (user_t) {
            .username = malloc(length),
            .password = malloc(length)
        },
        .pop3_port = malloc(pop3_port_length),
        .peep_port = malloc(peep_port_length),
        .transform_program = NULL,
    };
    strncpy(config.peep_admin.username, default_peep_admin, length);
    strncpy(config.peep_admin.password, default_peep_admin, length);
    strncpy(config.pop3_port, POP3_PORT, pop3_port_length);
    strncpy(config.peep_port, PEEP_PORT, peep_port_length);
    // ignoro nombre de programa
    argv = argv + 1;
    argc -= 1;
    while (argc > 0) {
        bool found = false;
        for (int i = 0; i < TOTAL_ARGUMENTS && !found; i += 1) {
            if ((found = strcmp(argv[0], arguments[i].argument) == 0)) {
                int arguments_consumed = arguments[i].handler(argc - 1, argv + 1, &config);
                argc -= arguments_consumed;
                argv += arguments_consumed;
            }
        }
        if (!found) {
            log(FATAL, "%s is not a valid argument\n", argv[0]);
        }
        argv += 1;
        argc -= 1;
    }
    return create_defaults(config);
}

void print_config(server_config config) {
    log(INFO, "MAX CONNECTIONS %ld, TIMEOUT %ld\n", config.max_connections, config.timeout);
    iterator_to_begin(config.users);
    
    while(iterator_has_next(config.users)){
        user_t *user = iterator_next(config.users);
        log(INFO, "USER: %s, PASSWORD: %s\n", user->username, user->password);
    }
        log(INFO, "MAILDIR: %s\n", config.maildir);
        log(INFO, "POP3-PORT: %s\n", config.pop3_port);
        log(INFO, "PEEP-PORT: %s\n", config.peep_port);
}

void free_server_config(server_config config) {
    while(!is_empty(config.users)) {
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
}

void start_metrics() {
    metrics.current_concurrent_pop3_connections = 0;
    metrics.emails_read = 0;
    metrics.emails_removed = 0;
    metrics.successful_quit = 0;
    metrics.sent_bytes = 0;
    metrics.total_pop3_connections = 0;
    metrics.current_loggedin_users = 0;
    metrics.historic_loggedin_users = 0;
}

void add_connection_metric() {
    metrics.total_pop3_connections += 1;
    metrics.current_concurrent_pop3_connections += 1;
}

void remove_connection_metric() {
    metrics.current_concurrent_pop3_connections -= 1;
}

void add_sent_bytes(size_t bytes) {
    metrics.sent_bytes += bytes;
}

void add_email_read() {
    metrics.emails_read += 1;
}
void add_email_removed() {
    metrics.emails_removed += 1;
}

void add_successful_quit() {
    metrics.successful_quit += 1;
}

void add_loggedin_user() {
    metrics.current_loggedin_users += 1;
    if (metrics.current_loggedin_users > metrics.historic_loggedin_users) {
        metrics.historic_loggedin_users = metrics.current_loggedin_users;
    }
}

void remove_loggedin_user() {
    metrics.current_loggedin_users -= 1;
}
