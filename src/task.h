#ifndef TASK_H
#define TASK_H

#include <sys/types.h> // pour pid_t

//Types de tâches

typedef enum {
    TASK_CONV_VIDEO = 0,
    TASK_COMPRESS = 1,
    TASK_UPDATE = 2,
    TASK_CLONE = 3
} task_type_t;

typedef enum {
    READY,
    RUNNING,
    TERMINATED
} task_state_t;

//Structure de description d'une tâche
typedef struct Task {
    int pid; //pid du fils
    int priority; //priorité pour ordonnancement PRIORITY
    task_type_t type; //quel type de tâche (conversion, compression)
    char *param1; //paramètre 1: chemin ou URL
    char *param2; //param2 : chemin de sortie du dossier
    task_state_t state; //etat de la tâche
    struct Task *next; //pour enchainer dan la file
} Task;

//Créer une tâche (les chaînes p1 et p2 sont dupliquées)
Task *create_task(task_type_t type, int priority, const char *p1, const char *p2);

//Libérer une tâche
void free_task(Task *t);

//Afficher la tâche (pour print_queue)
void print_task(const Task *t);

#endif // TASK_H