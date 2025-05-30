#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include "queue.h"

//initialise la file à vide
void queue_init(Queue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    pthread_mutex_init(&q->mutex, NULL);
}

//enfile une tâche ne fin de file(FIFO)
void enqueue(Queue *q, Task *t) {
    pthread_mutex_lock(&q->mutex);
    t->next = NULL;
    //file vide
    if (q->tail == NULL){
        q->head = t;
        q->tail = t;
    } else {
        q->tail->next = t;
        q->tail = t;
    }
    q->size++;
    pthread_mutex_unlock(&q->mutex);
}

//Défiler: retourne la tâte, ou NULL si queue vide
Task* dequeue(Queue *q) {
    pthread_mutex_lock(&q->mutex);

    if (q->head == NULL){
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }

    Task *t = q->head;
    q->head = t->next;
    if(q->head == NULL) {
        q->tail = NULL;
    }
    t->next = NULL;
    q->size--;
    pthread_mutex_unlock(&q->mutex);
    return t;
}

//Vérifie si la file est vide
int queue_is_empty(const Queue *q) {
    pthread_mutex_lock((pthread_mutex_t*)&q->mutex);
    int empty = (q->head == NULL);
    pthread_mutex_unlock((pthread_mutex_t*)&q->mutex);
    return empty;
}

//Afficher les tâches de la file
void print_queue(const Queue *q) {
    pthread_mutex_lock((pthread_mutex_t*)&q->mutex);
    printf("===== Contenu de la file (taille=%d) =====\n", q->size);
    Task *cursor = q->head;
    while (cursor) {
        print_task(cursor);
        cursor = cursor->next;
    }
    printf("=======================================\n");
    pthread_mutex_unlock((pthread_mutex_t*)&q->mutex);
}

//Fonction vide pour l'instant
void enqueue_priority(Queue *q, Task *t) {
    pthread_mutex_lock(&q->mutex);
    if (q->head == NULL) {
        //file vide
        t->next = NULL;
        q->head = t;
        q->tail = t;
    } else if (t->priority > q->head->priority) {
        //inserer en tête
        t->next = q->head;
        q->head = t;
    } else {
        Task *curr = q->head;
        while (curr->next != NULL && curr->next->priority >= t->priority) {
            curr = curr->next;
        }
        t->next = curr->next;
        curr->next = t;
        if (t->next == NULL) {
            q->tail = t;
        }
    }
    q->size++;
    pthread_mutex_unlock(&q->mutex);
}

void clear_queue(Queue *q) {
    pthread_mutex_lock(&q->mutex);
    Task *current = q->head;
    while (current != NULL) {
        Task *next = current->next;
        if (current->state != TERMINATED && current->pid > 0) {
            printf("[INFO] ➤ Suppression du fils PID=%d\n", current->pid);
            kill(current->pid, SIGKILL);
        }
        free_task(current);
        current = next;
    }
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    pthread_mutex_unlock(&q->mutex);

    printf("[INFO] File d’attente libérée avec succès.\n");
    pthread_mutex_destroy(&q->mutex); // détruire le mutex
}
