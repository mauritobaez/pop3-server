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
    struct parser_event *e1;

    struct parser_event *list;
};

void parser_destroy(struct parser *p)
{
    if (p != NULL)
    {
        for (int i = 0; i < 3; i += 1)
        {
            free(p->e1->args[i]);
        }
        free(p->e1);
        free_event_list(p);
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

// void parser_reset(struct parser *p)
// {
//     p->state = p->def->start_state;
// }

struct parser_event *
parser_feed(struct parser *p, const uint8_t c)
{
    const struct parser_state_transition *state = p->def->states[p->state];
    const size_t n = p->def->states_n[p->state];
    bool matched;
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
            if (state[i].act1 != NULL)
            {
                state[i].act1(p->e1, c);
            }
            p->state = state[i].dest;

            break;
        }
    }
    return p->e1;
}

struct parser_event *get_event_list(struct parser *p)
{
    struct parser_event *aux = p->list;
    p->list = NULL;
    return aux;
}

struct parser_event *get_last_event(struct parser *p)
{
    struct parser_event *aux = p->list;
    if (aux != NULL)
    {
        p->list = aux->next;
    }
    return aux;
}

void finish_event_item(struct parser *p)
{
    struct parser_event *aux = p->list;
    if (aux == NULL)
        p->list = p->e1;
    else
    {
        while (aux->next != NULL)
            aux = aux->next;
        aux->next = p->e1;
    }
    p->e1 = calloc(1, sizeof(struct parser_event));
}

void rec_free_event_list(struct parser_event *event)
{
    if (event != NULL)
    {
        rec_free_event_list(event->next);
        free_event(event, true);
    }
}

void free_event_list(struct parser *p)
{
    rec_free_event_list(p->list);
}
