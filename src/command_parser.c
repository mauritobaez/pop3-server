#include "command_parser.h"
#include "logger.h"
#include <stdio.h>
#include <malloc.h>


void write_in(struct parser_event *ret, const uint8_t c, size_t arg_number) {
   // while(ret->next != NULL) ret = ret->next;
    if(!(ret->index%10))
        ret->args[arg_number] = realloc(ret->args[arg_number], ret->index + 11);
    ret->args[arg_number][ret->index++] = c;
    ret->args[arg_number][ret->index] = '\0';
}

void act_write_main(struct parser_event *ret, const uint8_t c) {
    write_in(ret, c, 0);
}

void act_write_first_arg(struct parser_event *ret, const uint8_t c) {
    write_in(ret, c, 1);
}

void act_write_second_arg(struct parser_event *ret, const uint8_t c) {
    write_in(ret, c, 2);
}

void transition(struct parser_event *ret, const uint8_t c) {
    //while(ret->next != NULL) ret = ret->next;
    ret->index = 0;
}

void finish(struct parser_event *ret, const uint8_t c) {
    //while(ret->next != NULL) ret = ret->next;
    ret->index = 0;
    ret->finished++;
}


// parser hecho con parser.c
struct parser * set_up_parser() {
    size_t *states_n = malloc(sizeof(size_t)*STATES_COUNT);
    struct parser_state_transition **states = malloc(sizeof(struct parser_state_transition *)*STATES_COUNT);

    states_n[MAIN_COMMAND] = 3;
    struct parser_state_transition* main_command = malloc(sizeof(struct parser_state_transition)*3);
    main_command[0].when = ' '; 
    main_command[0].dest = FIRST_ARGUMENT;
    main_command[0].act1 = &transition;
    main_command[1].when = '\r';
    main_command[1].dest = ABOUT_TO_FINISH;
    main_command[1].act1 = NULL;
    main_command[2].when = ANY;
    main_command[2].dest = MAIN_COMMAND;
    main_command[2].act1 = &act_write_main;
    states[MAIN_COMMAND] = main_command;

 
    states_n[FIRST_ARGUMENT] = 3;
    struct parser_state_transition* first_argument = malloc(sizeof(struct parser_state_transition)*3);
    first_argument[0].when = ' ';
    first_argument[0].dest = SECOND_ARGUMENT;
    first_argument[0].act1 = &transition;
    first_argument[1].when = '\r';
    first_argument[1].dest = ABOUT_TO_FINISH;
    first_argument[1].act1 = NULL;
    first_argument[2].when = ANY;
    first_argument[2].dest = FIRST_ARGUMENT;
    first_argument[2].act1 = &act_write_first_arg;
    states[FIRST_ARGUMENT] = first_argument;

    states_n[SECOND_ARGUMENT] = 3;
    struct parser_state_transition* second_argument = malloc(sizeof(struct parser_state_transition)*3);
    second_argument[0].when = ' ';
    second_argument[0].dest = ERROR_STATE;
    second_argument[0].act1 = NULL;
    second_argument[1].when = '\r';
    second_argument[1].dest = ABOUT_TO_FINISH;
    second_argument[1].act1 = NULL;
    second_argument[2].when = ANY;
    second_argument[2].dest = SECOND_ARGUMENT;
    second_argument[2].act1 = &act_write_second_arg;
    states[SECOND_ARGUMENT] = second_argument;

    states_n[ABOUT_TO_FINISH] = 3;
    struct parser_state_transition* about_to_finish = malloc(sizeof(struct parser_state_transition)*3);
    about_to_finish[0].when = '\n';
    about_to_finish[0].dest = MAIN_COMMAND;
    about_to_finish[0].act1 = &finish;
    about_to_finish[1].when = '\r';
    about_to_finish[1].dest = ABOUT_TO_FINISH;
    about_to_finish[1].act1 = NULL;
    about_to_finish[2].when = ANY;
    about_to_finish[2].dest = ERROR_STATE;
    about_to_finish[2].act1 = NULL;
    states[ABOUT_TO_FINISH] = about_to_finish;

    states_n[ERROR_STATE] = 2;
    struct parser_state_transition* error = malloc(sizeof(struct parser_state_transition)*2);
    error[0].when = '\r';
    error[0].dest = ABOUT_TO_FINISH;
    error[0].act1 = NULL;
    error[1].when = ANY;
    error[1].dest = ERROR_STATE;
    error[1].act1 = NULL;
    states[ERROR_STATE] = error;


    struct parser_definition* pars_def = malloc(sizeof(struct parser_definition));
    
    pars_def->states_count = STATES_COUNT;
    pars_def->states = states;
    pars_def->states_n = states_n;
    pars_def->start_state = MAIN_COMMAND;


    return parser_init(pars_def);
}

char* str_to_upper(char* str) {
	char* aux = str;
	while (*aux) {
		*aux = toupper((unsigned char) *aux);
		aux++;
	}
	return str;
}

