#include <sys/types.h>   // pour pid_t
#include <sys/wait.h>    // pour waitpid
#include <unistd.h>      // pour fork, getpid, sleep
#include <signal.h>      // pour SIGSTOP
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>     // pour pthread_t, pthread_create, pthread_detach

#include "task.h"
#include "tasks_impl.h"
#include "queue.h"
#include "scheduler.h"

#define LOGFILE "/tmp/scheduler.log"

///// VARIABLE GLOBALE /////
static Queue q;                  // File d’attente globale protégée par un mutex
static int scheduler_running = 0; // 0 = pas d’ordonnanceur en cours, 1 = en cours

// Handler SIGINT (Ctrl+C) : tue tous les fils et libère la file
void sigint_handler(int sig) {
    (void)sig;
    printf("\n\n[INFO] Interruption (Ctrl+C) détectée. Nettoyage...\n");

    while (!queue_is_empty(&q)) {
        Task *t = dequeue(&q);
        if (!t) break;
        if (t->state != TERMINATED && t->pid > 0) {
            printf("[INFO] ➤ Suppression du fils PID=%d\n", t->pid);
            kill(t->pid, SIGKILL);
        }
        free_task(t);
    }

    printf("[INFO] Mémoire libérée. Fin du programme.\n");
    exit(EXIT_SUCCESS);
}

int main(void) {
    // 1) Installer le handler Ctrl+C
    signal(SIGINT, sigint_handler);

    // 2) Initialiser la file d'attente (avec mutex interne)
    queue_init(&q);

    // 3) Algorithme par défaut = FIFO
    algo_t current_algo = ALG_FIFO;
    int quantum = 2; // quantum de 2 secondes pour RR

    // 4) Boucle principale du menu
    while (1) {
        printf("\n===== Menu Ordonnanceur =====\n");
        printf("1. Ajouter une tâche\n");
        printf("2. Afficher la file d'attente\n");

        if (current_algo == ALG_FIFO) {
            printf("3. Choisir algorithme (actuel = FIFO)\n");
        } else if (current_algo == ALG_RR) {
            printf("3. Choisir algorithme (actuel = Round Robin RR)\n");
        } else {
            printf("3. Choisir algorithme (actuel = PRIORITY)\n");
        }

        printf("4. Lancer l'ordonnanceur\n");
        printf("5. Quitter\n");
        printf("Votre choix > ");

        char line[128];
        if (!fgets(line, sizeof(line), stdin)) {
            // EOF ou erreur de saisie
            break;
        }
        int choice = atoi(line);

        if (choice == 1) {
            // --- 1. Ajouter une tâche prédéfinie ---
            printf("\n===== Menu des tâches prédéfinies =====\n");
            printf("1. Conversion vidéo -> audio\n");
            printf("2. Compression de fichier\n");
            printf("3. Mise à jour du système\n");
            printf("4. Clonage Git\n");
            printf("Votre choix (1–4) > ");

            if (!fgets(line, sizeof(line), stdin)) continue;
            int type_choice = atoi(line);
            if (type_choice < 1 || type_choice > 4) {
                printf("Type invalide, retour au menu principal.\n");
                continue;
            }
            task_type_t chosen_type = (task_type_t)(type_choice - 1);

            // Variables pour les paramètres
            char *p1 = NULL, *p2 = NULL;

            switch (chosen_type) {
                case TASK_CONV_VIDEO:
                    // Demande chemin vidéo
                    printf("Chemin du fichier vidéo à convertir : ");
                    if (!fgets(line, sizeof(line), stdin)) {
                        break;
                    }
                    line[strcspn(line, "\n")] = '\0';
                    p1 = strdup(line);

                    // Demande chemin audio de sortie
                    printf("Chemin du fichier audio de sortie (ex: sortie.mp3) : ");
                    if (!fgets(line, sizeof(line), stdin)) {
                        free(p1);
                        break;
                    }
                    line[strcspn(line, "\n")] = '\0';
                    p2 = strdup(line);
                    break;

                case TASK_COMPRESS:
                    // Demande chemin du fichier/dossier à compresser
                    printf("Chemin du fichier ou dossier à compresser : ");
                    if (!fgets(line, sizeof(line), stdin)) {
                        break;
                    }
                    line[strcspn(line, "\n")] = '\0';
                    p1 = strdup(line);
                    // Génère nom de sortie en ajoutant .zst
                    {
                        char tmp[512];
                        snprintf(tmp, sizeof(tmp), "%s.zst", p1);
                        p2 = strdup(tmp);
                    }
                    break;

                case TASK_UPDATE:
                    // Pas de paramètres à demander
                    break;

                case TASK_CLONE:
                    // Demande URL du dépôt
                    printf("URL du dépôt Git à cloner : ");
                    if (!fgets(line, sizeof(line), stdin)) {
                        break;
                    }
                    line[strcspn(line, "\n")] = '\0';
                    p1 = strdup(line);

                    // Demande dossier de destination
                    printf("Dossier de destination : ");
                    if (!fgets(line, sizeof(line), stdin)) {
                        free(p1);
                        break;
                    }
                    line[strcspn(line, "\n")] = '\0';
                    p2 = strdup(line);
                    break;
            }

            int prio = 0;  // priorité par défaut (modifiable si souhaité)
            Task *t = create_task(chosen_type, prio, p1, p2);
            if (p1) free(p1);
            if (p2) free(p2);
            if (!t) {
                fprintf(stderr, "[Erreur] Échec création de la tâche.\n");
                continue;
            }

            // Fork + SIGSTOP + enqueue
            pid_t pid = fork();
            if (pid < 0) {
                perror("[Erreur] fork échoué");
                free_task(t);
                continue;
            }
            if (pid == 0) {
                // ==== Code exécuté DANS LE FILS ====
                signal(SIGINT, SIG_IGN); // Ignorer Ctrl+C dans le fils
                execute_task(t);
                _exit(0);
            } else {
                // ==== Code exécuté DANS LE PARENT ====
                t->pid = pid;
                t->state = READY;
                // Stopper le fils immédiatement
                kill(pid, SIGSTOP);

                // Enfiler la tâche (FIFO ou PRIORITY)
                if (current_algo == ALG_PRIORITY) {
                    enqueue_priority(&q, t);
                } else {
                    enqueue(&q, t);
                }
                printf("[Info] Tâche ajoutée : Type=%d, PID=%d\n", chosen_type, pid);
            }

        } else if (choice == 2) {
            // --- 2. Afficher la file d'attente ---
            print_queue(&q);

        } else if (choice == 3) {
            // --- 3. Choisir l'algorithme ---
            printf("Choisir algorithme : 0=FIFO, 1=RR, 2=PRIORITY > ");
            if (!fgets(line, sizeof(line), stdin)) continue;
            int a = atoi(line);
            if (a >= 0 && a <= 2) {
                current_algo = (algo_t)a;
                if (a == 0) {
                    printf("Algorithme changé en FIFO\n");
                } else if (a == 1) {
                    printf("Algorithme changé en Round Robin RR\n");
                } else {
                    printf("Algorithme changé en PRIORITY\n");
                }
            } else {
                printf("Choix invalide\n");
            }

        } else if (choice == 4) {
            // --- 4. Lancer l'ordonnanceur ---
            if (queue_is_empty(&q)) {
                printf("La file est vide, rien à ordonnancer.\n");
            } else {
                if (scheduler_running) {
                    printf("Un ordonnanceur est déjà en cours d'exécution.\n");
                } else {
                    // 1) Vider le fichier de log
                    FILE *f = fopen(LOGFILE, "w");
                    if (f) {
                        fclose(f);
                    } else {
                        perror("[Erreur] Ouverture fichier log");
                    }

                    // 2) Lancer xterm (ou gnome-terminal) pour suivre le log
                    pid_t child = fork();
                    if (child < 0) {
                        perror("[Erreur] fork pour xterm");
                    } else if (child == 0) {
                        // Dans l'enfant : lance xterm et tail le log
                        execlp("xterm", "xterm",
                               "-T", "Watcher Ordonnanceur",
                               "-e", "bash -c \"tail -f /tmp/scheduler.log; echo 'Fin du log. Appuyez sur Entrée pour fermer.'; read\"",
                               (char *)NULL);
                        perror("[Erreur] execlp xterm");
                        _exit(EXIT_FAILURE);
                    } else {
                        // Dans le parent, on laisse xterm s'ouvrir
                        printf("[Info] Fenêtre de log ouverte (PID=%d)\n", child);
                    }

                    // 3) Démarrer le scheduler dans un thread
                    scheduler_running = 1;
                    if (start_scheduler_thread(current_algo, &q, quantum) == -1) {
                        printf("[ERREUR] Impossible de démarrer l’ordonnanceur.\n");
                        scheduler_running = 0;
                    } else {
                        printf("[Info] Ordonnancement lancé (logs dans fenêtre dédiée).\n");
                        printf("[Info] Retour au menu principal.\n");
                    }
                }
            }

        } else if (choice == 5) {
            // --- 5. Quitter ---
            printf("Quitte…\n");
            sleep(1);
            break;

        } else {
            // Choix invalide
            printf("Choix invalide, réessayez.\n");
        }
    }

    // Nettoyage final (si on arrive ici)
    while (!queue_is_empty(&q)) {
        Task *t = dequeue(&q);
        if (!t) break;
        if (t->state != TERMINATED && t->pid > 0) {
            kill(t->pid, SIGKILL);
        }
        free_task(t);
    }

    return 0;
}
