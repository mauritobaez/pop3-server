#include <stdio.h>
#include <stdlib.h>

#include "queue.h"

typedef struct node_t {
    void* data;
    struct node_t* next;
} node_t;

typedef struct queue_cdt{
    node_t* front;
    node_t* rear;
    node_t* current; //Para el iterador
    unsigned int count;
} queue_cdt;

queue_t create_queue() {
    queue_t queue = (queue_t)malloc(sizeof(queue_cdt));
    queue->front = NULL;
    queue->rear = NULL;
    queue->count = 0;
    queue->current = NULL;
    return queue;
}

bool is_empty(queue_t queue) {
    return (queue->front == NULL);
}

void enqueue(queue_t queue, void* elem) {
    node_t* new = (node_t*)malloc(sizeof(node_t));
    new->data = elem;
    new->next = NULL;
    
    if (is_empty(queue)) {
        queue->front = new;
        queue->rear = new;
    } else {
        queue->rear->next = new;
        queue->rear = new;
    }
    
    queue->count++;
}

void* dequeue(queue_t queue) {
    if (is_empty(queue)) {
        return NULL;
    }
    
    node_t* aux = queue->front;
    void* data = aux->data;
    queue->front = queue->front->next;
    
    if (queue->front == NULL)
        queue->rear = NULL;
    
    free(aux);
    queue->count--;
    return data;
}

void push(queue_t queue, void* elem) {
    node_t* new = (node_t*)malloc(sizeof(node_t));
    new->data = elem;
    new->next = NULL;
    
    if (is_empty(queue)) {
        queue->front = new;
        queue->rear = new;
    } else {
        new->next = queue->front;
        queue->front = new;
    }
    
    queue->count++;
}

unsigned int size(queue_t queue) {
    return queue->count;
}

void free_queue(queue_t queue) {
    while (!is_empty(queue))
        dequeue(queue);
    
    free(queue);
}
void iterator_to_begin(queue_t queue){
    queue->current = queue->front;
}
bool iterator_has_next(queue_t queue){
    return queue->current != NULL;
}
void* iterator_next(queue_t queue){
    if(queue->current == NULL){
        return NULL;
    }
    void * aux = queue->current->data;
    queue->current = queue->current->next;
    return aux;
}

