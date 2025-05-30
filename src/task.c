#include "task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//creer une nouvelle tâche en mémoire
Task *create_task(task_type_t type, int priority, const char *p1, const char *p2) {

    Task *t = (Task*)malloc(sizeof(Task));
    if (!t) return NULL;

    t->pid = -1; //sera fixé après fork
    t->priority = priority;
    t->type = type;
    t->state = READY;

    if (p1) {
        t->param1 = strdup(p1);
        if (!t->param1) {
            free(t);
            return NULL;
        }
    } else {
        t->param1 = NULL;
    }

    if (p2) {
        t->param2 = strdup(p2);
        if (!t->param2) {
            if (t->param1) free(t->param1);
            free(t);
            return NULL;
        }
    } else {
        t->param2 = NULL;
    }

    t->next = NULL;
    return t;
}

//Libère la structure d'une tâche
void free_task(Task *t) {
    if (!t) return;
    if (t->param1) free(t->param1);
    if (t->param2) free(t->param2);
    free(t);
}

//Affiche une tâche (pour debug).
void print_task(const Task *t) {
    if (!t) return;
    //Affichage selon le type
    const char *type_str = "";
    switch (t->type) {
        case TASK_CONV_VIDEO: type_str = "Conversion"; break;
        case TASK_COMPRESS: type_str = "Compression"; break;
        case TASK_UPDATE: type_str = "MiseAJour"; break;
        case TASK_CLONE: type_str = "ClonageGit"; break;
        default: type_str = "Inconnu"; break;
        
    }
    const char *state_str = "";
    switch (t->state) {
        case READY:      state_str = "READY"; break;
        case RUNNING:    state_str = "RUNNING"; break;
        case TERMINATED: state_str = "TERMINATED"; break;
        default:         state_str = "INCONNU"; break;
    }

    printf("Task: PID=%d | Type=%s | Prio=%d | Etat=%s | Param1=\"%s\" | Param2=\"%s\"\n",
           t->pid,
           type_str,
           t->priority,
           state_str,
           t->param1 ? t->param1 : "N/A",
           t->param2 ? t->param2 : "N/A");

}
