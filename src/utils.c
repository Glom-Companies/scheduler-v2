/*#define _POSIX_C_SOURCE 200809L  // 🔧 Active les fonctionnalités POSIX

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

volatile sig_atomic_t stop_flag = 0;

// Handler pour SIGINT
void sigint_handler(int sig) {
    (void)sig;
    stop_flag = 1;
}

// Setup du handler
void setup_sigint_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}
*/