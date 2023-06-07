#include <string.h>

#include "server.h"
#include "logger.h"

/*
-u user:pass -u user:pass
*/

#define TOTAL_ARGUMENTS 1

int handle_user(int argc, char *arg[], server_config* config);

argument_t arguments[TOTAL_ARGUMENTS] = {
    {.argument = "-u", .handler = handle_user}
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
    enqueue(config->users, user);
    return 1;
}

server_config get_server_config(int argc, char *argv[]) {
    fprintf(stderr, "arguments: %d\n", argc);
    server_config config = {
        .max_connections = 10000,
        .polling_timeout = 10000,
        .users = create_queue()
    };
    // ignoro nombre de programa
    argv = argv + 1;
    argc -= 1;
    while (argc > 0) {
        bool found = false;
        for (int i = 0; i < TOTAL_ARGUMENTS && !found; i += 1) {
            if ((found = strcmp(argv[0], arguments[i].argument) == 0)) {
                argc -= 1;
                argv += 1;
                int arguments_consumed = arguments[i].handler(argc, argv, &config);
                argc -= arguments_consumed;
                argv += arguments_consumed;
            }
        }
    }
    return config;
}

void print_config(server_config config) {
    log(INFO, "MAX CONNECTIONS %ld, TIMEOUT %ld\n", config.max_connections, config.polling_timeout);
    size_t queue_size = size(config.users);
    for (size_t i = 0; i < queue_size; i += 1) {
        user_t *user = dequeue(config.users);
        log(INFO, "USER: %s, PASSWORD: %s\n", user->username, user->password);
        enqueue(config.users, user);
    }
}

void free_server_config(server_config config) {
    while(!is_empty(config.users)) {
        user_t *user = dequeue(config.users);
        free(user->password);
        free(user->username);
        free(user);
    }
    free(config.users);
}
