#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "tasks_impl.h"

//structure passée a chaque thread pour simuker le travail
typedef struct {
    int thread_id;
    char desc[256];
} ThreadArg;

//fonction exécuté par chaque thread
void *thread_work(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    printf("[Fils %d][Thread %d] Début du travail pour \"%s\"\n", getpid(), targ->thread_id, targ->desc);
    fflush(stdout);
    sleep(1);
    printf("[Fils %d][Thread %d] Fin du travail pour \"%s\"\n", getpid(), targ->thread_id, targ->desc);
    fflush(stdout);
    free(targ);
    return NULL;
}

//fonction provisoire exécutée par le fils
void dummy_task(const char *desc) {
    printf("[Fils %d] Démarrage tâche : %s\n", getpid(), desc);
    fflush(stdout);
    const int NUM_THREADS = 2;
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        ThreadArg *arg = malloc(sizeof(ThreadArg));
        if(!arg) {
            perror("malloc ThreadArg");
            exit(EXIT_FAILURE);
        }
        arg->thread_id = i + 1;
        snprintf(arg->desc, sizeof(arg->desc), "%s", desc);

        if (pthread_create(&threads[i], NULL, thread_work, arg) != 0) {
            perror("pthread_create");
            free(arg); 
            //on ne quitte pas immédiatement pour tenter de joindre ceux créés
        }
    }

    //Attendre la fin de tous les threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("[Fils %d] Tous les threads de \"%s\" sont terminés.\n", getpid(), desc);
}
