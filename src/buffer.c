#include "buffer.h"
#include <stdlib.h>
#include <string.h>
#include "logger.h"

struct buffer_cdt
{
    char *memstart;
    size_t max_size;
    char *read_ptr;
    char *write_ptr;
};

buffer_t buffer_init(const size_t size)
{
    buffer_t buffer = (buffer_t)malloc(sizeof(struct buffer_cdt));
    if (buffer != NULL)
    {
        buffer->memstart = (char *)malloc((size + 1) * sizeof(char));
        if (buffer->memstart != NULL)
        {
            buffer->max_size = size;
            buffer_clean(buffer);
        }
        else
        {
            free(buffer);
            buffer = NULL;
        }
    }
    return buffer;
}

//   r
// 0 1 2
//     w

size_t buffer_write_and_advance(buffer_t buffer, char *string, size_t nbytes)
{
    size_t available_size = buffer_available_space(buffer);

    if (available_size == 0 || nbytes == 0)
        return 0;

    size_t space_to_end = buffer->memstart + buffer->max_size - buffer->write_ptr;

    if (nbytes > available_size)
    {
        nbytes = available_size;
    }

    if (nbytes > space_to_end)
    {
        memcpy(buffer->write_ptr, string, space_to_end);
        memcpy(buffer->memstart, string + space_to_end, nbytes - space_to_end);
    }
    else
    {
        memcpy(buffer->write_ptr, string, nbytes);
    }

    buffer->write_ptr += nbytes;
    if (buffer->write_ptr > buffer->memstart + buffer->max_size)
    {
        buffer->write_ptr -= (buffer->max_size + 1);
    }
    return nbytes;
}

size_t buffer_read(buffer_t buffer, char *string, size_t nbytes)
{
    size_t available_chars = buffer_available_chars_count(buffer);
    if (available_chars == 0 || nbytes == 0)
        return 0;

    if (nbytes > available_chars)
    {
        nbytes = available_chars;
    }

    size_t space_to_end = buffer->memstart + buffer->max_size - buffer->read_ptr;
    if (nbytes > space_to_end)
    {
        memcpy(string, buffer->read_ptr, space_to_end);
        memcpy(string + space_to_end, buffer->memstart, nbytes - space_to_end);
    }
    else
    {
        memcpy(string, buffer->read_ptr, nbytes);
    }

    string[nbytes] = '\0';

    return nbytes;
}

void buffer_advance_read(buffer_t buffer, size_t nbytes)
{
    buffer->read_ptr += nbytes;
    if (buffer->read_ptr > buffer->memstart + buffer->max_size)
    {
        buffer->read_ptr -= (buffer->max_size + 1);
    }
}

void buffer_clean(buffer_t buffer)
{
    buffer->read_ptr = buffer->write_ptr = buffer->memstart;
}

// r = 2
// w = 0
size_t buffer_available_space(buffer_t buffer)
{
    ssize_t ptr_diff = buffer->write_ptr - buffer->read_ptr;
    return (ptr_diff >= 0) ? buffer->max_size - (size_t)ptr_diff : (size_t)(-ptr_diff - 1);
}

size_t buffer_available_chars_count(buffer_t buffer)
{
    ssize_t ptr_diff = buffer->write_ptr - buffer->read_ptr;
    return (ptr_diff >= 0) ? (size_t)ptr_diff : (size_t)(buffer->max_size + ptr_diff + 1);
}

void buffer_free(buffer_t buffer)
{
    free(buffer->memstart);
    free(buffer);
}