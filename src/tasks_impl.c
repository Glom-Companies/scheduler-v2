#define _POSIX_C_SOURCE 200809L
#include "tasks_impl.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <pthread.h>

//Fonction de compression
static void task_compress(Task *t) {

    //t->param1 = chemin entrée, t->param2= chemin de sortie (finit par zst)
    printf("[FIls %d] Compression %s -> %s\n", getpid(), t->param1, t->param2);
    //Avec execlp on remplace le process par zstd :
    int nbThreads = 1;
    int niveau = 3;
    //Récupérable depuis t->priority
    char strThreads[16], strNiveau[16];
    snprintf(strThreads, sizeof(strThreads),"-T%d", nbThreads);
    snprintf(strNiveau, sizeof(strNiveau), "-%d", niveau);

    execlp("zstd", "zstd", strThreads, strNiveau, t->param1, "-o", t->param2, (char *)NULL);
    perror("execlp zstd");
    _exit(EXIT_FAILURE);
}

//Fonction conversion vidéo->audio
static void task_convert(Task *t) {
    printf("[Fils %d] Conversion %s → %s\n", getpid(), t->param1, t->param2);

    //Extraction audio par FFmpeg en mode VBR
    execlp("ffmpeg", "ffmpeg", "-i", t->param1, "-q:a", "0", "-map", "a", t->param2, (char *)NULL);
    perror("execlp ffmpeg");
    _exit(EXIT_FAILURE);
}

//Fonction pour mise à jour système
static void task_update(Task *t) {
    (void)t;
    //Executer "sudo apt update && sudo apt upgrade -y"
    // Sans bloquer sur mdp
    printf("[Fils %d] Mise à jour système\n", getpid());
    int code = system("sudo apt update && sudo apt update upgrade -y");
    if (code != 0) {
        fprintf(stderr, "Erreur de mise à jour (code %d)\n", code);
        _exit(EXIT_FAILURE);
    }
    _exit(EXIT_SUCCESS);
}

//Fonction clonage Git
static void task_clone(Task *t) {
    //t->param1 = URL, t->param2 = dossier cible
    printf("[FIls %d] Clonage Git %s -> %s\n", getpid(), t->param1, t->param2);
    execlp("git", "git", "git", "clone", t->param1, t->param2, (char *)NULL);
    perror("execlp git");
    _exit(EXIT_FAILURE);
}

void execute_task(Task *t) {
    switch (t->type) {
        case TASK_COMPRESS:
            task_compress(t);
            break;
        case TASK_CONV_VIDEO:
            task_convert(t);
            break;
        case TASK_UPDATE:
            task_update(t);
            break;
        case TASK_CLONE:
            task_clone(t);
            break;
        default:
            fprintf(stderr, "[Fils %d] Type de tâche inconnu\n", getpid());
            _exit(EXIT_FAILURE);
    }
}