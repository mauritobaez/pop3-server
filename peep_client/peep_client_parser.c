#include "peep_client_parser.h"
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

char *str_to_upper(char *str);
#define SET_ARG(arg, part)          \
    arg = malloc(strlen(part) + 1); \
    strcpy(arg, part);
client_commands one_word_commands(const char *command)
{
    if (strcmp(command, "CAPA") == 0)
        return CAPABILITIES;
    if (strcmp(command, "QUIT") == 0)
        return QUIT;
    if (strcmp(command, "MAILDIR") == 0)
        return SHOW_MAILDIR;
    if (strcmp(command, "TIMEOUT") == 0)
        return SHOW_TIMEOUT;
    if (strcmp(command, "USERS") == 0)
        return SHOW_USERS;
    if (strcmp(command, "HELP") == 0)
        return HELP;
    return UNKNOWN;
}

client_commands two_word_commands(const char *first_part, const char *second_part)
{
    // comparar con retrived-bytes retrived-emails max-connections
    if (strcmp(first_part, "RETRIEVED") == 0)
    {
        if (strcmp(second_part, "BYTES") == 0)
            return SHOW_RETRIEVED_BYTES;
        if (strcmp(second_part, "EMAILS") == 0)
            return SHOW_RETRIEVED_EMAILS_COUNT;
    }
    if (strcmp(first_part, "MAX") == 0 && strcmp(second_part, "CONNECTIONS") == 0)
        return SHOW_MAX_CONNECTIONS;
    return UNKNOWN;
}

client_commands three_word_commands(char *first_part, char *second_part, char *third_part, command_info *cmd)
{
    // comparar con delete-user set-maildir set-timeout removed-emails-amount current-connections-amount current-logged-amount
    if (strcmp(first_part, "SET") == 0)
    {
        if (strcmp(second_part, "MAILDIR") == 0)
        {
            SET_ARG(cmd->str_args[0], third_part);
            return SET_MAILDIR;
        }
        else if (strcmp(second_part, "TIMEOUT") == 0)
        {
            SET_ARG(cmd->str_args[0], third_part);
            return SET_TIMEOUT;
        }
    }

    if (strcmp(first_part, "DELETE") == 0 && strcmp(second_part, "USER") == 0)
    {
        SET_ARG(cmd->str_args[0], third_part);
        return DELETE_USER;
    }

    str_to_upper(third_part);
    if (strcmp(third_part, "AMOUNT") == 0)
    {
        if (strcmp(first_part, "CURRENT") == 0 && strcmp(second_part, "CONNECTIONS") == 0)
            return SHOW_CURR_CONNECTION_COUNT;
        if (strcmp(first_part, "REMOVED") == 0 && strcmp(second_part, "EMAILS") == 0)
            return SHOW_REMOVED_EMAILS_COUNT;
    }

    if (strcmp(first_part, "CURRENT") == 0 && strcmp(second_part, "LOGGED") == 0 && strcmp(third_part, "USERS") == 0)
        return SHOW_CURR_LOGGED_IN;

    return UNKNOWN;
}

client_commands four_word_commands(char *first_part, char *second_part, char *third_part, char *fourth_part, command_info *cmd)
{
    // comparar con add-user set-max-connections all-time-connection-amount all-time-logged-amount
    if (strcmp(first_part, "ADD") == 0 && strcmp(second_part, "USER") == 0)
    {
        SET_ARG(cmd->str_args[0], third_part);
        SET_ARG(cmd->str_args[1], fourth_part);
        return ADD_USER;
    }

    str_to_upper(third_part);

    if (strcmp(first_part, "SET") == 0 && strcmp(second_part, "MAX") == 0 && strcmp(third_part, "CONNECTIONS") == 0)
    {
        SET_ARG(cmd->str_args[0], fourth_part);
        return SET_MAX_CONNECTIONS;
    }

    str_to_upper(fourth_part);

    if (strcmp(fourth_part, "AMOUNT") == 0 && strcmp(first_part, "ALL") == 0 && strcmp(second_part, "TIME") == 0)
    {
        if (strcmp(third_part, "CONNECTION") == 0)
            return SHOW_HIST_CONNECTION_COUNT;
        if (strcmp(third_part, "LOGGED") == 0)
            return SHOW_HIST_LOGGED_IN_COUNT;
    }

    return UNKNOWN;
}

command_info *parse_user_command(char *command)
{
    if (command == NULL)
        return NULL;

    char *words[MAX_WORDS_COMMAND];
    int index = 0;
    char *rest = command;
    words[index++] = strtok_r(rest, " ", &rest);
    // only an enter has been sent
    if (words[0] == NULL)
        return NULL;
    char *strtok_value;

    command_info *cmd = calloc(1, sizeof(command_info));

    while ((strtok_value = strtok_r(rest, " ", &rest)) != NULL)
    {
        if (index >= 4)
        {
            cmd->type = UNKNOWN;
            return cmd;
        }
        words[index++] = strtok_value;
    }

    str_to_upper(words[0]);
    if (index > 1)
        str_to_upper(words[1]);

    if (index == 1)
    {
        cmd->type = one_word_commands(words[0]);
    }
    else if (index == 2)
    {
        cmd->type = two_word_commands(words[0], words[1]);
    }
    else if (index == 3)
    {
        cmd->type = three_word_commands(words[0], words[1], words[2], cmd);
    }
    else
    {
        cmd->type = four_word_commands(words[0], words[1], words[2], words[3], cmd);
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

char *str_to_upper(char *str)
{
    char *aux = str;
    while (*aux)
    {
        *aux = toupper((unsigned char)*aux);
        aux++;
    }
    return str;
}
