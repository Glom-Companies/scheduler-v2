#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "task.h"

//Structure file d'attente
typedef struct Queue {
    Task *head; //tête
    Task *tail; //queue
    int size; //nbr d'elts
    pthread_mutex_t mutex; //mutex protégeant la file
} Queue;

//prototypes pour la file (FIFO)
void queue_init(Queue *q);
void enqueue(Queue *q, Task *t);
Task* dequeue(Queue *q);
int queue_is_empty(const Queue *q);
void print_queue(const Queue *q);


//prototype pour insertion triée par priorité
void enqueue_priority(Queue *q, Task *t);

//Tuer la queue
void clear_queue(Queue *q);

#endif // QUEUE_H