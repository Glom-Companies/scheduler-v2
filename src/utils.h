/*#ifndef UTILS_H
#define UTILS_H

#include <signal.h>

extern volatile sig_atomic_t stop_flag;

//Installe le handler pour SIGINT (ctrl - C)
void setup_sigint_handler(void);

//fonction hansler
void sigint_handler(int sig);

#endif // UTILS_H*/