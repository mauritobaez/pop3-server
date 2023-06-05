#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

typedef struct queue_cdt* queue_t;

queue_t create_queue();
bool is_empty(queue_t queue);
void enqueue(queue_t queue, void* elem);
void* dequeue(queue_t queue);
void push(queue_t queue, void* elem);
unsigned int size(queue_t queue);
void free_queue(queue_t queue);

#endif