#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "client_utils.h"
#include "peep_client_parser.h"

#define PEEP_PORT_DEFAULT "2110"
#define CRLF "\r\n"



char STDIN_BUFFER[STD_BUFFER_SIZE]={0};

int main(int argc, char* argv[]) {
    
    client_config config = get_client_config(argc, argv);

    int socket = setup_tcp_client_socket(config.values[ADDRESS], config.values[PORT]);
    if (socket < 0) {
		FATAL_ERROR("Setting up socket failed: %d",socket);
	}

    FILE* server = fdopen(socket, "r+");

    fprintf(server, "a %s %s\r\n", config.values[USERNAME], config.values[PASSWORD]);

    char answer[32] = {0};

    fgets(answer, 32, server);

    printf("Connected successfully to server through PEEP!\nSee available commands with 'help'\n");
    command_info* command = NULL;
    while(1){
        fgets(STDIN_BUFFER,STD_BUFFER_SIZE-1, stdin);
        int index_stdin=0;
        while(STDIN_BUFFER[index_stdin]!='\n' && STDIN_BUFFER[index_stdin]!='\0'){
            index_stdin++;
        }
        STDIN_BUFFER[index_stdin]='\0';
        command = parse_user_command(STDIN_BUFFER);
        bool end = 0;
        bool error = 0;
        if(command==NULL)
            continue;
        switch (command->type){
            case QUIT:
                fprintf(server, "q\r\n");
                end = 1;
                break;
            case ADD_USER:
                fprintf(server, "u+ %s %s\r\n", command->str_args[0], command->str_args[1]);
                free(command->str_args[0]);
                free(command->str_args[1]);
                break;
            case DELETE_USER:
                fprintf(server, "u- %s\r\n", command->str_args[0]);
                free(command->str_args[0]);
                break;
            case SHOW_USERS:
                fprintf(server, "u?\r\n");
                break;
            case SET_MAX_CONNECTIONS:   
                fprintf(server, "c= %s\r\n", command->str_args[0]);
                free(command->str_args[0]);
                break;
            case SHOW_MAX_CONNECTIONS:
                fprintf(server, "c?\r\n");
                break;
            case SET_MAILDIR:
                fprintf(server, "m= %s\r\n", command->str_args[0]);
                free(command->str_args[0]);
                break;
            case SHOW_MAILDIR:
                fprintf(server, "m?\r\n");
                break;
            case SET_TIMEOUT:
                fprintf(server, "t= %s\r\n", command->str_args[0]);
                free(command->str_args[0]);
                break;
            case SHOW_TIMEOUT:
                fprintf(server, "t?\r\n");
                break;
            case SHOW_RETRIEVED_BYTES:
                fprintf(server, "rb?\r\n");
                break;
            case SHOW_RETRIEVED_EMAILS_COUNT:
                fprintf(server, "re?\r\n");
                break;
            case SHOW_REMOVED_EMAILS_COUNT:
                fprintf(server, "xe?\r\n");
                break;
            case SHOW_CURR_CONNECTION_COUNT:
                fprintf(server, "cc?\r\n");
                break;
            case SHOW_CURR_LOGGED_IN:
                fprintf(server, "cu?\r\n");
                break;
            case SHOW_HIST_CONNECTION_COUNT:
                fprintf(server, "hc?\r\n");
                break;
            case SHOW_HIST_LOGGED_IN_COUNT:
                fprintf(server, "hu?\r\n");
                break;
            case CAPABILITIES:
                fprintf(server, "cap?\r\n");
                break;
            case HELP:
                printf("capa | quit | maildir | timeout | users | help | retrieved bytes | retrieved emails "
                "| max connections | set maildir <path> | set timeout <value> | delete user <username> " 
                "| current connections amount | current logged users | removed emails amount | add user <username> <password> " 
                "| set max connections <value> | all time connection amount | all time logged amount\r\n");
                free(command);
                continue;
            case UNKNOWN:
            default:
                error = true;
                break;
        }

        if(!error){
            fflush(server);
            if(handle_server_response(server,IS_MULTILINE(command->type)) == -1){
                FATAL_ERROR("Server error: %s",strerror(errno))
            }
        }else{
            printf("Unknown command\n");
        }
        free(command);
        if(end)
            goto end;
    }
    end:
    fclose(server);
}
