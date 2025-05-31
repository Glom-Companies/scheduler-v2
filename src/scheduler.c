// src/scheduler.c
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
#include <unistd.h>     // pause(), usleep
#include <stdarg.h>     // va_list, va_start, va_end
#include <fcntl.h>      // open
#include <errno.h>

// Déclarer l’externe pour pouvoir réinitialiser
extern int scheduler_running;

#define LOGFILE "/tmp/scheduler.log"

// Flag pour indiquer fin du quantum
static volatile sig_atomic_t alarm_flag = 0;

// Handler pour SIGALRM (préemption)
static void sigalrm_handler(int sig) {
    (void)sig;
    alarm_flag = 1;
}

// Fonction utilitaire : écrit un message formaté dans le fichier de log
static void log_msg(const char *format, ...) {
    va_list args;
    va_start(args, format);
    FILE *f = fopen(LOGFILE, "a");
    if (!f) {
        va_end(args);
        return;
    }
    vfprintf(f, format, args);
    fprintf(f, "\n");
    fclose(f);
    va_end(args);
}

// Aide pour afficher le type de tâche en texte
static const char *get_task_type_str(task_type_t type) {
    switch (type) {
        case TASK_CONV_VIDEO: return "Conversion";
        case TASK_COMPRESS:   return "Compression";
        case TASK_UPDATE:     return "MiseAJour";
        case TASK_CLONE:      return "ClonageGit";
        default:              return "Inconnu";
    }
}

// ====== FIFO ======
static void run_fifo(Queue *q) {
    while (!queue_is_empty(q)) {
        Task *t = dequeue(q);
        if (!t) break;

        pid_t pid = t->pid;
        t->state = RUNNING;
        // Deux sauts de ligne avant la reprise
        log_msg("\n\n[FIFO] Reprise pid=%d (Type=%s, Param=\"%s\")",
                pid,
                get_task_type_str(t->type),
                t->param1 ? t->param1 : "N/A");

        if (kill(pid, SIGCONT) == -1) {
            log_msg("[FIFO][ERREUR] kill SIGCONT pid=%d: %s", pid, strerror(errno));
        }

        int status;
        if (waitpid(pid, &status, 0) == -1) {
            log_msg("[FIFO][ERREUR] waitpid pid=%d: %s", pid, strerror(errno));
        } else {
            if (WIFEXITED(status)) {
                log_msg("[FIFO] pid=%d terminé (exit=%d)",
                        pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                log_msg("[FIFO] pid=%d tué par signal %d",
                        pid, WTERMSIG(status));
            }
        }

        t->state = TERMINATED;
        free_task(t);
    }
    log_msg("[INFO] FIFO terminé.");
}

// ====== Round Robin (RR) ======
static void run_rr(Queue *q, int quantum) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigalrm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        log_msg("[RR][ERREUR] sigaction SIGALRM: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (!queue_is_empty(q)) {
        Task *t = dequeue(q);
        if (!t) break;

        pid_t pid = t->pid;

        if (t->type == TASK_UPDATE || t->type == TASK_CLONE) {
            t->state = RUNNING;
            log_msg("\n\n[RR-NoPreempt] Exécution sans préemption pid=%d (Type=%s, Param=\"%s\")",
                    pid,
                    get_task_type_str(t->type),
                    t->param1 ? t->param1 : "N/A");

            if (kill(pid, SIGCONT) == -1) {
                log_msg("[RR-NoPreempt][ERREUR] kill SIGCONT pid=%d: %s", pid, strerror(errno));
            }

            int status;
            if (waitpid(pid, &status, 0) == -1) {
                log_msg("[RR-NoPreempt][ERREUR] waitpid pid=%d: %s", pid, strerror(errno));
            } else {
                if (WIFEXITED(status)) {
                    log_msg("[RR-NoPreempt] pid=%d terminé (exit=%d)",
                            pid, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    log_msg("[RR-NoPreempt] pid=%d tué par signal %d",
                            pid, WTERMSIG(status));
                }
            }

            t->state = TERMINATED;
            free_task(t);
            continue;
        }

        t->state = RUNNING;
        log_msg("\n\n[RR] Reprise pid=%d (Type=%s, Param=\"%s\")",
                pid,
                get_task_type_str(t->type),
                t->param1 ? t->param1 : "N/A");

        if (kill(pid, SIGCONT) == -1) {
            log_msg("[RR][ERREUR] kill SIGCONT pid=%d: %s", pid, strerror(errno));
        }

        alarm_flag = 0;
        struct itimerval timer;
        timer.it_value.tv_sec = quantum;
        timer.it_value.tv_usec = 0;
        timer.it_interval.tv_sec = 0;
        timer.it_interval.tv_usec = 0;
        if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
            log_msg("[RR][ERREUR] setitimer: %s", strerror(errno));
        }

        int status;
        while (1) {
            pid_t wpid = waitpid(pid, &status, WNOHANG);
            if (wpid == -1) {
                log_msg("[RR][ERREUR] waitpid pid=%d: %s", pid, strerror(errno));
                break;
            }
            if (wpid == pid) {
                if (WIFEXITED(status)) {
                    log_msg("[RR] pid=%d terminé (exit=%d)",
                            pid, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    log_msg("[RR] pid=%d tué par signal %d",
                            pid, WTERMSIG(status));
                }
                t->state = TERMINATED;
                timer.it_value.tv_sec = 0;
                timer.it_value.tv_usec = 0;
                setitimer(ITIMER_REAL, &timer, NULL);
                free_task(t);
                break;
            }
            if (alarm_flag) {
                if (kill(pid, SIGSTOP) == -1) {
                    log_msg("[RR][ERREUR] kill SIGSTOP pid=%d: %s", pid, strerror(errno));
                } else {
                    log_msg("[RR] Quantum écoulé pid=%d, préemption", pid);
                }
                t->state = READY;
                timer.it_value.tv_sec = 0;
                timer.it_value.tv_usec = 0;
                setitimer(ITIMER_REAL, &timer, NULL);
                enqueue(q, t);
                break;
            }
            usleep(10000);
        }
    }
    log_msg("[INFO] Round Robin terminé.");
}

// ====== Ordonnancement PAR PRIORITÉ ======
static void run_priority(Queue *q) {
    while (!queue_is_empty(q)) {
        pthread_mutex_lock(&q->mutex);
        Task *prev_max = NULL;
        Task *max_task = q->head;
        Task *prev = q->head;
        Task *cursor = q->head ? q->head->next : NULL;

        while (cursor != NULL) {
            if (cursor->priority > max_task->priority) {
                max_task = cursor;
                prev_max = prev;
            }
            prev = cursor;
            cursor = cursor->next;
        }

        if (!max_task) {
            pthread_mutex_unlock(&q->mutex);
            break;
        }
        if (prev_max == NULL) {
            q->head = max_task->next;
            if (q->head == NULL) {
                q->tail = NULL;
            }
        } else {
            prev_max->next = max_task->next;
            if (prev_max->next == NULL) {
                q->tail = prev_max;
            }
        }
        max_task->next = NULL;
        q->size--;
        pthread_mutex_unlock(&q->mutex);

        pid_t pid = max_task->pid;
        max_task->state = RUNNING;
        log_msg("\n\n[PR] Reprise pid=%d (Type=%s, Param=\"%s\") – priorité=%d",
                pid,
                get_task_type_str(max_task->type),
                max_task->param1 ? max_task->param1 : "N/A",
                max_task->priority);

        if (kill(pid, SIGCONT) == -1) {
            log_msg("[PR][ERREUR] kill SIGCONT pid=%d: %s", pid, strerror(errno));
        }

        int status;
        if (waitpid(pid, &status, 0) == -1) {
            log_msg("[PR][ERREUR] waitpid pid=%d: %s", pid, strerror(errno));
        } else {
            if (WIFEXITED(status)) {
                log_msg("[PR] pid=%d terminé (exit=%d)",
                        pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                log_msg("[PR] pid=%d tué par signal %d",
                        pid, WTERMSIG(status));
            }
        }

        max_task->state = TERMINATED;
        free_task(max_task);
    }
    log_msg("[INFO] PRIORITY terminé.");
}

// ====== run_scheduler et thread ======
void run_scheduler(algo_t alg, Queue *q, int quantum) {
    if (alg == ALG_FIFO) {
        log_msg("[Scheduler] Algorithme: FIFO");
        run_fifo(q);
    } else if (alg == ALG_RR) {
        log_msg("[Scheduler] Algorithme: Round Robin");
        run_rr(q, quantum);
    } else if (alg == ALG_PRIORITY) {
        log_msg("[Scheduler] Algorithme: Priority");
        run_priority(q);
    } else {
        log_msg("[Scheduler][ERREUR] Algorithme inconnu: %d", alg);
    }
    log_msg("[INFO] Ordonnancement terminé.");
    // À la fin, on remet la variable à 0
    scheduler_running = 0;
}

typedef struct {
    algo_t alg;
    Queue *q;
    int quantum;
} SchedulerArg;

void *scheduler_thread_func(void *arg) {
    SchedulerArg *sarg = (SchedulerArg *)arg;
    run_scheduler(sarg->alg, sarg->q, sarg->quantum);
    free(sarg);
    return NULL;
}

int start_scheduler_thread(algo_t alg, Queue *q, int quantum) {
    pthread_t tid;
    SchedulerArg *sarg = malloc(sizeof(SchedulerArg));
    if (!sarg) {
        perror("[Scheduler] malloc SchedulerArg");
        return -1;
    }
    sarg->alg = alg;
    sarg->q = q;
    sarg->quantum = quantum;

    int res = pthread_create(&tid, NULL, scheduler_thread_func, sarg);
    if (res != 0) {
        fprintf(stderr, "[Scheduler][ERREUR] pthread_create: %s\n", strerror(res));
        free(sarg);
        return -1;
    }
    pthread_detach(tid);
    return 0;
}

