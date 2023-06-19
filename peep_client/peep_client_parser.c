#include "peep_client_parser.h"
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

char* str_to_upper(char* str);

client_commands one_word_commands(const char* command) {
    if(strcmp(command, "CAPA")==0) return CAPABILITIES;
    if(strcmp(command, "QUIT")==0) return QUIT;
    if(strcmp(command, "MAILDIR")==0) return SHOW_MAILDIR;
    if(strcmp(command, "TIMEOUT")==0) return  SHOW_TIMEOUT;
    if(strcmp(command, "USERS")==0) return SHOW_USERS;
    return UNKNOWN;
}

client_commands two_word_commands(const char* first_part, const char* second_part) {
    // comparar con retrived-bytes retrived-emails max-connections
    if(strcmp(first_part, "RETREIVE")==0) {
        if(strcmp(second_part, "BYTES")==0) return SHOW_RETRIEVED_BYTES;
        if(strcmp(second_part, "EMAILS")==0) return SHOW_RETRIEVED_EMAILS_COUNT;
    }
    if(strcmp(first_part, "MAX") == 0 && strcmp(second_part, "CONNECTIONS")==0)
        return SHOW_MAX_CONNECTIONS;
    return UNKNOWN;
}

client_commands three_word_commands(const char* first_part, const char* second_part, const char* third_part, command_info* cmd) {
    // comparar con delete-user set-maildir set-timeout removed-emails-amount current-connections-amount current-logged-amount
    if(strcmp(first_part, "SET")==0) {
        if(strcmp(second_part, "MAILDIR")==0) {
            cmd->str_args[0] = malloc(strlen(third_part));
            strcpy(cmd->str_args[0], third_part);
            return SET_MAILDIR;
        } else if(strcmp(second_part, "TIMEOUT")==0) {
            // TODO: error en el atoi
            cmd->num_arg = atoi(third_part);
            return SET_TIMEOUT;
        }
    } else if(strcmp(third_part, "AMOUNT")==0) {
        if(strcmp(first_part, "CURRENT")==0) {
            if(strcmp(second_part, "CONNECTIONS")==0) return SHOW_CURR_CONNECTION_COUNT;
            if(strcmp(second_part, "LOGGED")==0) return SHOW_CURR_LOGGED_IN;
        }
        if(strcmp(first_part, "REMOVED")==0 && strcmp(second_part, "EMAILS")==0)
            return SHOW_REMOVED_EMAILS_COUNT;
    } else if(strcmp(first_part, "DELETE")==0 && strcmp(second_part, "USER") == 0) {
        cmd->str_args[0] = malloc(strlen(third_part));
        strcpy(cmd->str_args[0], third_part);
        return DELETE_USER;
    }

    return UNKNOWN;
}

client_commands four_word_commands(const char* first_part, const char* second_part, const char* third_part, const char* fourth_part, command_info* cmd) {
    // comparar con add-user set-max-connections all-time-connection-amount all-time-logged-amount
    if(strcmp(first_part,"ADD")==0 && strcmp(second_part,"USER")==0) {
        cmd->str_args[0] = malloc(strlen(third_part));
        strcpy(cmd->str_args[0], third_part);
        cmd->str_args[1] = malloc(strlen(fourth_part));
        strcpy(cmd->str_args[1], fourth_part);
        return ADD_USER;
    } else if(strcmp(fourth_part,"AMOUNT")==0 && strcmp(first_part, "ALL")==0 && strcmp(second_part,"TIME")==0) {
        if(strcmp(third_part, "CONNECTION")==0) return SHOW_HIST_CONNECTION_COUNT;
        if(strcmp(third_part, "LOGGED")==0) return SHOW_HIST_LOGGED_IN_COUNT;
    } else if(strcmp(first_part,"SET")==0 && strcmp(second_part,"MAX")==0 && strcmp(third_part, "CONNECTIONS")) {
        cmd->num_arg = atoi(fourth_part);
        return SET_MAX_CONNECTIONS;
    }

    return UNKNOWN;
}

command_info* parse_user_command(char* command) {
    if(command==NULL)
        return NULL;
    
    char* words[MAX_WORDS_COMMAND];
    int index=0;
    words[index++] = strtok(command, " ");
    while((words[index++] = strtok(NULL, " "))!=NULL);
    str_to_upper(words[0]);
    
    command_info* cmd = calloc(1, sizeof(command_info));

    index--;
    if(index == 1) {
        cmd->type = one_word_commands(words[0]);
    } else if(index==2) {
        str_to_upper(words[1]);
        cmd->type = two_word_commands(words[0], words[1]);
    } else if(index==3) {
        cmd->type = three_word_commands(words[0], words[1], words[3],cmd);
    } else {
        cmd->type = four_word_commands(words[0], words[1], words[3], words[4],cmd);
    }

    return cmd;
}
/*
void str_remove_spaces(char* str) {
    int i1 = i2 = 0;
    while(str[i1]) {
        if(str[i1]!=' ' && str[i1]!='\t')
            str[i2++] = str[i1];

        i1++;
    }
    str[i2] = '\0';
}
*/


char* str_to_upper(char* str) {
	char* aux = str;
	while (*aux) {
		*aux = toupper((unsigned char) *aux);
		aux++;
	}
	return str;
}
