#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>


#include "parser.h"
#include "logger.h"
#include "command_parser.h"

/* CDT del parser */
struct parser
{

    /** definición de estados */
    struct parser_definition *def; 

    /* estado actual */
    unsigned state;

    /* evento que semak retorna */
    struct parser_event * e1;

    struct parser_event * list;

};

void parser_destroy(struct parser *p)
{
    if (p != NULL)
    {
        for(int i = 0; i < 3; i += 1) {
            free(p->e1->args[i]);
        } 
        free(p->e1);
        free_parser_def(p->def);
        free(p);
    }
}

struct parser *
parser_init(struct parser_definition *def)
{
    struct parser *ret = malloc(sizeof(*ret));
    if (ret != NULL)
    {
        memset(ret, 0, sizeof(*ret));
        ret->def = def;
        ret->state = def->start_state;
        ret->e1 = calloc(1, sizeof(struct parser_event));
        ret->list = NULL;
    }
    return ret;
}

void parser_reset(struct parser *p)
{
    p->state = p->def->start_state;
}

struct parser_event *
parser_feed(struct parser *p, const uint8_t c)
{
    const struct parser_state_transition *state = p->def->states[p->state];
    const size_t n = p->def->states_n[p->state];
    bool matched = false;
    for (unsigned i = 0; i < n; i++)
    {
        const int when = state[i].when;
        if (state[i].when <= 0xFF)
        {
            matched = (c == when);
        }
        else if (state[i].when == (int)ANY)
        {
            matched = true;
        }
        else
        {
            matched = false;
        }

        if (matched)
        {
            if(state[i].act1!=NULL){
                state[i].act1(p->e1, c);
            }
            p->state = state[i].dest;
            
            break;
        }
    }
    return p->e1;
}

struct parser_event * get_event_list(struct parser * p) {
    struct parser_event * aux = p->list;
    p->list = NULL;
    return aux;
}

struct parser_event * get_last_event(struct parser * p) {
    struct parser_event * aux = p->list;
    if(aux != NULL) {
        p->list = aux->next;
    }
    return aux;
}

void finish_event_item(struct parser * p) {
    struct parser_event * aux = p->list;
    if(aux==NULL)
        p->list = p->e1;
    else {
        while (aux->next!=NULL) aux = aux->next;
        aux->next = p->e1;
    }
    p->e1 = calloc(1, sizeof(struct parser_event));
}


void rec_free_event_list(struct parser_event * event) {
    if(event!=NULL) {
        rec_free_event_list(event->next);
        free(event);
    }
}

void free_event_list(struct parser * p) {
    rec_free_event_list(p->list);
}


/*
joined_parser_t join_parsers(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    joined_parser_t final_parser;
    final_parser.n = count;
    final_parser.parsers = malloc(count * sizeof(struct parser *));

    struct parser *currentParser;
    for (int i = 0; i < count; i += 1)
    {
        final_parser.parsers[i] = va_arg(ap, struct parser *);
    }
    return final_parser;
}

struct parser_event *feed_joined_parser(joined_parser_t parsers, const uint8_t c)
{
    struct parser_event *events = malloc(sizeof(struct parser_event) * parsers.n);
    for (int i = 0; i < parsers.n; i += 1)
    {
        events[i] = *parser_feed(parsers.parsers[i], c);
    }
    return events;
}

void destroy_joined_parsers(joined_parser_t parsers)
{
    for (int i = 0; i < parsers.n; i += 1)
    {
        parser_destroy(parsers.parsers[i]);
    }
    free(parsers.parsers);
}

void reset_joined_parsers(joined_parser_t parsers)
{
    for (int i = 0; i < parsers.n; i += 1)
    {
        parser_reset(parsers.parsers[i]);
    }
}
*/
