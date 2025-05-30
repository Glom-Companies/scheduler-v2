# Projet : Ordonnanceur de Tâches avec Processus & Threads en C

**Auteur :** Livingstone  
**Date :** Mai 2025

---

## Sommaire

1. [Présentation du projet](#présentation-du-projet)  
2. [Fonctionnalités](#fonctionnalités)  
3. [Prérequis](#prérequis)  
4. [Architecture du code](#architecture-du-code)  
5. [Compiler le projet](#compiler-le-projet)  
6. [Exécuter le programme](#exécuter-le-programme)  
   1. [Menu principal](#menu-principal)  
   2. [Ajouter une tâche](#ajouter-une-tâche)  
   3. [Afficher la file d’attente](#afficher-la-file-dattente)  
   4. [Choisir un algorithme](#choisir-un-algorithme)  
   5. [Lancer l’ordonnanceur](#lancer-lordonnanceur)  
   6. [Quitter](#quitter)  
7. [Algorithmes disponibles](#algorithmes-disponibles)  
   1. [FIFO (First In, First Out)](#fifo-first-in-first-out)  
   2. [Round Robin (RR)](#round-robin-rr)  
   3. [Priorité (Priority)](#priorité-priority)  
8. [Gestion des signaux et threads](#gestion-des-signaux-et-threads)  
9. [Arrêt propre avec Ctrl+C](#arrêt-propre-avec-ctrlc)  
10. [Tests et vérifications](#tests-et-vérifications)  
11. [Annexe : Makefile](#annexe-makefile)

---

## Présentation du projet

Proposer un ordonnanceur de tâches en C sous Linux, où chaque tâche s’exécute dans un processus-fils multithreadé.  
L’ordonnanceur tourne dans un thread séparé pour que l’interface reste toujours réactive.

Trois modes d’ordonnancement :
- **FIFO** : exécuter chaque tâche dans l’ordre d’arrivée, sans préemption.  
- **Round Robin (RR)** : donner à chaque tâche un quantum fixe (2 s), préempter et réenfiler si elle n’est pas terminée.  
- **Priorité (Priority)** : exécuter la tâche de priorité la plus élevée jusqu’à sa fin, puis la suivante, sans préemption.

---

## Fonctionnalités

- Créer dynamiquement des tâches (description + priorité).  
- Lancer chaque tâche dans un processus-fils, stoppé immédiatement (`SIGSTOP`).  
- Simuler le travail interne par deux threads (via `dummy_task`) dans chaque fils.  
- Gérer la file d’attente de façon thread-safe (mutex).  
- Lancer l’ordonnanceur (FIFO, RR ou Priority) dans un thread détaché.  
- Afficher le contenu de la file en temps réel, même pendant l’exécution.  
- Arrêter proprement via Ctrl+C : tuer tous les fils, vider la file, libérer la mémoire.

---

## Prérequis

- Environnement Linux (Ubuntu ou autre distribution POSIX).  
- GCC (>= 4.8) avec support pthread.  
- `make` (facilite la compilation).  

---

## Architecture du code

