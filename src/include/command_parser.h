#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include "parser.h"
#include <ctype.h>
// parser hecho con parser.c
struct parser * set_up_parser();

void write_in(struct parser_event *ret, const uint8_t c, size_t arg_number);

void act_write_main(struct parser_event *ret, const uint8_t c);
void act_write_first_arg(struct parser_event *ret, const uint8_t c);
void act_write_second_arg(struct parser_event *ret, const uint8_t c);
void transition(struct parser_event *ret, const uint8_t c);
void finish(struct parser_event *ret, const uint8_t c);


char* str_to_upper(char* str);

#endif