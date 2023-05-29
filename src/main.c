#include "parser.h"
#include "parser_utils.h"
#include <stdlib.h>

int main(int argc, char *argv[])
{
    struct parser_definition parser_def = parser_utils_strcmpi("foo");
    struct parser_definition parser_def2 = parser_utils_strcmpi("bar");

    struct parser *parser_instance = parser_init(parser_no_classes(), &parser_def);
    struct parser *par2 = parser_init(parser_no_classes(), &parser_def2);
    char *text = "bar";

    joined_parser_t unified = join_parsers(2, parser_instance, par2);

    for (int i = 0; i < 3; i += 1)
    {
        struct parser_event *event = feed_joined_parser(unified, text[i]);

        print_event(&event[0]);
        print_event(&event[1]);
        free(event);
    }

    text = "foo";
    reset_joined_parsers(unified);
    for (int i = 0; i < 3; i += 1)
    {
        struct parser_event *event = feed_joined_parser(unified, text[i]);

        print_event(&event[0]);
        print_event(&event[1]);
        free(event);
    }
    destroy_joined_parsers(unified);
    parser_utils_strcmpi_destroy(&parser_def);
    parser_utils_strcmpi_destroy(&parser_def2);
}