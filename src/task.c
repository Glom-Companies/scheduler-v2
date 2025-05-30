#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "task.h"

//creer une nouvelle tâche en mémoire
Task* create_task(int priority, const char *desc) {
    Task *t = (Task*)malloc(sizeof(Task));
    if (!t){
        perror("Erreur malloc create_task");
        exit(EXIT_FAILURE);
    }
    t->pid = -1; //sera fixé après fork
    t->priority = priority;
    strncpy(t->description, desc, 255);
    t->description[255] = '\0';
    t->state = NEW;
    t->next = NULL;
    return t;
}

//Libère la structure d'une tâche
void free_task(Task *t) {
    if (t) free(t);
}

//Affiche une tâche (pour debug).
void print_task(const Task *t) {
    if (!t) return;
    const char *state_str;
    switch (t->state) {
        case NEW:        state_str = "NEW"; break;
        case READY:      state_str = "READY"; break;
        case RUNNING:    state_str = "RUNNING"; break;
        case WAITING:    state_str = "WAITING"; break;
        case TERMINATED: state_str = "TERMINATED"; break;
        default:         state_str = "UNKNOWN"; break;
    }
    printf("Task: PID=%d | Priorité=%d | Etat=%s | Description=\"%s\"\n",
           t->pid, t->priority, state_str, t->description);
}
