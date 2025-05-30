#define _POSIX_C_SOURCE 200809L
#include "tasks_impl.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>    // execlp, getpid, dup2
#include <fcntl.h>     // open
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define LOGFILE "/tmp/scheduler.log"

// Ouvre le fichier de log en mode append et redirige stdout/stderr vers ce fichier
//Doit être appelé avant d'éxécuter tout execlp() ou system()
static void redirect_output_to_log(void) {
    int fd = open(LOGFILE, O_WRONLY | O_APPEND);
    if (fd == -1) {
        // Si impossible d'ouvrir le log, on sort quand même. Les commandes afficheront dans le terminal principal (moins grave qu'un crash).
        perror("[tasks_impl] Erreur ouverture LOGFILE pour redirection");
        return;
    }
    // Remplace stdout et stderr par fd
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
}
//Fonction de compression
static void task_compress(Task *t) {

    //t->param1 = chemin entrée, t->param2= chemin de sortie (finit par zst)
    redirect_output_to_log();

    char thread_opt[16];
    char level_opt[16];
    //Demande niveau de compression mais mettons par defaut 1 thread et niveau 3
    snprintf(thread_opt, sizeof(thread_opt), "-T%d", 1); // 1 thread
    snprintf(level_opt, sizeof(level_opt), "-%d", 3); // Niveau 3
    
    execlp("zstd", "zstd", thread_opt, level_opt, t->param1, "-o", t->param2, (char *)NULL);
    //Si execlp échoue
    fprintf(stderr, "[tasks_impl] execlp zstd failed: %s\n", strerror(errno));
    _exit(EXIT_FAILURE);
}

//Fonction conversion vidéo->audio
static void task_convert(Task *t) {
    redirect_output_to_log();
    //Extraction audio par FFmpeg en mode VBR
    execlp("ffmpeg", "ffmpeg", "-i", t->param1, "-q:a", "0", "-map", "a", t->param2, (char *)NULL);
    fprintf(stderr, "[tasks_impl] execlp ffmpeg failed: %s\n", strerror(errno));
    _exit(EXIT_FAILURE);
}

//Fonction pour mise à jour système
static void task_update(Task *t) {
    (void)t;
    redirect_output_to_log();
    //Executer "sudo apt update && sudo apt upgrade -y"
    // Sans bloquer sur mdp
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
    redirect_output_to_log();
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