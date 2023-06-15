#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include "parser.h"
#include <ctype.h>
#include <stdbool.h>

// setea la máquina de estados que es el parser para ser utilizado en pop3
// Glosario: <...> = loop, ANY: cualquier character que no esté ya en una arista saliente de ese estado/vértice
// Automata:
//                                         |
//                                         v                                                                
//            -------'\n'----------->  MAIN_COMMAND <ANY>                                               
//            |                     /      |                                      
//            |                    /       |                                   
//            |                   /      SPACE      
//            |                  /         |                                                                  
//            |                 /          v                                            
//  <\r> ABOUT_TO_FINISH <--'\r'----- FIRST_ARGUMENT <ANY>                                             
//            |                 \          |                                           
//            |                  \         |                                           
//           ANY                  \      SPACE  
//            |                    \       |                                                                    
//            V                     \      v                                             
//     <ANY> ERROR <-----SPACE------ SECOND_ARGUMENT  <ANY>                                                
//                                                                                    
                                                                        
struct parser * set_up_parser();



void write_in(struct parser_event *ret, const uint8_t c, size_t arg_number);
void act_write_main(struct parser_event *ret, const uint8_t c);
void act_write_first_arg(struct parser_event *ret, const uint8_t c);
void act_write_second_arg(struct parser_event *ret, const uint8_t c);
void transition(struct parser_event *ret, const uint8_t c);
void finish(struct parser_event *ret, const uint8_t c);


// auto descriptivo :D
char* str_to_upper(char* str);
//Liberar todos los mallocs de la definition
void free_parser_def(struct parser_definition* par_def);

void free_event(struct parser_event *event, bool free_arguments);

#endif