#include <sys/types.h>   // pour pid_t
#include <sys/wait.h>    // pour waitpid si besoin
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

///// VARIABLE GLOBALE ///// 
Queue q; // File d’attente globale protégée par un mutex

// Handler SIGINT (Ctrl+C) qui tue les fils restants et libère la file
void sigint_handler(int sig) {
    (void)sig;
    printf("\n\n[INFO] Interruption (Ctrl+C) détectée. Nettoyage...\n");

    // Tant que la file n'est pas vide, on défiler et on tue les fils si nécessaire
    while (!queue_is_empty(&q)) {
        Task *t = dequeue(&q); 
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
            // --- 1. Ajouter une tâche ---
            printf("Entrez la description de la tâche : ");
            char desc[256];
            if (!fgets(desc, sizeof(desc), stdin)) continue;
            desc[strcspn(desc, "\n")] = '\0';

            printf("Entrez priorité (entier) : ");
            if (!fgets(line, sizeof(line), stdin)) continue;
            int prio = atoi(line);

            // Créer l'objet Task
            Task *t = create_task(prio, desc);

            // Fork pour créer le processus fils
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork échoué");
                free_task(t);
                continue; 
            }

            if (pid == 0) {
                // ---- Code exécuté DANS LE FILS ----
                signal(SIGINT, SIG_IGN);   // Ignorer Ctrl+C dans le fils

                // Exécuter la tâche (multithreadée)
                dummy_task(desc);

                _exit(0); // Terminer le fils proprement
            } else {
                // ---- Code exécuté DANS LE PARENT ----
                t->pid = pid;
                t->state = READY;

                // Stopper le fils immédiatement (pour contrôle via SIGCONT dans le scheduler)
                kill(pid, SIGSTOP);

                // Enfiler la tâche (FIFO ou PRIORITY)
                if (current_algo == ALG_PRIORITY) {
                    enqueue_priority(&q, t);
                } else {
                    enqueue(&q, t);
                }

                printf("Tâche ajoutée (fork & STOP) : \"%s\" (prio=%d, pid=%d)\n",
                       desc, prio, pid);
            }

        } else if (choice == 2) {
            // --- 2. Afficher la file d'attente ---
            print_queue(&q);

        } else if (choice == 3) {
            // --- 3. Choisir l'algorithme ---
            printf("Choisir algorithme: 0=FIFO, 1=RR, 2=PRIORITY : ");
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
                printf("Ordonnancement en cours avec algorithme = %d (dans un thread)\n",
                    current_algo);
                if (start_scheduler_thread(current_algo, &q, quantum) == -1) {
                    printf("[ERREUR] Impossible de démarrer l’ordonnanceur.\n");
                } else {
                    printf("Appuyez sur Entrée pour revenir au menu...\n");
                    char wait[4];
                    if (fgets(wait, sizeof(wait), stdin) == NULL) {
                        perror("Erreur de lecture avec fgets");
                        // Gérer l'erreur si nécessaire
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

    // Nettoyage final (lorsque l'utilisateur choisit 5 ou EOF)
    while (!queue_is_empty(&q)) {
        Task *t = dequeue(&q);
        // Si un fils est toujours en attente, on le tue
        if (t->state != TERMINATED && t->pid > 0) {
            kill(t->pid, SIGKILL);
        }
        free_task(t);
    }

    return 0;
}