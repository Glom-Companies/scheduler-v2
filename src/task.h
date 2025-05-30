#ifndef TASK_H
#define TASK_H

//States of a task

typedef enum {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} state_t;

//Structure that describes a task
typedef struct Task {
    int pid;
    int priority;
    char description[256];
    state_t state;
    struct Task *next; //To chain tasks in a queue
} Task;

//Functions prototyoes on tasks
Task* create_task(int priority, const char *desc);
void free_task(Task *t);
void print_task(const Task *t);

#endif // TASK_H