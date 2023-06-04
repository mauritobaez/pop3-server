#ifndef PARSER_H_00180a6350a1fbe79f133adf0a96eb6685c242b6   // estaría bueno cambiar el texto
#define PARSER_H_00180a6350a1fbe79f133adf0a96eb6685c242b6

/**
 * parser.c -- pequeño motor para parsers/lexers.
 *
 * El usuario describe estados y transiciones.
 * Las transiciones contienen una condición, un estado destino y acciones.
 *
 * El usuario provee al parser con bytes y éste retona eventos que pueden
 * servir para delimitar tokens o accionar directamente.
 */
#include <stdint.h>
#include <stddef.h>

#define STATES_COUNT 5

enum parser_states {
    MAIN_COMMAND,
    FIRST_ARGUMENT,
    SECOND_ARGUMENT,
    ABOUT_TO_FINISH,
    ERROR_STATE
};

/**
 * Evento que retorna el parser.
 * Cada tipo de evento tendrá sus reglas en relación a data.
 */
struct parser_event
{
    char* args[3];
    uint8_t finished;
    uint16_t index;

    /** lista de eventos: si es diferente de null ocurrieron varios eventos */
    struct parser_event *next;
};

/** describe una transición entre estados  */
struct parser_state_transition
{
    /* condición: un caracter o una clase de caracter. Por ej: '\r' */
    int when;
    /** descriptor del estado destino cuando se cumple la condición */
    unsigned dest;
    /** acción 1 que se ejecuta cuando la condición es verdadera. requerida. */
    void (*act1)(struct parser_event *ret, const uint8_t c);

};

/** predicado para utilizar en `when' que retorna siempre true */
static const unsigned ANY = 1 << 9;

/** declaración completa de una máquina de estados */
struct parser_definition
{
    /** cantidad de estados */
    const unsigned states_count;
    /** por cada estado, sus transiciones */
    const struct parser_state_transition **states;
    /** cantidad de estados por transición */
    const size_t *states_n;

    /** estado inicial */
    const unsigned start_state;
};

/**
 * inicializa el parser.
 *
 */
struct parser *
parser_init(const struct parser_definition *def);

/** destruye el parser */
void parser_destroy(struct parser *p);

/** permite resetear el parser al estado inicial */
void parser_reset(struct parser *p);

/**
 * el usuario alimenta el parser con un caracter, y el parser retorna un evento
 * de parsing. Los eventos son reusado entre llamadas por lo que si se desea
 * capturar los datos se debe clonar.
 */
struct parser_event *
parser_feed(struct parser *p, const uint8_t c);

// hay que llamarlo cuando finaliza
void finish_event_item(struct parser * p);

struct parser_event * get_event_list(struct parser * p);

void free_event_list(struct parser * p);

typedef struct joined_parser
{
    struct parser **parsers;
    size_t n;
} joined_parser_t;

joined_parser_t join_parsers(int count, ...);
struct parser_event *feed_joined_parser(joined_parser_t parsers, const uint8_t c);
void destroy_joined_parsers(joined_parser_t parsers);
void reset_joined_parsers(joined_parser_t parsers);

#endif
