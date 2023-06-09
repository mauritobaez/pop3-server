#include <string.h>

#include "server.h"
#include "logger.h"

/*
-u user:pass -u user:pass
-m <maildir>
*/

#define TOTAL_ARGUMENTS 2

int handle_user(int argc, char *arg[], server_config* config);
int handle_mail(int argc, char *arg[], server_config* config);

argument_t arguments[TOTAL_ARGUMENTS] = {
    {.argument = "-u", .handler = handle_user},
    {.argument = "-m", .handler = handle_mail}
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
    user->lock = 1;
    enqueue(config->users, user);
    return 1;
}

int handle_mail(int argc, char *arg[], server_config* config) {
    if (argc == 0) {
        log(FATAL, "No matching property for argument: %s\n", "-u");
    }
    size_t maildir_length = strlen(arg[0]);
    config->maildir = malloc(maildir_length + 1);
    strncpy(config->maildir, arg[0], maildir_length);
    config->maildir[maildir_length] = '\0';
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
    iterator_to_begin(config.users);
    
    while(iterator_has_next(config.users)){
        user_t *user = iterator_next(config.users);
        log(INFO, "USER: %s, PASSWORD: %s\n", user->username, user->password);
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
