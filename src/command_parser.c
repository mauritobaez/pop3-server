#include "command_parser.h"
#include "parser.h"
#include <stdio.h>
#include <malloc.h>


void writeIn(struct parser_event *ret, const uint8_t c, size_t argNumber) {
    while(ret->next != NULL) ret = ret->next;
    if(!(ret->index%10))
        ret->args[argNumber] = realloc(ret->args[argNumber], ret->index + 10);
    ret->args[argNumber][ret->index++] = c; 
}

void actWriteMain(struct parser_event *ret, const uint8_t c) {
    writeIn(ret, c, 0);
}

void actWriteFirstArg(struct parser_event *ret, const uint8_t c) {
    writeIn(ret, c, 1);
}

void actWriteSecondArg(struct parser_event *ret, const uint8_t c) {
    writeIn(ret, c, 2);
}

void transition(struct parser_event *ret, const uint8_t c) {
    while(ret->next != NULL) ret = ret->next;
    ret->index = 0;
}

void finish(struct parser_event *ret, const uint8_t c) {
    while(ret->next != NULL) ret = ret->next;
    ret->index = 0;
    ret->finished++;
}


// parser hecho con parser.c
void set_up_parser() {
    size_t states_n[STATES_COUNT];
    const struct parser_state_transition *states[STATES_COUNT];

    states_n[MAIN_COMMAND] = 3;
    struct parser_state_transition main_command[] = {
        {.when = ' ', .dest = FIRST_ARGUMENT, .act1 = transition}, //no recuerdo si hace falta el &
        {.when = '\r', .dest = ABOUT_TO_FINISH, .act1 = NULL},
        {.when = ANY, .dest = MAIN_COMMAND, .act1 = actWriteMain} 
    };
    states[MAIN_COMMAND] = main_command;
 
    states_n[FIRST_ARGUMENT] = 3;
    struct parser_state_transition first_argument[] = {
        {.when = ' ', .dest = SECOND_ARGUMENT, .act1 = transition},
        {.when = '\r', .dest = ABOUT_TO_FINISH, .act1 = NULL},
        {.when = ANY, .dest = FIRST_ARGUMENT, .act1 = actWriteFirstArg} 
    };
    states[FIRST_ARGUMENT] = first_argument;

    states_n[SECOND_ARGUMENT] = 3;
    struct parser_state_transition second_argument[] = {
        {.when = ' ', .dest = ERROR, .act1 = NULL},
        {.when = '\r', .dest = ABOUT_TO_FINISH, .act1 = NULL},
        {.when = ANY, .dest = SECOND_ARGUMENT, .act1 = actWriteSecondArg} 
    };
    states[SECOND_ARGUMENT] = second_argument; 

    states_n[ABOUT_TO_FINISH] = 3;
    struct parser_state_transition about_to_finish[] = {
        {.when = '\n', .dest = MAIN_COMMAND, .act1 = finish},   //pipelining
        {.when = '\r', .dest = ABOUT_TO_FINISH, .act1 = NULL},
        {.when = ANY, .dest = ERROR, .act1 = NULL} 
    };
    states[ABOUT_TO_FINISH] = about_to_finish;

    states_n[ERROR] = 2;
    struct parser_state_transition error[] = {
        {.when = '\r', .dest = ABOUT_TO_FINISH, .act1 = NULL},
        {.when = ANY, .dest = ERROR, .act1 = NULL} /* hay que comer todo lo que siga hasta el \r\n */
    };
    states[ERROR] = error;


    struct parser_definition pars_def = {
        .states_count=STATES_COUNT, // 6 con el error state (ver mural)
        .states = states,
        .states_n = states_n,
        .start_state = MAIN_COMMAND
    };


    struct parser * pars = parser_init(&pars_def);

/*
    char * string = "Hola Como Va?\r\nHola como va?\r\njajajaja\r\n";

    int i = 0;
    while(string[i]) {
        if(parser_feed(pars, string[i++])->finished)
            finish_event_item(pars);
    }

    struct parser_event * event = get_event_list(pars);
    i = 0;
    while(event!=NULL) {
        printf("\n\nCommand number: %d\n", i++);
        printf("Command: %s\nFirst Arg: %s\nSecond Arg: %s\nfinished? %s\n", event->args[0], (event->args[1]!=NULL)? event->args[1] : "NOTHING", (event->args[2]!=NULL)? event->args[2] : "NOTHING", (event->finished)? "Yes" : "Nope");
        event = event->next;
    }
*/
    
}

