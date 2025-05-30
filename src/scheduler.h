#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "queue.h"

//Enumeration des algos
typedef enum {
    ALG_FIFO = 0,
    ALG_RR = 1,
    ALG_PRIORITY = 2
} algo_t;

//Ordonnanceur bloquant
void run_scheduler(algo_t alg, Queue *q, int quantum);

//Ordonnanceur dans un thread séparé non bloquant
int start_scheduler_thread(algo_t alg, Queue *q, int quantum);

#endif // SCHEDULER_H