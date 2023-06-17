#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

//Buffer Circular

typedef struct buffer_cdt* buffer_t;

buffer_t buffer_init(const size_t size);

//escribe nbytes en el buffer, lee de char* string. (No corta por un \0, sino que lee nbytes). Avanza el puntero automáticamente.
size_t buffer_write_and_advance(buffer_t buffer, char* string, size_t nbytes);

//lee nbytes del buffer. Deja el string leído en char* string agregando un \0, asume que el espacio ya fue reservado (nbytes+1). No avanza el puntero hasta llamar a buffer_advance_read()
size_t buffer_read(buffer_t buffer, char* string, size_t nbytes);

void buffer_advance_read(buffer_t buffer, size_t nbytes);

void buffer_clean(buffer_t buffer);

size_t buffer_available_space(buffer_t buffer);

size_t buffer_available_chars_count(buffer_t buffer);

void buffer_free(buffer_t buffer);


#endif

