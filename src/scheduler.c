#define _POSIX_C_SOURCE 200809L
#include "scheduler.h"
#include "task.h"
#include "queue.h"
#include <sys/wait.h>
#include <signal.h>     // kill, SIGCONT, SIGSTOP, SIGALRM
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>   // setitimer
#include <unistd.h>     // pause()

//flag pour indiquer fin du quantum
static volatile sig_atomic_t alarm_flag = 0;

//handler pour SIGALRM (la préemption)
static void sigalrm_handler(int sig) {
    (void)sig;
    alarm_flag = 1; //indique temps écoulé
}

//Fonction FIFO

static void run_fifo(Queue *q) {
    while (!queue_is_empty(q)) {
        Task *t = dequeue(q); //on récupère la tête de la file first in first out
        pid_t pid = t->pid;
        t->state = RUNNING;
        printf("[FIFO] Reprise du fils pid=%d (\"%s\")\n", pid, t->description);
        

        //a- Réveil du fils endormi avec SIGCONT
        if(kill(pid, SIGCONT) == -1) {
            perror("kill SIGCONT");
            //nettoyage même si erreur
        }
        //b- attendre la fin du fils en bloquant
        int status;
        if(waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
        } else {
            if (WIFEXITED(status)) {
                printf("[FIFO] Tâche pid=%d terminée (exit code = %d)\n", pid, WEXITSTATUS(status));
            } else if ( WIFSIGNALED(status)) {
                printf("[FIFO] Tâche pid=%d tuée par signal %d\n", pid, WTERMSIG(status));

            }
        }

        //c- marquer comme TERMINATED
        t->state = TERMINATED;
        free_task(t);
    }

}

// Fonction Round Robin
static void run_rr(Queue *q, int quantum) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigalrm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction SIGALRM");
        exit(EXIT_FAILURE);
    }
    
    while(!queue_is_empty(q)) {
        //1-recuperation tete de file
        Task *t = dequeue(q);
        pid_t pid = t->pid;
        t->state = RUNNING;
        printf("[RR] Reprise du fils pid=%d (\"%s\")\n", pid, t->description);

        //2- reveil fils
        if(kill(pid, SIGCONT) == -1) {
            perror("kill SIGCONT");
            //O, passe à la suite même en cas de dohiir
        }

        //3-installation alarme pour quantum
        alarm_flag = 0;
        struct itimerval timer;
        timer.it_value.tv_sec = quantum;
        timer.it_value.tv_usec = 0;
        timer.it_interval.tv_sec = 0;
        timer.it_interval.tv_usec = 0;
        if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
            perror("setitimer");
            // Si impossible, on attend jusqu'à la fin du fils
        }

        //4- boucler jusqu'a la fin de la tach ou de l'alame
        int status;
        pid_t wpid;
        while (1) {
            // a) tester si le fils déja fini (WNOHANG)
            wpid = waitpid(pid, &status, WNOHANG);
            if (wpid == -1) {
                perror("waitpid");
                break;
            }
            if (wpid == pid) {
                //fils terminé avant quantum
                if (WIFEXITED(status)) {
                    printf("[RR] Tâche pid=%d terminée (exit code = %d)\n", pid, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    printf("[RR] Tâche pid=%d tuée par signal %d\n", pid, WTERMSIG(status));
                }
                t->state = TERMINATED;
                free_task(t);
                //Annuler l'alarme restante
                timer.it_value.tv_sec = 0;
                timer.it_value.tv_usec = 0;
                setitimer(ITIMER_REAL, &timer, NULL);
                break;
            }
            // b) Si quantum déja expiré (alarm_falg == 1)
            if (alarm_flag) {
                //Preempter la tâche
                if (kill(pid, SIGSTOP) == -1) {
                    perror("kill SIGSTOP (préemption)");
                } else {
                    printf("[RR] Quantum écoulé pour pid=%d, préemption.\n", pid);
                }
                t->state = READY;
                //Annuler l'alarme
                timer.it_value.tv_sec = 0;
                timer.it_value.tv_usec = 0;
                setitimer(ITIMER_REAL, &timer, NULL);
                //réenfiler en fin de file
                enqueue(q, t);
                break;
            }
            //c) faire une petite pause pour éviter les boucles serrées
            usleep(10000);
        }
    }
    printf("[INFO] Round Robin terminé. Revenez au menu.\n");
    fflush(stdout);

}

//Fonction Ordonnancement par priorité
static void run_priority(Queue *q) {
    while(!queue_is_empty(q)) {
        pthread_mutex_lock(&q->mutex);
        // 1- Chercher la tâche a priorité max
        Task *prev_max = NULL; //pointeur vers le prédecesseur
        Task *max_task = q->head; //on suppose temporarily que la head est la max
        Task *prev = q->head; //pointeur pour itérer
        Task *cursor = q->head->next; //on commence à la deuxième

        //parcours pour trouver max priority
        while (cursor != NULL) {
            if(cursor->priority > max_task->priority) {
                max_task = cursor;
                prev_max = prev;
            }
            prev = cursor;
            cursor = cursor->next;
        }

        //2- Retirer max_task de la liste chainée
        if (prev_max == NULL) {
            //max_task est la tête
            q->head = max_task->next;
            if (q->head == NULL) {
                //file devient vide
                q->tail = NULL;
            }
        } else {
            //max_task est en milieu (ou queue)
            prev_max->next = max_task->next;
            if (prev_max->next == NULL) {
                //on aretiré l'ancienne queue
                q->tail = prev_max;
            }
        }
        max_task->next = NULL;
        q->size--;
        pthread_mutex_unlock(&q->mutex);

        //3- executer la tâche sélectionnée (max_task) jusqu'à la fin
        pid_t pid = max_task->pid;
        max_task->state = RUNNING;
        printf("[PR] Reprise du fils pid=%d (\"%s\") – priorité %d\n", pid, max_task->description, max_task->priority);

        //reveil du fils
        if (kill(pid, SIGCONT) == -1) {
            perror("kill SIGCONT (priority)");
        }

        //Attendre la fin du fils (bloquantd)
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid (priority)");
        } else {
            if (WIFEXITED(status)) {
                printf("[PR] Tâche pid=%d terminée (exit code = %d)\n", pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("[PR] Tâche pid=%d tuée par signal %d\n", pid, WTERMSIG(status));
            }
        }
        // 4) Libérer l'objet max_task
        max_task->state = TERMINATED;
        free_task(max_task);
    }
    printf("[INFO] Ordonnancement PRIORITY terminé. Revenez au menu.\n");
    fflush(stdout);
}

// Choix d'Ordonnanceur
void run_scheduler(algo_t alg, Queue *q, int quantum) {
    if (alg == 0) {
        printf("[Scheduler] Algorithme reçu : FIFO\n");
    } else if (alg == 1) {
        printf("[Scheduler] Algorithme reçu : Round Robin RR\n");
    } else {
        printf("[Scheduler] Algorithme reçu : Priority\n");
    }
    
    (void)quantum;
    switch (alg) {
        case 0:
            run_fifo(q);
            break;
        case 1:
            run_rr(q, quantum);
            break;
        case 2:
            run_priority(q);
            break;
        default:
        printf("[Scheduler] Algorithme inconnu.\n");
    }
    printf("[INFO] Ordonnancement terminé. Appuyez sur Entrée pour revenir au menu.\n");
    fflush(stdout);
}

//thread wrapper
typedef struct {
    algo_t alg;
    Queue *q;
    int quantum;
} SchedulerArg;

//fonction exécuté par le thread
void *scheduler_thread_func(void *arg) {
    SchedulerArg *sarg = (SchedulerArg *)arg;
    run_scheduler(sarg->alg, sarg->q, sarg->quantum);
    free(sarg);
    return NULL;
}

//demarre un thread qui exécute un run_scheduler
int start_scheduler_thread(algo_t alg, Queue *q, int quantum) {
    pthread_t tid;
    SchedulerArg *sarg = malloc(sizeof(SchedulerArg));
    if(!sarg){
        perror("malloc SchedulerArg");
        return -1;
    }
    sarg->alg = alg;
    sarg->q = q;
    sarg->quantum = quantum;

    int res = pthread_create(&tid, NULL, scheduler_thread_func, sarg);
    if (res != 0) {
        fprintf(stderr, "Erreur pthread_create: %s\n", strerror(res));
        free(sarg);
        return -1;
    }
    //detacher le thread pour exec independante
    pthread_detach(tid);
    return 0;
}